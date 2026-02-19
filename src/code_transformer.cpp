#include "code_transformer.h"
#include <clang/AST/ASTContext.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Basic/SourceManager.h>
#include <clang/AST/Expr.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/StringRef.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace move_optimizer {

CodeTransformer::CodeTransformer(clang::ASTContext& context, 
                                  clang::Rewriter& rewriter)
    : context_(context), rewriter_(rewriter), insertedMoveInFile_(false), utilityHeaderEnsured_(false) {
}

bool CodeTransformer::applyTransformation(const Transformation& transformation) {
    // Validate transformation before applying
    if (!validateTransformation(transformation)) {
        return false;
    }
    
    // Check if already moved
    if (isAlreadyMoved(transformation.range)) {
        return true; // Already optimized
    }
    
    bool success = false;
    switch (transformation.type) {
        case Transformation::RETURN_VALUE_MOVE:
        case Transformation::FUNCTION_ARG_MOVE:
        case Transformation::VARIABLE_ASSIGNMENT_MOVE:
        case Transformation::CONSTRUCTOR_INIT_MOVE:
            success = wrapWithMove(transformation.range);
            break;
        default:
            return false;
    }
    
    if (success) {
        appliedRanges_.push_back(transformation.range);
    }
    
    return success;
}

bool CodeTransformer::applyTransformations(
    const std::vector<Transformation>& transformations) {
    bool success = true;
    insertedMoveInFile_ = false;
    
    // Apply transformations in reverse order to preserve source locations
    for (auto it = transformations.rbegin(); it != transformations.rend(); ++it) {
        if (!applyTransformation(*it)) {
            success = false;
        }
    }

    if (insertedMoveInFile_ && !ensureUtilityHeader()) {
        success = false;
    }
    
    return success;
}

std::string CodeTransformer::getTransformedCode() const {
    const clang::RewriteBuffer* buffer = rewriter_.getRewriteBufferFor(
        context_.getSourceManager().getMainFileID());
    
    if (!buffer) {
        return "";
    }
    
    return std::string(buffer->begin(), buffer->end());
}

bool CodeTransformer::insertMove(clang::SourceLocation loc, 
                                  clang::SourceRange range) {
    // Insert std::move at the specified location
    std::string moveCode = "std::move(";
    rewriter_.InsertTextBefore(loc, moveCode);
    
    // Find the end of the range and insert closing parenthesis
    clang::SourceLocation end = range.getEnd();
    rewriter_.InsertTextAfterToken(end, ")");
    
    return true;
}

bool CodeTransformer::wrapWithMove(clang::SourceRange range) {
    clang::SourceManager& sm = context_.getSourceManager();
    const auto& langOpts = context_.getLangOpts();

    if (!range.isValid()) {
        return false;
    }

    clang::SourceLocation begin = range.getBegin();
    clang::SourceLocation end = range.getEnd();
    std::string originalText = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(begin, end), sm, langOpts).str();
    
    // Check if already wrapped with std::move
    if (originalText.find("std::move(") == 0) {
        return true; // Already optimized
    }
    
    // Wrap with std::move
    std::string moveCode = "std::move(";
    rewriter_.InsertTextBefore(begin, moveCode);
    rewriter_.InsertTextAfterToken(end, ")");
    insertedMoveInFile_ = true;
    
    return true;
}

std::string CodeTransformer::generateMoveCode(const Transformation& transformation) {
    std::ostringstream oss;
    
    switch (transformation.type) {
        case Transformation::RETURN_VALUE_MOVE:
        case Transformation::FUNCTION_ARG_MOVE:
        case Transformation::VARIABLE_ASSIGNMENT_MOVE:
        case Transformation::CONSTRUCTOR_INIT_MOVE:
            oss << "std::move(" << transformation.originalCode << ")";
            break;
        default:
            oss << transformation.originalCode;
    }
    
    return oss.str();
}

