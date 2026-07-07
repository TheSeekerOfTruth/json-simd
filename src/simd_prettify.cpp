#include "simd_prettify.h"
#include "json_escape_scanner.h"
#include "json_string_scanner.h"
#include <vector>
#include <cstdint> 
#include <immintrin.h>       
#include <bit>
#include <cstring>
#include <algorithm>

struct StructuralMap {
    std::vector<uint32_t> indexes;
    std::vector<char> types;
};

static StructuralMap simd_analyze_json(const std::string& json_input) {
    StructuralMap map;
    size_t len = json_input.size();
    const char* json_data = json_input.data();

    map.indexes.reserve(len);
    map.types.reserve(12);

    json_escape_scanner escape_scanner;
    uint64_t prev_string_state = 0ULL;

    size_t i = 0;
    __m512i v_quote   = _mm512_set1_epi8('"');
    __m512i v_bslash  = _mm512_set1_epi8('\\');
    __m512i v_space   = _mm512_set1_epi8(0x20);
    __m512i t_space   = _mm512_set1_epi8(0x09);
    __m512i r_space   = _mm512_set1_epi8(0x0A);
    __m512i n_space   = _mm512_set1_epi8(0x0D);

    __m512i v_open_b  = _mm512_set1_epi8('{');
    __m512i v_close_b = _mm512_set1_epi8('}');
    __m512i v_open_s  = _mm512_set1_epi8('[');
    __m512i v_close_s = _mm512_set1_epi8(']');
    __m512i v_comma   = _mm512_set1_epi8(',');
    __m512i v_colon   = _mm512_set1_epi8(':');

    for (; i + 63 < len; i += 64) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(json_data + i));

        uint64_t backslash_mask = _mm512_cmpeq_epi8_mask(chunk, v_bslash);
        uint64_t quote_mask     = _mm512_cmpeq_epi8_mask(chunk, v_quote);

        auto escape_res = escape_scanner.next(backslash_mask);
        uint64_t unescaped_quotes = quote_mask & ~escape_res.escaped;

        uint64_t string_mask = generate_string_mask(unescaped_quotes, prev_string_state);

        uint64_t is_control_ws = _mm512_cmpeq_epi8_mask(chunk, t_space) | 
                                 _mm512_cmpeq_epi8_mask(chunk, r_space) | 
                                 _mm512_cmpeq_epi8_mask(chunk, n_space);
        uint64_t is_space      = _mm512_cmpeq_epi8_mask(chunk, v_space);
        uint64_t whitespace_mask = is_control_ws | is_space;

        uint64_t structural_whitespace = whitespace_mask & ~string_mask;

        uint64_t tokens_mask = 
            _mm512_cmpeq_epi8_mask(chunk, v_open_b)  | _mm512_cmpeq_epi8_mask(chunk, v_close_b) |
            _mm512_cmpeq_epi8_mask(chunk, v_open_s)  | _mm512_cmpeq_epi8_mask(chunk, v_close_s) |
            _mm512_cmpeq_epi8_mask(chunk, v_comma)   | _mm512_cmpeq_epi8_mask(chunk, v_colon);
        tokens_mask &= ~string_mask;

        uint64_t structural_mask = tokens_mask | structural_whitespace;

        while (structural_mask != 0) {
            uint32_t t_idx = std::countr_zero(structural_mask);
            size_t absolute_pos = i + t_idx;
            
            map.indexes.push_back(absolute_pos);

            if ((structural_whitespace >> t_idx) & 1ULL) {
                map.types.push_back(' ');
            } else {
                map.types.push_back(json_data[absolute_pos]);
            }

            structural_mask &= structural_mask - 1;
        }
    }

    bool inside_string = (prev_string_state != 0);
    bool escaped = (escape_scanner.next_is_escaped != 0);

    for (; i < len; ++i) {
        char c = json_data[i];
        if (inside_string) {
            if (escaped) { escaped = false; } 
            else if (c == '\\') { escaped = true; } 
            else if (c == '"') { inside_string = false; }
        } else {
            if (c == '"') {
                inside_string = true;
                escaped = false;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                map.indexes.push_back(i);
                map.types.push_back(' ');
            } else if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':') {
                map.indexes.push_back(i);
                map.types.push_back(c);
            }
        }
    }

    return map;
}

std::string simd_prettify_json(const std::string& json_input, int indent_spaces) {
    if (json_input.empty()) return "";

    size_t len = json_input.size();
    const char* src = json_input.data();

    StructuralMap map = simd_analyze_json(json_input);

    std::string output;
    output.resize(len * 3); 
    char* output_ptr = output.data();

    size_t last_src_pos = 0;
    int depth = 0;

    auto append_indent = [&](int target_depth) {
        *output_ptr++ = '\n';
        int spaces = target_depth * indent_spaces;
        std::fill_n(output_ptr, spaces, ' ');
        output_ptr += spaces;
    };

    if (!map.indexes.empty()) {
        last_src_pos = map.indexes[0];
    }

    size_t num_tokens = map.indexes.size();
    for (size_t k = 0; k < num_tokens; ++k) {
        size_t next_token_pos = map.indexes[k];
        char token = map.types[k];

        size_t run_len = next_token_pos - last_src_pos;

        switch (token) {
            case ' ':
                if (run_len > 0) {
                    std::memcpy(output_ptr, src + last_src_pos, run_len);
                    output_ptr += run_len;
                }
                last_src_pos = next_token_pos + 1;
                break;

            case '{': 
            case '[':
                if (run_len > 0) {
                    std::memcpy(output_ptr, src + last_src_pos, run_len);
                    output_ptr += run_len;
                }
                *output_ptr++ = token;

                {
                    size_t next_valid_idx = k + 1;
                    while (next_valid_idx < num_tokens && map.types[next_valid_idx] == ' ') {
                        next_valid_idx++;
                    }
                    if (next_valid_idx < num_tokens && 
                        ((token == '{' && map.types[next_valid_idx] == '}') || 
                         (token == '[' && map.types[next_valid_idx] == ']'))) {
                    } else {
                        depth++;
                        append_indent(depth);
                    }
                }
                last_src_pos = next_token_pos + 1;
                break;

            case '}': 
            case ']':
                if (run_len > 0) {
                    std::memcpy(output_ptr, src + last_src_pos, run_len);
                    output_ptr += run_len;
                }

                if (next_token_pos > 0 && src[next_token_pos - 1] == (token == '}' ? '{' : '[')) {
                    *output_ptr++ = token;
                } else {
                    depth--;
                    append_indent(depth);
                    *output_ptr++ = token;
                }
                last_src_pos = next_token_pos + 1;
                break;

            case ',':
                if (run_len > 0) {
                    std::memcpy(output_ptr, src + last_src_pos, run_len);
                    output_ptr += run_len;
                }
                *output_ptr++ = ',';
                append_indent(depth);
                last_src_pos = next_token_pos + 1;
                break;

            case ':':
                if (run_len > 0) {
                    std::memcpy(output_ptr, src + last_src_pos, run_len);
                    output_ptr += run_len;
                }
                *output_ptr++ = ':';
                *output_ptr++ = ' '; 
                last_src_pos = next_token_pos + 1;
                break;
        }
    }

    if (last_src_pos < len) {
        std::memcpy(output_ptr, src + last_src_pos, len - last_src_pos);
        output_ptr += (len - last_src_pos);
    }

    output.resize(output_ptr - output.data());
    return output;
}