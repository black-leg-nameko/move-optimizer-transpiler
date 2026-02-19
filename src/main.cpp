#include "move_optimizer.h"
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <iostream>
#include <fstream>
#include <system_error>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory MoveOptimizerCategory("Move Optimizer Options");
static llvm::cl::opt<std::string> OutputFile("o", 
    llvm::cl::desc("Output file path"),
    llvm::cl::value_desc("filename"),
    llvm::cl::cat(MoveOptimizerCategory));
static llvm::cl::opt<std::string> OutputDir("out-dir",
    llvm::cl::desc("Output directory for multi-file mode"),
    llvm::cl::value_desc("directory"),
    llvm::cl::cat(MoveOptimizerCategory));

class MoveOptimizerAction : public ASTFrontendAction {
public:
    MoveOptimizerAction() : rewriter_(nullptr) {}
    
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, 
                                                    StringRef file) override {
        rewriter_ = std::make_unique<Rewriter>(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MoveOptimizerConsumer>(&CI.getASTContext(), rewriter_.get());
    }
    
    void EndSourceFileAction() override {
        if (!rewriter_) {
            return;
        }
        
        // Get the output file name
        std::string outputPath;
        if (!OutputFile.empty()) {
            outputPath = OutputFile;
        } else if (!OutputDir.empty()) {
            llvm::SmallString<256> outputPathBuf(OutputDir);
            llvm::sys::path::append(outputPathBuf, llvm::sys::path::filename(getCurrentFile()));
            outputPathBuf += ".optimized";
            outputPath = outputPathBuf.str().str();

            llvm::SmallString<256> parentDir(outputPathBuf);
            llvm::sys::path::remove_filename(parentDir);
            if (!parentDir.empty()) {
                std::error_code dirEc = llvm::sys::fs::create_directories(parentDir);
                if (dirEc) {
                    llvm::errs() << "Error creating output directory: " << dirEc.message() << "\n";
                    return;
                }
            }
        } else {
            outputPath = getCurrentFile().str() + ".optimized";
        }
        
        // Write the transformed code
        std::error_code EC;
        llvm::raw_fd_ostream OS(outputPath, EC, llvm::sys::fs::OF_None);
        if (EC) {
            llvm::errs() << "Error opening output file: " << EC.message() << "\n";
            return;
        }
        
        const RewriteBuffer* buffer = rewriter_->getRewriteBufferFor(
            getCompilerInstance().getSourceManager().getMainFileID());
        
        if (buffer) {
            OS << std::string(buffer->begin(), buffer->end());
        } else {
            // No changes made, write original file
            const SourceManager& SM = getCompilerInstance().getSourceManager();
            clang::FileID mainFileID = SM.getMainFileID();
            const FileEntry* FE = SM.getFileEntryForID(mainFileID);
            if (FE) {
                std::string filename = FE->getName().str();
                std::ifstream input(filename);
                if (input.is_open()) {
                    OS << input.rdbuf();
                }
            }
        }
        
        llvm::outs() << "Optimized: " << getCurrentFile() << " -> " << outputPath << "\n";
    }

private:
    std::unique_ptr<Rewriter> rewriter_;
    
    class MoveOptimizerConsumer : public ASTConsumer {
    public:
        MoveOptimizerConsumer(ASTContext* context, Rewriter* rewriter) 
            : context_(context), rewriter_(rewriter) {}
        
        void HandleTranslationUnit(ASTContext& context) override {
            move_optimizer::MoveOptimizer optimizer(context, *rewriter_);
            if (!optimizer.processAST(context)) {
                llvm::errs() << "Error processing AST\n";
                return;
            }
            
            if (!optimizer.applyTransformations()) {
                llvm::errs() << "Error applying transformations\n";
                return;
            }
        }
        
    private:
        ASTContext* context_;
        Rewriter* rewriter_;
    };
};

int main(int argc, const char** argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MoveOptimizerCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    const auto& sourcePaths = OptionsParser.getSourcePathList();
    if (!OutputFile.empty() && !OutputDir.empty()) {
        llvm::errs() << "Error: -o and --out-dir cannot be used together.\n";
        return 1;
    }
    if (!OutputFile.empty() && sourcePaths.size() != 1) {
        llvm::errs() << "Error: -o is only supported with a single input file. "
                        "Use --out-dir for multiple files.\n";
        return 1;
    }

    ClangTool Tool(OptionsParser.getCompilations(), 
                   sourcePaths);
    
    return Tool.run(newFrontendActionFactory<MoveOptimizerAction>().get());
}
