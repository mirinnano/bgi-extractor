#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

namespace bgi {

struct ArcEntry {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
};

class ArcReader {
public:
    static bool is_valid_arc(const std::string& path);
    static std::unique_ptr<ArcReader> open(const std::string& path);

    ~ArcReader();

    const std::string& path() const { return path_; }
    const std::vector<ArcEntry>& entries() const { return entries_; }

    std::vector<uint8_t> read_entry(const ArcEntry& entry);
    std::vector<uint8_t> read_entry_by_index(size_t index);

private:
    ArcReader(std::string path, std::ifstream stream, uint32_t base_offset, std::vector<ArcEntry> entries);

    std::string path_;
    std::ifstream stream_;
    uint32_t base_offset_ = 0;
    std::vector<ArcEntry> entries_;
};

} // namespace bgi
