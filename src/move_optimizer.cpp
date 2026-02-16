#include "move_optimizer.h"
#include <clang/AST/ASTContext.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <fstream>
#include <sstream>

namespace move_optimizer {

MoveOptimizer::MoveOptimizer(clang::ASTContext& context, clang::Rewriter& rewriter)
    : context_(context), rewriter_(rewriter) {
    transformer_ = std::make_unique<CodeTransformer>(context_, rewriter_);
}

MoveOptimizer::~MoveOptimizer() {
    // Destructor implementation
}

bool MoveOptimizer::processAST(clang::ASTContext& context) {
    if (!astVisitor_) {
        astVisitor_ = std::make_unique<ASTVisitor>(context);
    }
    
    // Traverse the AST
    astVisitor_->TraverseDecl(context.getTranslationUnitDecl());
    
    // Collect transformations
    transformations_ = astVisitor_->getTransformations();
    
    return true;
}

bool MoveOptimizer::applyTransformations() {
    if (!transformer_) {
        transformer_ = std::make_unique<CodeTransformer>(context_, rewriter_);
    }
    
    return transformer_->applyTransformations(transformations_);
}

} // namespace move_optimizer
