#include "arc.hpp"
#include <cstring>
#include <iostream>

namespace bgi {

bool ArcReader::is_valid_arc(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    char magic[16];
    f.read(magic, 16);
    if (f.gcount() < 16) return false;

    return std::memcmp(magic, "BURIKO ARC20", 12) == 0;
}

std::unique_ptr<ArcReader> ArcReader::open(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return nullptr;

    char header[16];
    f.read(header, 16);
    if (f.gcount() < 16 || std::memcmp(header, "BURIKO ARC20", 12) != 0) {
        return nullptr;
    }

    uint32_t count = 0;
    std::memcpy(&count, header + 12, 4);

    uint32_t base_offset = 0x10 + count * 0x80;
    std::vector<ArcEntry> entries;
    entries.reserve(count);

    std::vector<uint8_t> entry_raw(0x80);
    for (uint32_t i = 0; i < count; ++i) {
        f.read(reinterpret_cast<char*>(entry_raw.data()), 0x80);
        if (f.gcount() < 0x80) break;

        ArcEntry e;
        // filename is null-terminated within 0x60 bytes
        const char* name_ptr = reinterpret_cast<const char*>(entry_raw.data());
        size_t name_len = 0;
        while (name_len < 0x60 && name_ptr[name_len] != '\0') {
            name_len++;
        }
        e.name = std::string(name_ptr, name_len);

        std::memcpy(&e.offset, entry_raw.data() + 0x60, 4);
        std::memcpy(&e.size, entry_raw.data() + 0x64, 4);

        entries.push_back(std::move(e));
    }

    return std::unique_ptr<ArcReader>(new ArcReader(path, std::move(f), base_offset, std::move(entries)));
}

ArcReader::ArcReader(std::string path, std::ifstream stream, uint32_t base_offset, std::vector<ArcEntry> entries)
    : path_(std::move(path)), stream_(std::move(stream)), base_offset_(base_offset), entries_(std::move(entries)) {}

ArcReader::~ArcReader() = default;

std::vector<uint8_t> ArcReader::read_entry(const ArcEntry& entry) {
    if (!stream_.is_open()) return {};

    uint64_t full_offset = static_cast<uint64_t>(base_offset_) + entry.offset;
    stream_.seekg(full_offset, std::ios::beg);

    std::vector<uint8_t> data(entry.size);
    stream_.read(reinterpret_cast<char*>(data.data()), entry.size);
    if (static_cast<size_t>(stream_.gcount()) != entry.size) {
        data.resize(stream_.gcount());
    }
    return data;
}

std::vector<uint8_t> ArcReader::read_entry_by_index(size_t index) {
    if (index >= entries_.size()) return {};
    return read_entry(entries_[index]);
}

} // namespace bgi
