#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include <clang/AST/AST.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <vector>
#include <string>
#include <map>

namespace move_optimizer {

// Represents a transformation opportunity
struct Transformation {
    enum Type {
        RETURN_VALUE_MOVE,      // Move return value
        FUNCTION_ARG_MOVE,      // Move function argument
        VARIABLE_ASSIGNMENT_MOVE, // Move variable assignment
        CONSTRUCTOR_INIT_MOVE   // Move constructor initialization
    };
    
    Type type;
    clang::SourceLocation location;
    std::string originalCode;
    std::string transformedCode;
    clang::SourceRange range;
    
    Transformation(Type t, clang::SourceLocation loc, clang::SourceRange r)
        : type(t), location(loc), range(r) {}
};

class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
public:
    explicit ASTVisitor(clang::ASTContext& context);
    
    // Visit declarations
    bool VisitVarDecl(clang::VarDecl* decl);
    bool VisitFunctionDecl(clang::FunctionDecl* decl);
    bool VisitCXXConstructExpr(clang::CXXConstructExpr* expr);
    bool VisitCallExpr(clang::CallExpr* expr);
    bool VisitReturnStmt(clang::ReturnStmt* stmt);
    bool VisitBinaryOperator(clang::BinaryOperator* op);
    bool VisitDeclRefExpr(clang::DeclRefExpr* expr);
    bool VisitStmt(clang::Stmt* stmt);
    
    // Get collected transformations
    const std::vector<Transformation>& getTransformations() const { return transformations_; }
    void clearTransformations() { transformations_.clear(); }
    
private:
    clang::ASTContext& context_;
    std::vector<Transformation> transformations_;
    
    // Variable usage tracking
    struct VariableUsage {
        clang::VarDecl* var;
        std::vector<clang::Stmt*> uses;
        clang::Stmt* lastUse;
        bool isModified;
    };
    std::map<clang::VarDecl*, VariableUsage> variableUsages_;
    clang::FunctionDecl* currentFunction_;
    
    // Helper methods
    bool isCopyOperation(clang::Expr* expr);
    bool canBeMoved(clang::Expr* expr);
    bool isLastUse(clang::VarDecl* var, clang::Stmt* currentStmt);
    bool hasMoveConstructor(clang::QualType type);
    bool isSafeToMove(clang::Expr* expr, clang::Stmt* context);
    void trackVariableUsage(clang::VarDecl* var, clang::Stmt* stmt);
    std::string getSourceText(clang::SourceRange range);
};

} // namespace move_optimizer

#endif // AST_VISITOR_H
