#pragma once
#include <cstdint>
struct json_escape_scanner {
    uint64_t next_is_escaped = 0ULL;
    static constexpr const uint64_t ODD_BITS = 0xAAAAAAAAAAAAAAAAULL;

    struct escaped_and_escape {
        uint64_t escaped;
        uint64_t escape;
    };

    inline escaped_and_escape next(uint64_t backslash) noexcept {
        uint64_t potential_escape = backslash & ~this->next_is_escaped;
        uint64_t maybe_escaped = potential_escape << 1;
        uint64_t maybe_escaped_and_odd_bits = maybe_escaped | ODD_BITS;
        uint64_t even_series_codes_and_odd_bits = maybe_escaped_and_odd_bits - potential_escape;
        uint64_t escape_and_terminal_code = even_series_codes_and_odd_bits ^ ODD_BITS;
        uint64_t escaped = escape_and_terminal_code ^ (backslash | this->next_is_escaped);
        uint64_t escape = escape_and_terminal_code & backslash;
        this->next_is_escaped = escape >> 63;
        return {escaped, escape};
    }
};