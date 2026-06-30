#include "simd_minify.h"
#include "simd_minify_nibble.h"
#include <iostream>
#include <string>

int main() {
    // Valid representation containing genuine tabs and newlines mapped out
    std::string test_json = "{\n"
                            "  \"info\": \"This contains \\t tabs, \\n lines, and an escaped \\\"quote\\\".\",\n"
                            "  \"array\": [\n"
                            "    1\t,\n" 
                            "    2,\n"
                            "    3\n"
                            "  ],\n"
                            "  \"clean\": true\n"
                            "}";

    std::cout << "Raw Document Input:\n" << test_json << "\n\n";

    std::string minified = simd_minify_json(test_json);
    std::cout << "Resulting Output:\n" << minified << "\n";

    std::string minified_nibble = simd_minify_json_nibble(test_json);
    std::cout << "Resulting Output(Nibble):\n" << minified_nibble << "\n";
    return 0;
}