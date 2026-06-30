#include "simd_minify.h"
#include "simd_minify_nibble.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>

std::string read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        std::cerr << "[-] Error: Could not open file: " << path << "\n";
        exit(1);
    }
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::string buffer(size, '\0');
    ifs.read(&buffer[0], size);
    return buffer;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "[-] Error: Could not write to file: " << path << "\n";
        exit(1);
    }
    ofs.write(content.data(), content.size());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_input_json>\n";
        return 1;
    }

    std::string og_json = read_file(argv[1]);

    // Ensure the output directory exists
    mkdir("output", 0777); 

    // Run algorithms and drop files directly to disk
    write_file("output/original.json", simd_minify_json(og_json));
    write_file("output/nibble.json", simd_minify_json_nibble(og_json));

    std::cout << "[+] Minified files written to output/original.json and output/nibble.json\n";
    return 0;
}