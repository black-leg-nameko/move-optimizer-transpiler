// Test case: Return value optimization
// This file demonstrates return value move optimization

#include <string>
#include <vector>

class LargeObject {
public:
    LargeObject() = default;
    LargeObject(const LargeObject&) = default;
    LargeObject(LargeObject&&) = default;
    LargeObject& operator=(const LargeObject&) = default;
    LargeObject& operator=(LargeObject&&) = default;
    
private:
    int data[1000];
};

LargeObject createLargeObject() {
    LargeObject obj;
    return obj;  // Should be optimized to: return std::move(obj);
}

std::string createString() {
    std::string str = "test";
    return str;  // Should be optimized to: return std::move(str);
}

std::vector<int> createVector() {
    std::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    return vec;  // Should be optimized to: return std::move(vec);
}

LargeObject processAndReturn(LargeObject input) {
    // Process input
    return input;  // Should be optimized to: return std::move(input);
}
