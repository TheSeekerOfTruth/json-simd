#pragma once
#include <cstdint>
#include <immintrin.h>
#include <wmmintrin.h>


inline uint64_t generate_string_mask(uint64_t unescaped_quotes, uint64_t& prev_string_state) noexcept {
    __m128i q = _mm_cvtsi64_si128(unescaped_quotes);    
    __m128i all_ones = _mm_set1_epi64x(-1LL);
    __m128i xor_scan = _mm_clmulepi64_si128(q, all_ones, 0);
    uint64_t string_mask = _mm_cvtsi128_si64(xor_scan);
    string_mask ^= prev_string_state;
    prev_string_state = static_cast<int64_t>(string_mask) >> 63; 
    return string_mask;
}