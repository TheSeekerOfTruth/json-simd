#include "simd_minify.h"
#include "json_escape_scanner.h"
#include "json_string_scanner.h"
#include <cstdint> 
#include <immintrin.h>       

std::string simd_minify_json(std::string json_input) {
    if (json_input.empty()) return "";

    std::string output;
    output.resize(json_input.size()); 
    char* output_ptr = output.data();

    json_escape_scanner escape_scanner;
    uint64_t prev_string_state = 0ULL;

    size_t i = 0;
    size_t len = json_input.size();

    __m512i v_quote  = _mm512_set1_epi8('"');
    __m512i v_bslash = _mm512_set1_epi8('\\');
    __m512i v_space  = _mm512_set1_epi8(0x20);
    __m512i t_space  = _mm512_set1_epi8(0x09);
    __m512i r_space  = _mm512_set1_epi8(0x0A);
    __m512i n_space  = _mm512_set1_epi8(0x0D);

    for (; i + 63 < len; i += 64) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&json_input[i]));

        uint64_t backslash_mask = _mm512_cmpeq_epi8_mask(chunk, v_bslash);
        uint64_t quote_mask     = _mm512_cmpeq_epi8_mask(chunk, v_quote);

        auto escape_res = escape_scanner.next(backslash_mask);
        uint64_t unescaped_quotes = quote_mask & ~escape_res.escaped;

        uint64_t string_mask = generate_string_mask(unescaped_quotes, prev_string_state);


        uint64_t is_control_ws = _mm512_cmpeq_epi8_mask(chunk, t_space); 
        is_control_ws |= _mm512_cmpeq_epi8_mask(chunk, r_space);
        is_control_ws |= _mm512_cmpeq_epi8_mask(chunk, n_space);
        uint64_t is_space      = _mm512_cmpeq_epi8_mask(chunk, v_space);

        uint64_t whitespace_mask = is_control_ws | is_space;
        uint64_t structural_whitespace = whitespace_mask & ~string_mask;
        uint64_t keep_mask = ~structural_whitespace;

        _mm512_mask_compressstoreu_epi8(output_ptr, keep_mask, chunk);
        output_ptr += std::popcount(keep_mask);
    }

    bool inside_string = (prev_string_state != 0);
    bool escaped = (escape_scanner.next_is_escaped != 0);

    for (; i < len; ++i) {
        char c = json_input[i];
        if (escaped) {
            escaped = false;
            *output_ptr++ = c;
        } else if (c == '\\') {
            escaped = true;
            *output_ptr++ = c;
        } else if (c == '"') {
            inside_string = !inside_string;
            *output_ptr++ = c;
        } else {
            if (inside_string) {
                *output_ptr++ = c;
            } else if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
                *output_ptr++ = c;
            }
        }
    }

    output.resize(output_ptr - output.data());
    return output;
}