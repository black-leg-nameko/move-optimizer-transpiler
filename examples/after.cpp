// Example: Code after optimization
// This shows how the transpiler would optimize the code

#include <string>
#include <vector>
#include <memory>

class Widget {
public:
    Widget() = default;
    Widget(const Widget&) = default;
    Widget(Widget&&) = default;
    Widget& operator=(const Widget&) = default;
    Widget& operator=(Widget&&) = default;
    
private:
    std::string name_;
    std::vector<int> data_;
};

// Case 1: Variable initialization with move
void example1() {
    Widget w1;
    Widget w2 = std::move(w1);  // Optimized: moved instead of copied
}

// Case 2: Return value
Widget createWidget() {
    Widget w;
    return std::move(w);  // Optimized: moved instead of copied
}

// Case 3: Function argument
void processWidget(Widget w) {
    // Process widget
}

void example3() {
    Widget w;
    processWidget(std::move(w));  // Optimized: moved instead of copied
}

// Case 4: Assignment
void example4() {
    Widget w1, w2;
    w2 = std::move(w1);  // Optimized: moved instead of copied
}

// Case 5: Container operations
void example5() {
    std::vector<Widget> widgets;
    Widget w;
    widgets.push_back(std::move(w));  // Optimized: moved instead of copied
}

// Case 6: String operations
void example6() {
    std::string str1 = "Hello";
    std::string str2 = std::move(str1);  // Optimized: moved instead of copied
    std::string str3;
    str3 = std::move(str1);  // Optimized: moved instead of copied
}
