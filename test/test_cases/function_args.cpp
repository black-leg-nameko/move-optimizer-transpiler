// Test case: Function argument move optimization
// This file demonstrates function argument move optimization

#include <string>
#include <vector>

class Resource {
public:
    Resource() = default;
    Resource(const Resource&) = default;
    Resource(Resource&&) = default;
    Resource& operator=(const Resource&) = default;
    Resource& operator=(Resource&&) = default;
};

void processResource(Resource res) {
    // Process resource
}

void testFunctionArgs() {
    Resource res1;
    processResource(res1);  // Should be optimized to: processResource(std::move(res1));
    
    Resource res2;
    Resource res3 = res2;
    processResource(res3);  // Should be optimized to: processResource(std::move(res3));
}

void processString(std::string str) {
    // Process string
}

void testStringArgs() {
    std::string str = "test";
    processString(str);  // Should be optimized to: processString(std::move(str));
}

void processVector(std::vector<int> vec) {
    // Process vector
}

void testVectorArgs() {
    std::vector<int> vec;
    vec.push_back(1);
    processVector(vec);  // Should be optimized to: processVector(std::move(vec));
}
