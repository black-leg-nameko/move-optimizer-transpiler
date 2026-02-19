#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

class MoveOptimizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "move_optimizer_tests";
        fs::create_directories(testDir_);
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }
    
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    fs::path writeTestFile(const std::string& filename, const std::string& content) {
        fs::path path = testDir_ / filename;
        std::ofstream out(path);
        out << content;
        out.close();
        return path;
    }

    int runOptimizer(const fs::path& inputPath, const fs::path& outputPath) {
        std::ostringstream cmd;
        cmd << "\"" << optimizerBinary() << "\" "
            << "\"" << inputPath.string() << "\" "
            << "-- -std=c++17 -I.";
        if (!outputPath.empty()) {
            cmd.str("");
            cmd << "\"" << optimizerBinary() << "\" "
                << "\"" << inputPath.string() << "\" "
                << "-o \"" << outputPath.string() << "\" "
                << "-- -std=c++17 -I.";
        }
        return std::system(cmd.str().c_str());
    }

    static std::string detectCompiler() {
        if (std::system("command -v clang++ >/dev/null 2>&1") == 0) {
            return "clang++";
        }
        if (std::system("command -v g++ >/dev/null 2>&1") == 0) {
            return "g++";
        }
        return "";
    }

    static int compileSource(const fs::path& sourcePath) {
        const std::string compiler = detectCompiler();
        if (compiler.empty()) {
            return -1;
        }

        std::ostringstream cmd;
        cmd << compiler << " -std=c++17 -fsyntax-only "
            << "\"" << sourcePath.string() << "\"";
        return std::system(cmd.str().c_str());
    }

    static std::string optimizerBinary() {
#ifdef MOVE_OPTIMIZER_BIN
        return MOVE_OPTIMIZER_BIN;
#else
        return "move-optimizer";
#endif
    }

    fs::path testDir_;
};

TEST_F(MoveOptimizerTest, AddsMoveForLastUseFunctionArgument) {
    const std::string input = R"cpp(
#include <string>
void consume(std::string s) {}
void f() {
    std::string local = "hello";
    consume(local);
}
)cpp";
    const fs::path inPath = writeTestFile("arg_input.cpp", input);
    const fs::path outPath = testDir_ / "arg_output.cpp";

    ASSERT_EQ(runOptimizer(inPath, outPath), 0);
    const std::string out = readFile(outPath.string());
    EXPECT_NE(out.find("consume(std::move(local))"), std::string::npos);
    EXPECT_NE(out.find("#include <utility>"), std::string::npos);
}

TEST_F(MoveOptimizerTest, DoesNotMoveWhenVariableIsUsedAgain) {
    const std::string input = R"cpp(
#include <string>
void consume(std::string s) {}
void g() {
    std::string local = "hello";
    consume(local);
    consume(local);
}
)cpp";
    const fs::path inPath = writeTestFile("reuse_input.cpp", input);
    const fs::path outPath = testDir_ / "reuse_output.cpp";

    ASSERT_EQ(runOptimizer(inPath, outPath), 0);
    const std::string out = readFile(outPath.string());
    EXPECT_EQ(out.find("consume(std::move(local))"), std::string::npos);
}

TEST_F(MoveOptimizerTest, MovesByValueParameterOnReturn) {
    const std::string input = R"cpp(
#include <string>
std::string pass(std::string in) {
    return in;
}
)cpp";
    const fs::path inPath = writeTestFile("ret_input.cpp", input);
    const fs::path outPath = testDir_ / "ret_output.cpp";

    ASSERT_EQ(runOptimizer(inPath, outPath), 0);
    const std::string out = readFile(outPath.string());
    EXPECT_NE(out.find("return std::move(in);"), std::string::npos);
}

TEST_F(MoveOptimizerTest, KeepsLocalReturnUnchangedForNRVO) {
    const std::string input = R"cpp(
#include <string>
std::string makeString() {
    std::string local = "value";
    return local;
}
)cpp";
    const fs::path inPath = writeTestFile("nrvo_input.cpp", input);
    const fs::path outPath = testDir_ / "nrvo_output.cpp";

    ASSERT_EQ(runOptimizer(inPath, outPath), 0);
    const std::string out = readFile(outPath.string());
    EXPECT_EQ(out.find("return std::move(local);"), std::string::npos);
}

TEST_F(MoveOptimizerTest, TransformedOutputCompiles) {
    const std::string input = R"cpp(
#include <string>
void consume(std::string s) {}
std::string process(std::string value) {
    consume(value);
    return value;
}
)cpp";
    const fs::path inPath = writeTestFile("compile_input.cpp", input);
    const fs::path outPath = testDir_ / "compile_output.cpp";

    ASSERT_EQ(runOptimizer(inPath, outPath), 0);
    const int compileResult = compileSource(outPath);
    if (compileResult == -1) {
        GTEST_SKIP() << "No C++ compiler available for syntax check";
    }
    EXPECT_EQ(compileResult, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
