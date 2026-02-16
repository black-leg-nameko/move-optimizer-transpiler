#ifndef CODE_TRANSFORMER_H
#define CODE_TRANSFORMER_H

#include "ast_visitor.h"
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/AST/ASTContext.h>
#include <string>
#include <vector>

namespace move_optimizer {

class CodeTransformer {
public:
    CodeTransformer(clang::ASTContext& context, clang::Rewriter& rewriter);
    
    // Apply a single transformation
    bool applyTransformation(const Transformation& transformation);
    
    // Apply all transformations
    bool applyTransformations(const std::vector<Transformation>& transformations);
    
    // Get transformed code
    std::string getTransformedCode() const;
    
    // Safety checks
    bool validateTransformation(const Transformation& transformation);
    bool isAlreadyMoved(clang::SourceRange range);
    
private:
    clang::ASTContext& context_;
    clang::Rewriter& rewriter_;
    std::vector<clang::SourceRange> appliedRanges_;
    
    // Helper methods
    bool insertMove(clang::SourceLocation loc, clang::SourceRange range);
    bool wrapWithMove(clang::SourceRange range);
    std::string generateMoveCode(const Transformation& transformation);
    bool checkOverlap(clang::SourceRange range);
    bool isValidMoveTarget(clang::Expr* expr);
};

} // namespace move_optimizer

#endif // CODE_TRANSFORMER_H
