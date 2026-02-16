#include "ast_visitor.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <sstream>
#include <map>

namespace move_optimizer {

ASTVisitor::ASTVisitor(clang::ASTContext& context) 
    : context_(context), currentFunction_(nullptr) {
}

bool ASTVisitor::VisitVarDecl(clang::VarDecl* decl) {
    if (!decl || !decl->hasInit()) {
        return true;
    }
    
    clang::Expr* init = decl->getInit();
    if (isCopyOperation(init) && isSafeToMove(init, decl->getInit())) {
        clang::SourceRange range = init->getSourceRange();
        transformations_.emplace_back(
            Transformation::VARIABLE_ASSIGNMENT_MOVE,
            decl->getLocation(),
            range
        );
    }
    
    // Track variable declaration
    trackVariableUsage(decl, nullptr);
    
    return true;
}

bool ASTVisitor::VisitFunctionDecl(clang::FunctionDecl* decl) {
    if (!decl) {
        return true;
    }
    
    currentFunction_ = decl;
    
    // Check function parameters for move opportunities
    if (decl->hasBody()) {
        // Parameters that are passed by value and could be moved
        for (unsigned i = 0; i < decl->getNumParams(); ++i) {
            clang::ParmVarDecl* param = decl->getParamDecl(i);
            if (param && param->getType()->isRecordType() && 
                !param->getType()->isReferenceType()) {
                // Track parameter usage
                trackVariableUsage(param, nullptr);
            }
        }
    }
    
    return true;
}

bool ASTVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr* expr) {
    if (!expr) {
        return true;
    }
    
    // Check if this is a copy construction that could be a move
    if (expr->getConstructor()->isCopyConstructor()) {
        if (expr->getNumArgs() == 1) {
            clang::Expr* arg = expr->getArg(0);
            if (isSafeToMove(arg, expr)) {
                clang::SourceRange range = expr->getSourceRange();
                transformations_.emplace_back(
                    Transformation::CONSTRUCTOR_INIT_MOVE,
                    expr->getLocation(),
                    range
                );
            }
        }
    }
    
    return true;
}

bool ASTVisitor::VisitCallExpr(clang::CallExpr* expr) {
    // Check function call arguments for move opportunities
    if (!expr) {
        return true;
    }
    
    for (unsigned i = 0; i < expr->getNumArgs(); ++i) {
        clang::Expr* arg = expr->getArg(i);
        if (isCopyOperation(arg) && isSafeToMove(arg, expr)) {
            clang::SourceRange range = arg->getSourceRange();
            transformations_.emplace_back(
                Transformation::FUNCTION_ARG_MOVE,
                expr->getLocation(),
                range
            );
        }
    }
    
    return true;
}

bool ASTVisitor::VisitReturnStmt(clang::ReturnStmt* stmt) {
    if (!stmt || !stmt->getRetValue()) {
        return true;
    }
    
    clang::Expr* retValue = stmt->getRetValue();
    
    // Check if return value is a copy that could be moved
    if (isCopyOperation(retValue) && isSafeToMove(retValue, stmt)) {
        clang::SourceRange range = retValue->getSourceRange();
        transformations_.emplace_back(
            Transformation::RETURN_VALUE_MOVE,
            stmt->getReturnLoc(),
            range
        );
    }
    
    return true;
}

bool ASTVisitor::VisitBinaryOperator(clang::BinaryOperator* op) {
    if (!op || op->getOpcode() != clang::BO_Assign) {
        return true;
    }
    
    clang::Expr* rhs = op->getRHS();
    if (isCopyOperation(rhs) && isSafeToMove(rhs, op)) {
        clang::SourceRange range = rhs->getSourceRange();
        transformations_.emplace_back(
            Transformation::VARIABLE_ASSIGNMENT_MOVE,
            op->getOperatorLoc(),
            range
        );
    }
    
    return true;
}

bool ASTVisitor::isCopyOperation(clang::Expr* expr) {
    if (!expr) {
        return false;
    }
    
    // Check if expression involves a copy constructor
    if (clang::CXXConstructExpr* construct = 
        clang::dyn_cast<clang::CXXConstructExpr>(expr)) {
        return construct->getConstructor()->isCopyConstructor();
    }
    
    // Check if it's a variable that would be copied
    if (clang::DeclRefExpr* declRef = 
        clang::dyn_cast<clang::DeclRefExpr>(expr)) {
        clang::ValueDecl* decl = declRef->getDecl();
        if (clang::VarDecl* var = clang::dyn_cast<clang::VarDecl>(decl)) {
            clang::QualType type = var->getType();
            if (type->isRecordType() && !type->isReferenceType()) {
                return true;
            }
        }
    }
    
    return false;
}

