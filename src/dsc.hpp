#pragma once

#include <cstdint>
#include <vector>

namespace bgi {

bool is_dsc_format(const uint8_t* data, size_t size);
std::vector<uint8_t> decompress_dsc(const uint8_t* crypted, size_t crypted_size);

} // namespace bgi