bool CodeTransformer::validateTransformation(const Transformation& transformation) {
    if (!transformation.range.isValid()) {
        return false;
    }
    
    clang::SourceManager& sm = context_.getSourceManager();
    
    // Check if range is in the main file
    clang::FileID fileID = sm.getFileID(transformation.range.getBegin());
    if (fileID != sm.getMainFileID()) {
        return false; // Only transform main file
    }
    
    // Check for overlap with already applied transformations
    if (checkOverlap(transformation.range)) {
        return false;
    }
    
    return true;
}

bool CodeTransformer::isAlreadyMoved(clang::SourceRange range) {
    clang::SourceManager& sm = context_.getSourceManager();
    const auto& langOpts = context_.getLangOpts();

    if (!range.isValid()) {
        return false;
    }

    std::string text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(range), sm, langOpts).str();
    
    // Check if already wrapped with std::move
    size_t pos = text.find("std::move(");
    if (pos != std::string::npos) {
        // Check if it's at the beginning (not nested)
        std::string before = text.substr(0, pos);
        // Remove whitespace
        before.erase(std::remove_if(before.begin(), before.end(), ::isspace), before.end());
        if (before.empty()) {
            return true;
        }
    }
    
    return false;
}

bool CodeTransformer::checkOverlap(clang::SourceRange range) {
    for (const auto& appliedRange : appliedRanges_) {
        clang::SourceManager& sm = context_.getSourceManager();
        
        // Check if ranges overlap
        if (sm.isBeforeInTranslationUnit(range.getEnd(), appliedRange.getBegin()) ||
            sm.isBeforeInTranslationUnit(appliedRange.getEnd(), range.getBegin())) {
            continue; // No overlap
        }
        
        return true; // Overlap detected
    }
    
    return false;
}

bool CodeTransformer::isValidMoveTarget(clang::Expr* expr) {
    if (!expr) {
        return false;
    }
    
    // Check if expression is an lvalue
    if (!expr->isLValue()) {
        return false; // Already an rvalue, no need to move
    }
    
    // Check if type is const
    clang::QualType type = expr->getType();
    if (type.isConstQualified()) {
        return false; // Cannot move const objects
    }
    
    return true;
}

bool CodeTransformer::ensureUtilityHeader() {
    if (utilityHeaderEnsured_) {
        return true;
    }

    clang::SourceManager& sm = context_.getSourceManager();
    clang::FileID mainFileId = sm.getMainFileID();
    llvm::StringRef buffer = sm.getBufferData(mainFileId);
    if (buffer.empty()) {
        return false;
    }

    if (buffer.contains("#include <utility>") || buffer.contains("#include \"utility\"")) {
        utilityHeaderEnsured_ = true;
        return true;
    }

    size_t offset = 0;
    size_t insertOffset = 0;
    bool hasIncludes = false;

    while (offset < buffer.size()) {
        size_t lineEnd = buffer.find('\n', offset);
        if (lineEnd == llvm::StringRef::npos) {
            lineEnd = buffer.size();
        } else {
            ++lineEnd;
        }

        llvm::StringRef line = buffer.slice(offset, lineEnd).trim();
        if (line.startswith("#include")) {
            hasIncludes = true;
            insertOffset = lineEnd;
            offset = lineEnd;
            continue;
        }

        if (line.empty() || line.startswith("//") || line.startswith("/*") || line.startswith("*")) {
            offset = lineEnd;
            continue;
        }

        if (line.startswith("#pragma once")) {
            insertOffset = lineEnd;
            offset = lineEnd;
            continue;
        }

        if (hasIncludes) {
            break;
        }

        offset = lineEnd;
    }

    clang::SourceLocation insertLoc = sm.getLocForStartOfFile(mainFileId).getLocWithOffset(insertOffset);
    const char* includeText = hasIncludes ? "#include <utility>\n" : "#include <utility>\n\n";
    rewriter_.InsertTextBefore(insertLoc, includeText);
    utilityHeaderEnsured_ = true;
    return true;
}

} // namespace move_optimizer
