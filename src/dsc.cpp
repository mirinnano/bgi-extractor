#include "dsc.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace bgi {

namespace {

inline uint8_t myHIBYTE(uint16_t v) { return static_cast<uint8_t>(v >> 8); }
inline uint8_t myLOBYTE(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }
inline uint16_t myHIWORD(uint32_t v) { return static_cast<uint16_t>(v >> 16); }
inline uint16_t myLOWORD(uint32_t v) { return static_cast<uint16_t>(v & 0xFFFF); }

inline uint32_t read32(const uint8_t** c) {
    uint32_t i;
    std::memcpy(&i, *c, 4);
    *c += 4;
    return i;
}

inline uint32_t hash_update(uint32_t* hash_val) {
    uint32_t edx = 20021 * myLOWORD(*hash_val);
    uint32_t eax = (20021 * myHIWORD(*hash_val)) + (346 * *hash_val) + myHIWORD(edx);
    *hash_val = (myLOWORD(eax) << 16) + myLOWORD(edx) + 1;
    return eax & 0x7FFF;
}

struct NodeDSC {
    uint32_t has_childs = 0;
    uint32_t leaf_value = 0;
    uint32_t childs[2] = {0, 0};
};

} // anonymous namespace

bool is_dsc_format(const uint8_t* data, size_t size) {
    if (size < 32) return false;
    return std::memcmp(data, "DSC FORMAT 1.00", 15) == 0;
}

std::vector<uint8_t> decompress_dsc(const uint8_t* crypted, size_t crypted_size) {
    if (!is_dsc_format(crypted, crypted_size)) {
        return {};
    }

    const uint8_t* begin_of_raw_data = crypted;
    crypted += 16;
    uint32_t hash = read32(&crypted);
    uint32_t size = read32(&crypted);
    /* uint32_t v2 = */ read32(&crypted);
    /* uint32_t padding = */ read32(&crypted);

    std::vector<NodeDSC> nodes(1024);
    uint32_t buffer[512] = {0};
    int buffer_len = 0;

    for (int n = 0; n < 512; ++n) {
        uint8_t v = crypted[n] - static_cast<uint8_t>(hash_update(&hash));
        if (v != 0) {
            buffer[buffer_len++] = (static_cast<uint32_t>(v) << 16) + n;
        }
    }

    std::sort(buffer, buffer + buffer_len);

    uint32_t vector0[1024] = {0};
    uint32_t* v13 = vector0;
    uint32_t toggle = 0x200;
    uint32_t dec0 = 1;
    uint32_t value_set = 1;
    uint32_t nn = 0;

    for (int buffer_cur = 0; buffer_cur < buffer_len; ++nn) {
        uint32_t* vector0_ptr = &vector0[toggle];
        uint32_t* vector0_ptr_init = vector0_ptr;
        uint32_t group_count = 0;

        for (; buffer_cur < buffer_len && nn == myHIWORD(buffer[buffer_cur]); ++buffer_cur, ++v13, ++group_count) {
            nodes[*v13].has_childs = 0;
            nodes[*v13].leaf_value = buffer[buffer_cur] & 0x1FF;
        }

        uint32_t v18 = 2 * (dec0 - group_count);
        if (group_count < dec0) {
            dec0 = (dec0 - group_count);
            for (uint32_t dd = 0; dd < dec0; ++dd) {
                nodes[*v13].has_childs = 1;
                for (int m = 0; m < 2; ++m) {
                    *vector0_ptr++ = nodes[*v13].childs[m] = value_set;
                    ++value_set;
                }
                ++v13;
            }
        }
        dec0 = v18;
        v13 = vector0_ptr_init;
        toggle ^= 0x200;
    }

    crypted += 512;
    std::vector<uint8_t> data(size);

    uint32_t src_ptr = 0;
    uint32_t dst_ptr = 0;
    uint32_t src_end = static_cast<uint32_t>(crypted_size - (crypted - begin_of_raw_data));
    uint32_t dst_end = size;

    uint32_t bits = 0;
    uint32_t nbits = 0;

    while (src_ptr < src_end && dst_ptr < dst_end) {
        uint32_t nentry = 0;

        for (; nodes[nentry].has_childs; nbits--, bits = (bits << 1) & 0xFF) {
            if (nbits == 0) {
                nbits = 8;
                bits = crypted[src_ptr++];
            }
            nentry = nodes[nentry].childs[(bits >> 7) & 1];
        }

        uint16_t info = myLOWORD(nodes[nentry].leaf_value);

        if (myHIBYTE(info) == 1) {
            uint32_t cvalue = bits >> (8 - nbits);
            uint32_t nbits2 = nbits;
            if (nbits < 12) {
                uint32_t bytes = ((11 - nbits) >> 3) + 1;
                nbits2 = nbits;
                while (bytes--) {
                    cvalue = crypted[src_ptr++] + (cvalue << 8);
                    nbits2 += 8;
                }
            }
            nbits = nbits2 - 12;
            bits = myLOBYTE(cvalue << (8 - (nbits2 - 12)));

            int32_t offset = static_cast<int32_t>((cvalue >> (nbits2 - 12)) + 2);
            uint32_t ring_ptr = dst_ptr - offset;
            uint32_t count = myLOBYTE(info) + 2;

            while (count--) {
                data[dst_ptr++] = data[ring_ptr++];
            }
        } else {
            data[dst_ptr++] = myLOBYTE(info);
        }
    }

    return data;
}

} // namespace bgi
