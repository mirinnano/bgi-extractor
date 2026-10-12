#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace bgi {

enum class ItemType {
    Line,
    Choice
};

struct DialogueItem {
    ItemType type = ItemType::Line;
    std::string speaker;
    std::string text;
    std::string voice;

    std::string format(bool include_voice = true) const;
    std::string to_json() const;
};

class ScriptParser {
public:
    static bool is_valid_script(const uint8_t* data, size_t size);
    static std::vector<DialogueItem> parse(const uint8_t* data, size_t size);
};

} // namespace bgi
