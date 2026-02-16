#ifndef MOVE_OPTIMIZER_H
#define MOVE_OPTIMIZER_H

#include "ast_visitor.h"
#include "code_transformer.h"
#include <clang/AST/ASTContext.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <memory>
#include <string>
#include <vector>

namespace move_optimizer {

class MoveOptimizer {
public:
    MoveOptimizer(clang::ASTContext& context, clang::Rewriter& rewriter);
    ~MoveOptimizer();

    // Process AST and collect optimization opportunities
    bool processAST(clang::ASTContext& context);
    
    // Apply transformations
    bool applyTransformations();

private:
    clang::ASTContext& context_;
    clang::Rewriter& rewriter_;
    std::unique_ptr<ASTVisitor> astVisitor_;
    std::unique_ptr<CodeTransformer> transformer_;
    std::vector<Transformation> transformations_;
};

} // namespace move_optimizer

#endif // MOVE_OPTIMIZER_H
