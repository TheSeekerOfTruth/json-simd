#include "simd_minify.h"
#include "json_escape_scanner.h"
#include "json_string_scanner.h"
#include <cstdint> 
#include <immintrin.h> 
#include <bit>

std::string simd_minify_json_nibble(std::string json_input) {
    if (json_input.empty()) return "";

    std::string output;
    output.resize(json_input.size()); 
    char* output_ptr = output.data();

    json_escape_scanner escape_scanner;
    uint64_t prev_string_state = 0ULL;

    size_t i = 0;
    size_t len = json_input.size();

    // Bit 0 (0x01): Space character (' ')
    // Bit 1 (0x02): Quote character ('"')
    // Bit 2 (0x04): Backslash character ('\')
    // Bit 3 (0x08): Control Whitespace ('\t', '\n', '\r')

    alignas(16) const uint8_t low_nibble_mask[16] = {
        0x01, // 0x0: ' '  (0x20) -> Bit 0 
        0x00, // 0x1: None
        0x02, // 0x2: '"'  (0x22) -> Bit 1 
        0x00, // 0x3: None
        0x00, // 0x4: None
        0x00, // 0x5: None
        0x00, // 0x6: None
        0x00, // 0x7: None
        0x00, // 0x8: None
        0x08, // 0x9: '\t' (0x09) -> Bit 3 
        0x08, // 0xA: '\n' (0x0A) -> Bit 3 
        0x00, // 0xB: None
        0x04, // 0xC: '\\' (0x5C) -> Bit 2 
        0x08, // 0xD: '\r' (0x0D) -> Bit 3 
        0x00, // 0xE: None
        0x00  // 0xF: None
    };

    // 16-byte Lookup table mapped by High Nibble (0x0 to 0xF)
    alignas(16) const uint8_t high_nibble_mask[16] = {
        0x08, // Authorizes Bit 3 (\t, \n, \r)
        0x00, // None
        0x03, // Authorizes Bit 0 (Space) & Bit 1 (Quote)
        0x00, // None
        0x00, // None
        0x04, // Authorizes Bit 2 (Backslash)
        0x00, // None
        0x00, // None
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // 0x8 to 0xF (Unused ASCII space)
    };

    __m512i v_low_nibble_mask  = _mm512_broadcast_i32x4(_mm_load_si128(reinterpret_cast<const __m128i*>(low_nibble_mask)));
    __m512i v_high_nibble_mask = _mm512_broadcast_i32x4(_mm_load_si128(reinterpret_cast<const __m128i*>(high_nibble_mask)));
    
    __m512i v_nibble_filter = _mm512_set1_epi8(0x0F);

    for (; i + 63 < len; i += 64) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&json_input[i]));

        __m512i low_nibbles  = _mm512_and_si512(chunk, v_nibble_filter);
        __m512i high_nibbles = _mm512_and_si512(_mm512_srli_epi32(chunk, 4), v_nibble_filter);

        __m512i low_features  = _mm512_shuffle_epi8(v_low_nibble_mask, low_nibbles);
        __m512i high_features = _mm512_shuffle_epi8(v_high_nibble_mask, high_nibbles);

        __m512i final_features = _mm512_and_si512(low_features, high_features);


        uint64_t whitespace_mask = _mm512_test_epi8_mask(final_features, _mm512_set1_epi8(0x09));
        uint64_t quote_mask      = _mm512_test_epi8_mask(final_features, _mm512_set1_epi8(0x02)); // Bit 1
        uint64_t backslash_mask  = _mm512_test_epi8_mask(final_features, _mm512_set1_epi8(0x04)); // Bit 2

        auto escape_res = escape_scanner.next(backslash_mask);
        uint64_t unescaped_quotes = quote_mask & ~escape_res.escaped;

        uint64_t string_mask = generate_string_mask(unescaped_quotes, prev_string_state);

        uint64_t structural_whitespace = whitespace_mask & ~string_mask;
        uint64_t keep_mask = ~structural_whitespace;

        _mm512_mask_compressstoreu_epi8(output_ptr, keep_mask, chunk);
        output_ptr += std::popcount(keep_mask); 
    }

    bool inside_string = ((prev_string_state & (1ULL << 63)) != 0);
    bool escaped = (escape_scanner.next_is_escaped != 0);

    for (; i < len; ++i) {
        char c = json_input[i];
        
        if (inside_string) {
            *output_ptr++ = c;
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inside_string = false;
            }
        } else {
            if (c == '"') {
                inside_string = true;
                escaped = false;
                *output_ptr++ = c;
            } else if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D && c != '\0') {
                *output_ptr++ = c;
            }
        }
    }

    size_t final_written_len = output_ptr - output.data();
    output.resize(final_written_len);
    return output;
}