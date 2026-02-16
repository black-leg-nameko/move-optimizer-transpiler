# C++ Move Semantics Optimizer Transpiler

C++トランスパイラ - 不要なコピー操作をmoveに自動変換してパフォーマンスを向上させます。

## 概要

このトランスパイラは、C++コードを解析して、不要なコピー操作を`std::move`を使ったmove操作に自動変換します。最適化を自動化できます。

## 主な機能

- **戻り値の最適化**: 関数の戻り値で不要なコピーをmoveに変換
- **関数引数の最適化**: 関数呼び出し時の引数でmove可能な場合に変換
- **変数代入の最適化**: 変数の初期化や代入でmove可能な場合に変換
- **コンストラクタ初期化の最適化**: コンストラクタ呼び出しでmove可能な場合に変換
- **安全性チェック**: 変換後のコードが正しくコンパイルできることを確認

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
./move-optimizer file1.cpp file2.cpp -o optimized.cpp
```

## 使用例

### Before (最適化前)

```cpp
Widget createWidget() {
    Widget w;
    return w;  // コピーが発生
}

void example() {
    Widget w1;
    Widget w2 = w1;  // コピーが発生
    processWidget(w1);  // コピーが発生
}
```

### After (最適化後)

```cpp
Widget createWidget() {
    Widget w;
    return std::move(w);  // moveに変換
}

void example() {
    Widget w1;
    Widget w2 = std::move(w1);  // moveに変換
    processWidget(std::move(w1));  // moveに変換
}
```

## アーキテクチャ

### 主要コンポーネント

1. **AST Visitor** (`ast_visitor.h/cpp`)
   - Clang ASTを走査してコピー操作を検出
   - 関数の戻り値、変数代入、関数引数を解析

2. **Move可能性解析** (`ast_visitor.cpp`)
   - 変数の使用状況を追跡
   - 最後の使用箇所を特定
   - コピーが不要かmove可能かを判定

3. **コード変換エンジン** (`code_transformer.h/cpp`)
   - `std::move()`の自動挿入
   - コピーコンストラクタ呼び出しをmoveコンストラクタに変換
   - 戻り値の最適化（RVO/NRVOの活用）

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

- 現在の実装は基本的なケースに対応しています
- 複雑なテンプレートコードやマクロを含むコードには対応していない場合があります
- 変数の使用状況追跡は簡易的な実装のため、より高度な解析が必要な場合があります

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
