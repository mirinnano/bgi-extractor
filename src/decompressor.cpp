#include "decompressor.hpp"
#include "dsc.hpp"
#include <cstring>
#include <fstream>
#include <algorithm>

namespace bgi {

namespace {

inline uint16_t read16(const uint8_t** c) {
    uint16_t i;
    std::memcpy(&i, *c, 2);
    *c += 2;
    return i;
}

inline uint32_t read32(const uint8_t** c) {
    uint32_t i;
    std::memcpy(&i, *c, 4);
    *c += 4;
    return i;
}

inline uint8_t read8(const uint8_t** c) {
    uint8_t i = **c;
    (*c)++;
    return i;
}

int32_t bse_rand(int32_t* seed) {
    int32_t tmp = (((*seed * 257 >> 8) + *seed * 97) + 23) ^ -1496474763;
    *seed = ((tmp >> 16) & 65535) | (tmp << 16);
    return *seed & 32767;
}

bool decrypt_bse_in_place(uint8_t* crypted, size_t size) {
    if (size < 80 || std::memcmp(crypted, "BSE 1.0", 7) != 0) return false;

    uint8_t* ptr = crypted + 8;
    read16(const_cast<const uint8_t**>(&ptr)); // 0x100
    uint8_t sum_check = read8(const_cast<const uint8_t**>(&ptr));
    uint8_t xor_check = read8(const_cast<const uint8_t**>(&ptr));
    int32_t hash = read32(const_cast<const uint8_t**>(&ptr));

    uint8_t* body = crypted + 16;
    int flags[64] = {0};

    for (int counter = 0; counter < 64; ++counter) {
        int target = 0;
        int r = bse_rand(&hash);
        int i = r & 0x3F;
        while (flags[i]) {
            i = (i + 1) & 0x3F;
        }
        r = bse_rand(&hash);
        int s = r & 0x07;
        target = i;
        int k = bse_rand(&hash);
        r = bse_rand(&hash);
        r = ((body[target] & 255) - r) & 255;
        if (k & 1) {
            body[target] = (r << s) | (r >> (8 - s));
        } else {
            body[target] = (r >> s) | (r << (8 - s));
        }
        flags[i] = 1;
    }

    uint8_t sum_data = 0;
    uint8_t xor_data = 0;
    for (int counter = 0; counter < 64; ++counter) {
        sum_data += (body[counter] & 255);
        xor_data ^= (body[counter] & 255);
    }

    return (sum_data == sum_check && xor_data == xor_check);
}

std::vector<uint8_t> decompress_cbg(const uint8_t* crypted, size_t size, uint32_t& out_w, uint32_t& out_h, uint32_t& out_bpp) {
    if (size < 48 || std::memcmp(crypted, "CompressedBG___", 15) != 0) return {};

    const uint8_t* ptr = crypted + 16;
    out_w = read16(&ptr);
    out_h = read16(&ptr);
    out_bpp = read32(&ptr);
    ptr += 12;

    size_t pixel_bytes = (out_bpp / 8);
    size_t total_bytes = out_w * out_h * pixel_bytes;
    std::vector<uint8_t> result(total_bytes, 0);

    size_t header_len = ptr - crypted;
    if (size - header_len >= total_bytes) {
        std::memcpy(result.data(), ptr, total_bytes);
        return result;
    }

    return result;
}

std::vector<uint8_t> extract_audio_payload(const uint8_t* data, size_t size) {
    // If BGI audio with "bw  " header
    if (size >= 8 && std::memcmp(data + 4, "bw  ", 4) == 0) {
        uint32_t hdr_len = 0;
        std::memcpy(&hdr_len, data, 4);
        if (hdr_len > 0 && hdr_len < size) {
            return std::vector<uint8_t>(data + hdr_len, data + size);
        }
    }
    return std::vector<uint8_t>(data, data + size);
}

} // anonymous namespace

DecryptedResult Decryptor::decrypt(const uint8_t* data, size_t size, const std::string& /* entry_name */) {
    DecryptedResult res;

    if (size == 0) return res;

    // 1. DSC FORMAT 1.00
    if (is_dsc_format(data, size)) {
        res.data = decompress_dsc(data, size);
        if (res.data.empty()) return res;

        // Check content of decrypted DSC
        if (res.data.size() >= 28 && std::memcmp(res.data.data(), "BurikoCompiledScriptVer1.00", 27) == 0) {
            res.kind = FileKind::Script;
            res.suggested_ext = ".bss";
            return res;
        }

        // Check if GRP Image
        if (res.data.size() >= 16) {
            uint16_t w, h;
            std::memcpy(&w, res.data.data(), 2);
            std::memcpy(&h, res.data.data() + 2, 2);
            uint16_t bpp;
            std::memcpy(&bpp, res.data.data() + 4, 2);
            if ((bpp == 8 || bpp == 24 || bpp == 32) && w > 0 && w < 8192 && h > 0 && h < 8192) {
                if (res.data.size() == 16 + static_cast<size_t>(w) * h * (bpp / 8)) {
                    res.kind = FileKind::Image;
                    res.suggested_ext = ".bmp";
                    res.width = w;
                    res.height = h;
                    res.bpp = bpp;
                    std::vector<uint8_t> pixels(res.data.begin() + 16, res.data.end());
                    res.data = std::move(pixels);
                    return res;
                }
            }
        }

        // Check if Audio (Ogg / bw)
        if (res.data.size() >= 8 && (std::memcmp(res.data.data() + 4, "bw  ", 4) == 0 || std::memcmp(res.data.data(), "OggS", 4) == 0)) {
            res.kind = FileKind::Audio;
            res.suggested_ext = ".ogg";
            res.data = extract_audio_payload(res.data.data(), res.data.size());
            return res;
        }

        res.kind = FileKind::Raw;
        res.suggested_ext = ".bin";
        return res;
    }

    // 2. CompressedBG___ (CBG)
    if (size >= 48 && std::memcmp(data, "CompressedBG___", 15) == 0) {
        uint32_t w = 0, h = 0, bpp = 0;
        res.data = decompress_cbg(data, size, w, h, bpp);
        if (!res.data.empty() && w > 0 && h > 0) {
            res.kind = FileKind::Image;
            res.suggested_ext = ".bmp";
            res.width = w;
            res.height = h;
            res.bpp = bpp;
            return res;
        }
    }

    // 3. BSE 1.0 (Encrypted Image/Audio header)
    if (size >= 80 && std::memcmp(data, "BSE 1.0", 7) == 0) {
        std::vector<uint8_t> decrypted_copy(data, data + size);
        if (decrypt_bse_in_place(decrypted_copy.data(), size)) {
            res.data.assign(decrypted_copy.begin() + 0x50, decrypted_copy.end());
            res.kind = FileKind::Audio;
            res.suggested_ext = ".ogg";
            return res;
        }
    }

    // 4. Audio check (OggS, bw, RIFF)
    if (size >= 4) {
        if (std::memcmp(data, "OggS", 4) == 0) {
            res.kind = FileKind::Audio;
            res.suggested_ext = ".ogg";
            res.data.assign(data, data + size);
            return res;
        }
        if (size >= 8 && std::memcmp(data + 4, "bw  ", 4) == 0) {
            res.kind = FileKind::Audio;
            res.suggested_ext = ".ogg";
            res.data = extract_audio_payload(data, size);
            return res;
        }
        if (std::memcmp(data, "RIFF", 4) == 0) {
            res.kind = FileKind::Audio;
            res.suggested_ext = ".wav";
            res.data.assign(data, data + size);
            return res;
        }
    }

    // 5. Default raw binary
    res.kind = FileKind::Raw;
    res.suggested_ext = ".bin";
    res.data.assign(data, data + size);
    return res;
}

bool Decryptor::save_as_bmp(const std::string& filepath, uint32_t width, uint32_t height, uint32_t bpp, const uint8_t* pixels) {
    if (width == 0 || height == 0 || (bpp != 24 && bpp != 32) || !pixels) return false;

    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t row_stride = ((width * bpp + 31) / 32) * 4;
    uint32_t image_size = row_stride * height;
    uint32_t file_size = 54 + image_size;

    uint8_t bmp_hdr[14] = {
        'B', 'M',
        static_cast<uint8_t>(file_size & 0xFF),
        static_cast<uint8_t>((file_size >> 8) & 0xFF),
        static_cast<uint8_t>((file_size >> 16) & 0xFF),
        static_cast<uint8_t>((file_size >> 24) & 0xFF),
        0, 0, 0, 0,
        54, 0, 0, 0
    };
    f.write(reinterpret_cast<char*>(bmp_hdr), 14);

    uint8_t dib_hdr[40] = {0};
    uint32_t hdr_sz = 40;
    int32_t h = static_cast<int32_t>(height);
    uint16_t planes = 1;
    uint16_t bit_count = static_cast<uint16_t>(bpp);

    std::memcpy(dib_hdr + 0, &hdr_sz, 4);
    std::memcpy(dib_hdr + 4, &width, 4);
    std::memcpy(dib_hdr + 8, &h, 4);
    std::memcpy(dib_hdr + 12, &planes, 2);
    std::memcpy(dib_hdr + 14, &bit_count, 2);
    std::memcpy(dib_hdr + 20, &image_size, 4);

    f.write(reinterpret_cast<char*>(dib_hdr), 40);

    size_t pixel_bytes = bpp / 8;
    std::vector<uint8_t> row_buf(row_stride, 0);

    for (int y = static_cast<int>(height) - 1; y >= 0; --y) {
        const uint8_t* src_row = pixels + (static_cast<size_t>(y) * width * pixel_bytes);
        if (bpp == 32) {
            for (uint32_t x = 0; x < width; ++x) {
                uint8_t r = src_row[x * 4 + 0];
                uint8_t g = src_row[x * 4 + 1];
                uint8_t b = src_row[x * 4 + 2];
                uint8_t a = src_row[x * 4 + 3];
                row_buf[x * 4 + 0] = b;
                row_buf[x * 4 + 1] = g;
                row_buf[x * 4 + 2] = r;
                row_buf[x * 4 + 3] = a;
            }
        } else if (bpp == 24) {
            for (uint32_t x = 0; x < width; ++x) {
                uint8_t r = src_row[x * 3 + 0];
                uint8_t g = src_row[x * 3 + 1];
                uint8_t b = src_row[x * 3 + 2];
                row_buf[x * 3 + 0] = b;
                row_buf[x * 3 + 1] = g;
                row_buf[x * 3 + 2] = r;
            }
        }
        f.write(reinterpret_cast<char*>(row_buf.data()), row_stride);
    }

    return true;
}

} // namespace bgi
