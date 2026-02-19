#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include <clang/AST/AST.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Analysis/CFG.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <unordered_map>

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
    bool VisitFunctionDecl(clang::FunctionDecl* decl);
    bool VisitCallExpr(clang::CallExpr* expr);
    bool VisitReturnStmt(clang::ReturnStmt* stmt);
    
    // Get collected transformations
    const std::vector<Transformation>& getTransformations() const { return transformations_; }
    void clearTransformations() { transformations_.clear(); }
    
private:
    struct UsePosition {
        unsigned blockId;
        unsigned elementIndex;
        clang::SourceLocation location;
    };

    clang::ASTContext& context_;
    std::vector<Transformation> transformations_;
    
    clang::FunctionDecl* currentFunction_;
    std::unique_ptr<clang::CFG> currentFunctionCfg_;
    std::map<const clang::VarDecl*, std::vector<UsePosition>> variableUsePositions_;
    std::unordered_map<unsigned, const clang::CFGBlock*> cfgBlocksById_;
    
    // Helper methods
    bool isCopyOperation(clang::Expr* expr);
    bool hasMoveConstructor(clang::QualType type);
    bool isSafeToMove(clang::Expr* expr, const clang::Stmt* context);
    bool isLastUseInCurrentFunction(const clang::VarDecl* var, clang::SourceLocation useLoc) const;
    void collectUsesForCurrentFunction();
    bool canOccurAfter(const UsePosition& current, const UsePosition& candidate) const;
    bool isReachable(const clang::CFGBlock* from, const clang::CFGBlock* to) const;
    bool blockCanReachItself(const clang::CFGBlock* block) const;
    const UsePosition* findUsePosition(const clang::VarDecl* var, clang::SourceLocation useLoc) const;
    static clang::Expr* ignoreImplicit(clang::Expr* expr);
};

} // namespace move_optimizer

#endif // AST_VISITOR_H
