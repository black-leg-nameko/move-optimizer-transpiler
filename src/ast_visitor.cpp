#include "ast_visitor.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <queue>
#include <unordered_set>

namespace move_optimizer {

ASTVisitor::ASTVisitor(clang::ASTContext& context)
    : context_(context), currentFunction_(nullptr) {
}

bool ASTVisitor::VisitFunctionDecl(clang::FunctionDecl* decl) {
    if (!decl || !decl->hasBody() || !decl->isThisDeclarationADefinition()) {
        return true;
    }

    currentFunction_ = decl;
    collectUsesForCurrentFunction();
    return true;
}

bool ASTVisitor::VisitCallExpr(clang::CallExpr* expr) {
    if (!expr) {
        return true;
    }

    for (unsigned i = 0; i < expr->getNumArgs(); ++i) {
        clang::Expr* arg = ignoreImplicit(expr->getArg(i));
        if (isCopyOperation(arg) && isSafeToMove(arg, expr)) {
            transformations_.emplace_back(
                Transformation::FUNCTION_ARG_MOVE,
                expr->getLocation(),
                arg->getSourceRange()
            );
        }
    }

    return true;
}

bool ASTVisitor::VisitReturnStmt(clang::ReturnStmt* stmt) {
    if (!stmt || !stmt->getRetValue()) {
        return true;
    }

    clang::Expr* retValue = ignoreImplicit(stmt->getRetValue());
    const auto* declRef = clang::dyn_cast<clang::DeclRefExpr>(retValue);
    if (!declRef) {
        return true;
    }

    // Keep return optimization conservative: only by-value params.
    const auto* param = clang::dyn_cast<clang::ParmVarDecl>(declRef->getDecl());
    if (!param) {
        return true;
    }

    if (isCopyOperation(retValue) && isSafeToMove(retValue, stmt)) {
        transformations_.emplace_back(
            Transformation::RETURN_VALUE_MOVE,
            stmt->getReturnLoc(),
            retValue->getSourceRange()
        );
    }

    return true;
}

bool ASTVisitor::isCopyOperation(clang::Expr* expr) {
    expr = ignoreImplicit(expr);
    if (!expr) {
        return false;
    }

    if (auto* construct = clang::dyn_cast<clang::CXXConstructExpr>(expr)) {
        return construct->getConstructor()->isCopyConstructor();
    }

    if (auto* declRef = clang::dyn_cast<clang::DeclRefExpr>(expr)) {
        if (const auto* var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
            clang::QualType type = var->getType().getNonReferenceType();
            return type->isRecordType();
        }
    }

    return false;
}

bool ASTVisitor::hasMoveConstructor(clang::QualType type) {
    type = type.getNonReferenceType();
    if (!type->isRecordType()) {
        return false;
    }

    const auto* record = type->getAsCXXRecordDecl();
    if (!record) {
        return false;
    }

    for (clang::CXXConstructorDecl* ctor : record->ctors()) {
        if (ctor->isMoveConstructor()) {
            return true;
        }
    }

    return false;
}

bool ASTVisitor::isSafeToMove(clang::Expr* expr, const clang::Stmt* context) {
    expr = ignoreImplicit(expr);
    if (!expr || !context) {
        return false;
    }

    if (!expr->isLValue()) {
        return false;
    }

    auto* declRef = clang::dyn_cast<clang::DeclRefExpr>(expr);
    if (!declRef) {
        return false;
    }

    const auto* var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl());
    if (!var) {
        return false;
    }

    // Do not move globals/statics. We only reason about local state here.
    if (!clang::isa<clang::ParmVarDecl>(var) && !var->hasLocalStorage()) {
        return false;
    }

    clang::QualType type = expr->getType().getNonReferenceType();
    if (!type->isRecordType() || type.isConstQualified()) {
        return false;
    }

    if (!hasMoveConstructor(type)) {
        return false;
    }

    if (clang::isa<clang::ReturnStmt>(context)) {
        return true;
    }

    if (clang::isa<clang::CallExpr>(context)) {
        const auto& sm = context_.getSourceManager();
        return isLastUseInCurrentFunction(var, sm.getExpansionLoc(expr->getBeginLoc()));
    }

    return false;
}

bool ASTVisitor::isLastUseInCurrentFunction(const clang::VarDecl* var,
                                            clang::SourceLocation useLoc) const {
    if (!var || !currentFunctionCfg_ || !useLoc.isValid()) {
        return false;
    }

    const UsePosition* current = findUsePosition(var, useLoc);
    if (!current) {
        return false;
    }

    auto usesIt = variableUsePositions_.find(var);
    if (usesIt == variableUsePositions_.end()) {
        return false;
    }

    for (const UsePosition& candidate : usesIt->second) {
        if (candidate.location == current->location &&
            candidate.blockId == current->blockId &&
            candidate.elementIndex == current->elementIndex) {
            continue;
        }
        if (canOccurAfter(*current, candidate)) {
            return false;
        }
    }

    return true;
}

