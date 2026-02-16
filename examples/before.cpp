// Example: Code before optimization
// This demonstrates typical cases where move optimization can be applied

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

// Case 1: Variable initialization with copy
void example1() {
    Widget w1;
    Widget w2 = w1;  // Copy - could be moved if w1 is not used afterwards
}

// Case 2: Return value
Widget createWidget() {
    Widget w;
    return w;  // Copy - should be moved
}

// Case 3: Function argument
void processWidget(Widget w) {
    // Process widget
}

void example3() {
    Widget w;
    processWidget(w);  // Copy - could be moved
}

// Case 4: Assignment
void example4() {
    Widget w1, w2;
    w2 = w1;  // Copy assignment - could be moved if w1 is not used afterwards
}

// Case 5: Container operations
void example5() {
    std::vector<Widget> widgets;
    Widget w;
    widgets.push_back(w);  // Copy - could be moved
}

// Case 6: String operations
void example6() {
    std::string str1 = "Hello";
    std::string str2 = str1;  // Copy - could be moved
    std::string str3;
    str3 = str1;  // Copy assignment - could be moved
}
