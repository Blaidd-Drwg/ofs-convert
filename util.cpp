#include "util.h"


uint32_t log2(uint32_t value) {
    return sizeof(value) * 8 - __builtin_clz(value) - 1;
}


uint32_t min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}


uint64_t from_lo_hi(uint32_t lo, uint32_t hi) {
    return static_cast<uint64_t>(hi) << 32 | lo;
}

uint32_t from_lo_hi(uint16_t lo, uint16_t hi) {
    return static_cast<uint32_t>(hi) << 16 | lo;
}


void set_lo_hi(uint32_t& lo, uint16_t& hi, uint64_t value) {
    lo = static_cast<uint32_t>(value & 0xFFFFFFFF);
    hi = static_cast<uint16_t>(value >> 32);
}


void set_lo_hi(uint32_t& lo, uint32_t& hi, uint64_t value) {
    lo = static_cast<uint32_t>(value & 0xFFFFFFFF);
    hi = static_cast<uint32_t>(value >> 32);
}


void set_lo_hi(uint16_t& lo, uint16_t& hi, uint32_t value) {
    lo = static_cast<uint16_t>(value & 0xFFFF);
    hi = static_cast<uint16_t>(value >> 16);
}


void incr_lo_hi(uint16_t& lo, uint16_t& hi, uint32_t diff) {
    set_lo_hi(lo, hi, from_lo_hi(lo, hi) + diff);
}


void decr_lo_hi(uint16_t& lo, uint16_t& hi, uint32_t diff) {
    set_lo_hi(lo, hi, from_lo_hi(lo, hi) - diff);
}


void bitmap_set_bit(uint8_t* bitmap, uint32_t bit_num) {
    bitmap[bit_num / 8] |= 1 << (bit_num % 8);
}

void bitmap_set_bits(uint8_t* bitmap, uint32_t begin, uint32_t end) {
    // TODO: More efficient
    for (uint32_t i = begin; i < end; ++i) {
        bitmap_set_bit(bitmap, i);
    }
}
