# C++ Move Semantics Optimizer Transpiler

C++トランスパイラ - 不要なコピー操作をmoveに自動変換してパフォーマンスを向上させます。

## 概要

このトランスパイラは、C++コードを解析して、不要なコピー操作を`std::move`を使ったmove操作に自動変換します。最適化を自動化できます。

## 主な機能

- **関数引数の最適化**: 関数呼び出し時の引数で「関数内で最終使用」の場合に `std::move` を挿入
- **戻り値の最適化**: 値渡しパラメータを return する場合に `std::move` を挿入
- **ヘッダ自動補完**: `std::move` を挿入したファイルに `<utility>` を自動追加
- **安全性重視の保守的変換**: 判定が曖昧なケースは変換しない

## 要件

- C++17以上をサポートするコンパイラ
- CMake 3.15以上
- LLVM/Clang (LibToolingを使用)
- Google Test (テスト用、オプション)

### 依存関係のインストール

#### macOS (Homebrew)

```bash
# CMakeのインストール
brew install cmake

# LLVMのインストール（Clang LibToolingを含む）
brew install llvm

# Google Testのインストール（オプション、テスト用）
brew install googletest
```

インストール後、LLVMのパスを環境変数に設定する必要がある場合があります：

```bash
export LLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm
export Clang_DIR=$(brew --prefix llvm)/lib/cmake/clang
```

#### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install cmake llvm-dev clang-dev libclang-dev
```

#### Linux (Fedora/RHEL)

```bash
sudo dnf install cmake llvm-devel clang-devel
```

## ビルド方法

```bash
# プロジェクトディレクトリに移動
cd transpiler

# ビルドディレクトリを作成
mkdir build
cd build

# CMakeで設定
cmake ..

# ビルド
make
```

## 使用方法

```bash
# 基本的な使用方法
./move-optimizer input.cpp -o output.cpp

# 複数ファイルの処理
./move-optimizer file1.cpp file2.cpp --out-dir optimized
```

注意:
- `-o` は単一入力ファイルでのみ使用できます
- 複数入力ファイルでは `--out-dir` を使用してください

## 使用例

### Before (最適化前)

```cpp
Widget passthrough(Widget input) {
    return input;  // コピーが発生
}

void example() {
    Widget w1;
    processWidget(w1);  // コピーが発生
}
```

### After (最適化後)

```cpp
Widget passthrough(Widget input) {
    return std::move(input);  // moveに変換
}

void example() {
    Widget w1;
    processWidget(std::move(w1));  // moveに変換
}
```

## アーキテクチャ

### 主要コンポーネント

1. **AST Visitor** (`ast_visitor.h/cpp`)
   - Clang ASTを走査してコピー操作を検出
   - 関数の戻り値、関数引数を解析

2. **Move可能性解析** (`ast_visitor.cpp`)
   - CFGベースで変数使用を追跡
   - 分岐/ループを考慮した最終使用箇所を推定
   - コピーが不要かmove可能かを判定

3. **コード変換エンジン** (`code_transformer.h/cpp`)
   - `std::move()`の自動挿入
   - `<utility>`ヘッダの自動補完

4. **安全性チェック** (`code_transformer.cpp`)
   - 変換後のコードのセマンティクス検証
   - コンパイルエラーの検出
   - 重複変換の防止

## テスト

```bash
cd build
make move_optimizer_tests
./move_optimizer_tests
```

テストケースは `test/test_cases/` ディレクトリにあります。

## 制限事項

- 現在は安全性を優先し、保守的なケースのみ変換します
- 複雑なテンプレートコードやマクロ展開を含むコードには未対応のケースがあります
- 最終使用判定は関数単位の位置情報ベースであり、CFG ベース解析は未対応です

## 今後の改善予定

- より高度な変数使用状況追跡
- テンプレートコードへの対応強化
- パフォーマンス最適化
- より詳細なエラーメッセージ

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。

## 貢献

プルリクエストやイシューの報告を歓迎します。

## 参考文献

- [Clang LibTooling Documentation](https://clang.llvm.org/docs/LibTooling.html)
- [C++ Move Semantics](https://en.cppreference.com/w/cpp/language/move_constructor)
- [LLVM/Clang AST Documentation](https://clang.llvm.org/docs/IntroductionToTheClangAST.html)
