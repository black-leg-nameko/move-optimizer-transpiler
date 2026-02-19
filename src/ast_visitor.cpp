#include "ast_visitor.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Basic/SourceManager.h>
#include <clang/AST/Decl.h>

namespace move_optimizer {

ASTVisitor::ASTVisitor(clang::ASTContext& context) 
    : context_(context), currentFunction_(nullptr) {
}

bool ASTVisitor::VisitFunctionDecl(clang::FunctionDecl* decl) {
    if (!decl || !decl->hasBody() || !decl->isThisDeclarationADefinition()) {
        return true;
    }

    currentFunction_ = decl;
    collectLastUsesForCurrentFunction();
    return true;
}

bool ASTVisitor::VisitCallExpr(clang::CallExpr* expr) {
    if (!expr) {
        return true;
    }

    for (unsigned i = 0; i < expr->getNumArgs(); ++i) {
        clang::Expr* arg = ignoreImplicit(expr->getArg(i));
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

    clang::Expr* retValue = ignoreImplicit(stmt->getRetValue());

    // Restrict return optimization to by-value parameters only.
    // This avoids pessimizing NRVO for local variables.
    const auto* declRef = clang::dyn_cast<clang::DeclRefExpr>(retValue);
    if (!declRef) {
        return true;
    }
    const auto* param = clang::dyn_cast<clang::ParmVarDecl>(declRef->getDecl());
    if (!param) {
        return true;
    }

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

bool ASTVisitor::isCopyOperation(clang::Expr* expr) {
    expr = ignoreImplicit(expr);
    if (!expr) {
        return false;
    }

    if (clang::CXXConstructExpr* construct = 
        clang::dyn_cast<clang::CXXConstructExpr>(expr)) {
        return construct->getConstructor()->isCopyConstructor();
    }

    if (clang::DeclRefExpr* declRef = 
        clang::dyn_cast<clang::DeclRefExpr>(expr)) {
        if (const auto* var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
            clang::QualType type = var->getType().getNonReferenceType();
            if (type->isRecordType()) {
                return true;
            }
        }
    }
    
    return false;
}
bool ASTVisitor::hasMoveConstructor(clang::QualType type) {
    type = type.getNonReferenceType();
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

bool ASTVisitor::isSafeToMove(clang::Expr* expr, const clang::Stmt* context) {
    expr = ignoreImplicit(expr);
    if (!expr || !context) {
        return false;
    }

    if (!expr->isLValue()) {
        return false;
    }

    clang::QualType type = expr->getType().getNonReferenceType();
    if (!type->isRecordType() || type.isConstQualified()) {
        return false;
    }

    if (!hasMoveConstructor(type)) {
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

    if (clang::dyn_cast<clang::ReturnStmt>(context)) {
        return true;
    }

    if (clang::isa<clang::CallExpr>(context)) {
        return isLastUseInCurrentFunction(var, expr->getBeginLoc());
    }

    return false;
}

bool ASTVisitor::isLastUseInCurrentFunction(const clang::VarDecl* var, clang::SourceLocation useLoc) const {
    if (!var || !currentFunction_ || !useLoc.isValid()) {
        return false;
    }

    auto it = lastUseLocations_.find(var);
    if (it == lastUseLocations_.end() || !it->second.isValid()) {
        return false;
    }

    const auto& sm = context_.getSourceManager();
    return !sm.isBeforeInTranslationUnit(useLoc, it->second);
}

void ASTVisitor::collectLastUsesForCurrentFunction() {
    lastUseLocations_.clear();
    if (!currentFunction_ || !currentFunction_->hasBody()) {
        return;
    }

    class LastUseCollector : public clang::RecursiveASTVisitor<LastUseCollector> {
    public:
        LastUseCollector(clang::ASTContext& context,
                         std::map<const clang::VarDecl*, clang::SourceLocation>& lastUses)
            : context_(context), lastUses_(lastUses) {}

        bool VisitDeclRefExpr(clang::DeclRefExpr* expr) {
            if (!expr || !expr->getLocation().isValid()) {
                return true;
            }
            const auto* var = clang::dyn_cast<clang::VarDecl>(expr->getDecl());
            if (!var) {
                return true;
            }

            auto& lastLoc = lastUses_[var];
            if (!lastLoc.isValid() ||
                context_.getSourceManager().isBeforeInTranslationUnit(lastLoc, expr->getLocation())) {
                lastLoc = expr->getLocation();
            }
            return true;
        }

    private:
        clang::ASTContext& context_;
        std::map<const clang::VarDecl*, clang::SourceLocation>& lastUses_;
    };

    LastUseCollector collector(context_, lastUseLocations_);
    collector.TraverseStmt(currentFunction_->getBody());
}

clang::Expr* ASTVisitor::ignoreImplicit(clang::Expr* expr) {
    if (!expr) {
        return nullptr;
    }
    return expr->IgnoreImplicit();
}

} // namespace move_optimizer
