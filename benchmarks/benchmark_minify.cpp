#include <benchmark/benchmark.h>
#include "simd_minify.h"
#include "simd_minify_nibble.h"
#include <string>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef HAS_JQ
#include <jq.h>
#endif

const std::string* g_json_data = nullptr;

std::string load_file_mmap(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error: Could not open benchmark file: " << filename << "\n";
        exit(1);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        std::cerr << "Error: Could not stat file.\n";
        exit(1);
    }

    if (sb.st_size == 0) {
        close(fd);
        return "";
    }

    void* addr = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        std::cerr << "Error: mmap failed.\n";
        exit(1);
    }

    std::string data(static_cast<const char*>(addr), sb.st_size);

    munmap(addr, sb.st_size);
    close(fd);
    return data;
}


static void SIMD_Minify(benchmark::State& state) {
    if (!g_json_data || g_json_data->empty()) {
        state.SkipWithError("Benchmark payload is empty!");
        return;
    }
    
    for (auto _ : state) {
        std::string result = simd_minify_json(*g_json_data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(g_json_data->size()));
}
BENCHMARK(SIMD_Minify);

static void SIMD_Minify_Nibble(benchmark::State& state) {
    if (!g_json_data || g_json_data->empty()) {
        state.SkipWithError("Benchmark payload is empty!");
        return;
    }

    for (auto _ : state) {
        std::string result = simd_minify_json_nibble(*g_json_data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(g_json_data->size()));
}
BENCHMARK(SIMD_Minify_Nibble);
#ifdef HAS_JQ
static void Linux_JQ_Minify(benchmark::State& state) {
    if (!g_json_data || g_json_data->empty()) {
        state.SkipWithError("Benchmark payload is empty!");
        return;
    }

    for (auto _ : state) {
        jq_state* jv_machine = jq_init();
        if (!jv_machine) {
            state.SkipWithError("Could not initialize jq state machine");
            return;
        }
        if (!jq_compile(jv_machine, ".")) {
            jq_teardown(&jv_machine);
            state.SkipWithError("Failed to compile internal jq routine");
            return;
        }
        jv parser_input = jv_parse_sized(g_json_data->data(), g_json_data->size());       
        if (jv_is_valid(parser_input)) {
            jq_start(jv_machine, parser_input, 0);
            jv parser_output = jq_next(jv_machine);
            
            jv serialized_string = jv_dump_string(parser_output, 0);
            
            std::string result(jv_string_value(serialized_string));
            benchmark::DoNotOptimize(result);
            
            jv_free(serialized_string);
        } else {
            jv_free(parser_input);
        }
        jq_teardown(&jv_machine);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(g_json_data->size()));
}
BENCHMARK(Linux_JQ_Minify);
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_json_file> [benchmark_flags...]\n";
        return 1;
    }

    std::string target_file_path = argv[1];
    std::cout << "Pre-loading payload via mmap from: " << target_file_path << "\n";
    
    std::string payload = load_file_mmap(target_file_path);
    g_json_data = &payload;

    int benchmark_argc = argc - 1;
    char** benchmark_argv = &argv[1];
    benchmark_argv[0] = argv[0];

    ::benchmark::Initialize(&benchmark_argc, benchmark_argv);
    if (::benchmark::ReportUnrecognizedArguments(benchmark_argc, benchmark_argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown(); 
    return 0;
}