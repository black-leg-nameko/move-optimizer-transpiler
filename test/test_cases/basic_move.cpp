// Test case: Basic move optimization
// This file demonstrates cases where move optimization should be applied

#include <string>
#include <vector>

class TestClass {
public:
    TestClass() = default;
    TestClass(const TestClass&) = default;
    TestClass(TestClass&&) = default;
    TestClass& operator=(const TestClass&) = default;
    TestClass& operator=(TestClass&&) = default;
};

void testBasicMove() {
    TestClass obj1;
    TestClass obj2 = obj1;  // Should be optimized to: TestClass obj2 = std::move(obj1);
    
    std::string str1 = "hello";
    std::string str2 = str1;  // Should be optimized to: std::string str2 = std::move(str1);
}

TestClass createObject() {
    TestClass obj;
    return obj;  // Should be optimized to: return std::move(obj);
}

void testFunctionArg(TestClass obj) {
    // obj is passed by value, could be moved if caller doesn't need it
}

void testCall() {
    TestClass obj;
    testFunctionArg(obj);  // Should be optimized to: testFunctionArg(std::move(obj));
}
