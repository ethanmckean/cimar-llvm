#include <cstdint>
#include <cstddef>

extern "C" {

// ASAN shadow memory offset for x86-64
#define SHADOW_OFFSET 2147450880ULL  // 0x7FFF8000
#define MAX_SEARCH_DISTANCE 512       // granules (±4KB search window)

// Check if access of 'size' bytes at 'addr' is valid per ASAN shadow memory
static inline bool is_valid_access(uint64_t addr, size_t size) {
    uint64_t shadow_addr = (addr >> 3) + SHADOW_OFFSET;
    uint8_t shadow_byte = *(volatile uint8_t*)shadow_addr;

    if (shadow_byte == 0x00) {
        return true;  // Fully valid
    }

    if (shadow_byte >= 0xF1) {
        return false;  // Poisoned
    }

    // Partially valid (0x01-0x07): check if access fits
    uint64_t offset = addr & 7;
    return (offset + size) <= shadow_byte;
}

// Bidirectional search for nearest valid memory address
void* __cima_find_nearest_valid(void* invalid_ptr, size_t access_size) {
    uint64_t invalid_addr = (uint64_t)invalid_ptr;
    uint64_t base_granule = invalid_addr >> 3;

    // Alternate between forward and backward search
    for (uint64_t offset = 0; offset < MAX_SEARCH_DISTANCE; offset++) {
        // Try forward direction
        uint64_t fwd_granule = base_granule + offset;
        uint64_t fwd_addr = fwd_granule << 3;  // Align to granule

        if (is_valid_access(fwd_addr, access_size)) {
            return (void*)fwd_addr;
        }

        if (offset > 0) {
            uint64_t bwd_granule = base_granule - offset;
            uint64_t bwd_addr = bwd_granule << 3;

            if (is_valid_access(bwd_addr, access_size)) {
                return (void*)bwd_addr;
            }
        }
    }

    // No valid memory found within search window
    return nullptr;
}

}  // extern "C"