void ASTVisitor::collectUsesForCurrentFunction() {
    variableUsePositions_.clear();
    cfgBlocksById_.clear();
    currentFunctionCfg_.reset();

    if (!currentFunction_ || !currentFunction_->hasBody()) {
        return;
    }

    clang::CFG::BuildOptions options;
    options.AddImplicitDtors = true;
    options.AddTemporaryDtors = true;
    options.AddInitializers = true;
    currentFunctionCfg_ = clang::CFG::buildCFG(
        currentFunction_, currentFunction_->getBody(), &context_, options);
    if (!currentFunctionCfg_) {
        return;
    }

    class DeclRefCollector : public clang::RecursiveASTVisitor<DeclRefCollector> {
    public:
        DeclRefCollector(clang::SourceManager& sourceManager,
                         std::vector<std::pair<const clang::VarDecl*, clang::SourceLocation>>& out)
            : sourceManager_(sourceManager), out_(out) {
        }

        bool VisitDeclRefExpr(clang::DeclRefExpr* expr) {
            if (!expr || !expr->getLocation().isValid()) {
                return true;
            }
            const auto* var = clang::dyn_cast<clang::VarDecl>(expr->getDecl());
            if (!var) {
                return true;
            }
            out_.push_back({var, sourceManager_.getExpansionLoc(expr->getLocation())});
            return true;
        }

    private:
        clang::SourceManager& sourceManager_;
        std::vector<std::pair<const clang::VarDecl*, clang::SourceLocation>>& out_;
    };

    clang::SourceManager& sm = context_.getSourceManager();
    for (const clang::CFGBlock* block : *currentFunctionCfg_) {
        if (!block) {
            continue;
        }
        cfgBlocksById_[block->getBlockID()] = block;

        unsigned elementIndex = 0;
        for (const clang::CFGElement& element : *block) {
            auto cfgStmt = element.getAs<clang::CFGStmt>();
            if (!cfgStmt) {
                ++elementIndex;
                continue;
            }

            const clang::Stmt* stmt = cfgStmt->getStmt();
            if (!stmt) {
                ++elementIndex;
                continue;
            }

            std::vector<std::pair<const clang::VarDecl*, clang::SourceLocation>> refs;
            DeclRefCollector collector(sm, refs);
            collector.TraverseStmt(const_cast<clang::Stmt*>(stmt));

            for (const auto& [var, location] : refs) {
                variableUsePositions_[var].push_back({block->getBlockID(), elementIndex, location});
            }

            ++elementIndex;
        }
    }
}

bool ASTVisitor::canOccurAfter(const UsePosition& current, const UsePosition& candidate) const {
    if (!currentFunctionCfg_) {
        return false;
    }

    const auto fromIt = cfgBlocksById_.find(current.blockId);
    const auto toIt = cfgBlocksById_.find(candidate.blockId);
    if (fromIt == cfgBlocksById_.end() || toIt == cfgBlocksById_.end()) {
        return false;
    }

    if (current.blockId == candidate.blockId) {
        if (candidate.elementIndex > current.elementIndex) {
            return true;
        }
        return blockCanReachItself(fromIt->second);
    }

    return isReachable(fromIt->second, toIt->second);
}

bool ASTVisitor::isReachable(const clang::CFGBlock* from, const clang::CFGBlock* to) const {
    if (!from || !to) {
        return false;
    }
    if (from == to) {
        return true;
    }

    std::queue<const clang::CFGBlock*> queue;
    std::unordered_set<unsigned> visited;
    visited.insert(from->getBlockID());
    queue.push(from);

    while (!queue.empty()) {
        const clang::CFGBlock* current = queue.front();
        queue.pop();

        for (const clang::CFGBlock::AdjacentBlock succ : current->succs()) {
            const clang::CFGBlock* succBlock = succ.getReachableBlock();
            if (!succBlock) {
                continue;
            }
            if (succBlock == to) {
                return true;
            }
            if (visited.insert(succBlock->getBlockID()).second) {
                queue.push(succBlock);
            }
        }
    }

    return false;
}

bool ASTVisitor::blockCanReachItself(const clang::CFGBlock* block) const {
    if (!block) {
        return false;
    }

    std::queue<const clang::CFGBlock*> queue;
    std::unordered_set<unsigned> visited;

    for (const clang::CFGBlock::AdjacentBlock succ : block->succs()) {
        const clang::CFGBlock* succBlock = succ.getReachableBlock();
        if (!succBlock) {
            continue;
        }
        if (succBlock == block) {
            return true;
        }
        if (visited.insert(succBlock->getBlockID()).second) {
            queue.push(succBlock);
        }
    }

    while (!queue.empty()) {
        const clang::CFGBlock* current = queue.front();
        queue.pop();

        for (const clang::CFGBlock::AdjacentBlock succ : current->succs()) {
            const clang::CFGBlock* succBlock = succ.getReachableBlock();
            if (!succBlock) {
                continue;
            }
            if (succBlock == block) {
                return true;
            }
            if (visited.insert(succBlock->getBlockID()).second) {
                queue.push(succBlock);
            }
        }
    }

    return false;
}

const ASTVisitor::UsePosition* ASTVisitor::findUsePosition(const clang::VarDecl* var,
                                                           clang::SourceLocation useLoc) const {
    auto usesIt = variableUsePositions_.find(var);
    if (usesIt == variableUsePositions_.end()) {
        return nullptr;
    }

    const auto& sm = context_.getSourceManager();
    useLoc = sm.getExpansionLoc(useLoc);
    for (const UsePosition& use : usesIt->second) {
        if (sm.isWrittenInSameFile(use.location, useLoc) &&
            sm.getFileOffset(use.location) == sm.getFileOffset(useLoc)) {
            return &use;
        }
    }
    return nullptr;
}

clang::Expr* ASTVisitor::ignoreImplicit(clang::Expr* expr) {
    if (!expr) {
        return nullptr;
    }
    return expr->IgnoreImplicit();
}

} // namespace move_optimizer
