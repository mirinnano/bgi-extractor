#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace bgi {

enum class FileKind {
    Unknown,
    Script,
    Image,
    Audio,
    Raw
};

struct DecryptedResult {
    FileKind kind = FileKind::Unknown;
    std::string suggested_ext; // ".bss", ".bmp", ".ogg", ".wav", ".bin"
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bpp = 0;
};

class Decryptor {
public:
    static DecryptedResult decrypt(const uint8_t* data, size_t size, const std::string& entry_name);
    static bool save_as_bmp(const std::string& filepath, uint32_t width, uint32_t height, uint32_t bpp, const uint8_t* pixels);
};

} // namespace bgi