bool ASTVisitor::canBeMoved(clang::Expr* expr) {
    if (!expr) {
        return false;
    }
    
    // Check if the type has a move constructor
    clang::QualType type = expr->getType();
    
    // Remove references to get the actual type
    if (type->isReferenceType()) {
        type = type.getNonReferenceType();
    }
    
    if (!type->isRecordType()) {
        return false;
    }
    
    // Check if it's an lvalue (can be moved)
    if (expr->isLValue()) {
        // Check if it's not const
        if (!type.isConstQualified()) {
            return true;
        }
    }
    
    return false;
}

bool ASTVisitor::VisitDeclRefExpr(clang::DeclRefExpr* expr) {
    if (!expr) {
        return true;
    }
    
    clang::VarDecl* var = clang::dyn_cast<clang::VarDecl>(expr->getDecl());
    if (var) {
        trackVariableUsage(var, expr);
    }
    
    return true;
}

bool ASTVisitor::VisitStmt(clang::Stmt* stmt) {
    // Track statement context for move analysis
    return true;
}

bool ASTVisitor::isLastUse(clang::VarDecl* var, clang::Stmt* currentStmt) {
    if (!var || !currentStmt) {
        return false;
    }
    
    auto it = variableUsages_.find(var);
    if (it == variableUsages_.end()) {
        return false;
    }
    
    // Check if this is the last use
    return it->second.lastUse == currentStmt;
}

void ASTVisitor::trackVariableUsage(clang::VarDecl* var, clang::Stmt* stmt) {
    if (!var) {
        return;
    }
    
    auto& usage = variableUsages_[var];
    usage.var = var;
    
    if (stmt) {
        usage.uses.push_back(stmt);
        usage.lastUse = stmt;
        
        // Check if this is a modification
        if (clang::BinaryOperator* op = clang::dyn_cast<clang::BinaryOperator>(stmt)) {
            if (op->isAssignmentOp()) {
                usage.isModified = true;
            }
        }
    }
}

bool ASTVisitor::hasMoveConstructor(clang::QualType type) {
    if (!type->isRecordType()) {
        return false;
    }
    
    const clang::CXXRecordDecl* record = type->getAsCXXRecordDecl();
    if (!record) {
        return false;
    }
    
    // Check if the type has a move constructor
    for (clang::CXXConstructorDecl* ctor : record->ctors()) {
        if (ctor->isMoveConstructor()) {
            return true;
        }
    }
    
    return false;
}

bool ASTVisitor::isSafeToMove(clang::Expr* expr, clang::Stmt* context) {
    if (!expr || !canBeMoved(expr)) {
        return false;
    }
    
    // Check if type has move constructor
    clang::QualType type = expr->getType();
    if (type->isReferenceType()) {
        type = type.getNonReferenceType();
    }
    
    if (!hasMoveConstructor(type)) {
        return false;
    }
    
    // Check if it's a variable reference
    if (clang::DeclRefExpr* declRef = clang::dyn_cast<clang::DeclRefExpr>(expr)) {
        clang::VarDecl* var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl());
        if (var) {
            // Check if this is the last use
            if (context && isLastUse(var, context)) {
                return true;
            }
            
            // For return statements, it's generally safe to move
            if (clang::dyn_cast<clang::ReturnStmt>(context)) {
                return true;
            }
        }
    }
    
    // For temporary objects, always safe to move
    if (expr->isRValue()) {
        return true;
    }
    
    return false;
}

std::string ASTVisitor::getSourceText(clang::SourceRange range) {
    clang::SourceManager& sm = context_.getSourceManager();
    clang::LangOptions langOpts;
    
    clang::SourceLocation begin = range.getBegin();
    clang::SourceLocation end = range.getEnd();
    
    // Get the text between begin and end
    bool invalid = false;
    const char* start = sm.getCharacterData(begin, &invalid);
    if (invalid) {
        return "";
    }
    
    const char* finish = sm.getCharacterData(end, &invalid);
    if (invalid) {
        return "";
    }
    
    return std::string(start, finish - start);
}

} // namespace move_optimizer
