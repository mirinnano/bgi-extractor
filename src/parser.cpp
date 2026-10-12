#include "parser.hpp"
#include "encoding.hpp"
#include <cstring>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <sstream>

namespace bgi {

namespace {

inline uint32_t read_u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

inline int32_t read_i32(const uint8_t* p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

bool has_non_ascii(const std::string& s) {
    for (unsigned char c : s) {
        if (c > 127) return true;
    }
    return false;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    return s.compare(0, prefix.size(), prefix) == 0;
}

std::string clean_text(std::string s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i+1] == 'n') { out += '\n'; i++; }
            else if (s[i+1] == 'r') { i++; }
            else if (s[i+1] == 't') { out += '\t'; i++; }
            else if (s[i+1] == '"') { out += '"'; i++; }
            else if (s[i+1] == '\\') { out += '\\'; i++; }
            else { out += s[i]; }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // anonymous namespace

std::string DialogueItem::format(bool include_voice) const {
    std::string v = (include_voice && !voice.empty()) ? (" (" + voice + ")") : "";
    if (type == ItemType::Choice) {
        return "[選択肢] " + text;
    } else if (!speaker.empty()) {
        return "【" + speaker + "】" + text + v;
    } else {
        return text + v;
    }
}

std::string DialogueItem::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"" << (type == ItemType::Choice ? "choice" : "line") << "\"";
    if (!speaker.empty()) {
        ss << ",\"speaker\":\"";
        for (char c : speaker) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else if (c == '\n') ss << "\\n";
            else ss << c;
        }
        ss << "\"";
    }
    ss << ",\"text\":\"";
    for (char c : text) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\n') ss << "\\n";
        else ss << c;
    }
    ss << "\"";
    if (!voice.empty()) {
        ss << ",\"voice\":\"" << voice << "\"";
    }
    ss << "}";
    return ss.str();
}

bool ScriptParser::is_valid_script(const uint8_t* data, size_t size) {
    if (size < 0x20) return false;
    return std::memcmp(data, "BurikoCompiledScriptVer1.00\0", 28) == 0;
}

struct Instruction {
    uint32_t addr = 0;
    uint32_t op = 0;
    uint32_t arg_u32 = 0;
    int32_t arg_i32 = 0;
    std::string str_val;
};

std::vector<DialogueItem> ScriptParser::parse(const uint8_t* data, size_t size) {
    if (!is_valid_script(data, size)) return {};

    uint32_t hdr_extra = read_u32(data + 0x1C);
    uint32_t hdrsize = 0x1C + hdr_extra;
    if (hdrsize >= size) return {};

    const uint8_t* code = data + hdrsize;
    size_t code_len = size - hdrsize;

    // Find code end: position after last 0x01B (ret) instruction
    size_t code_end = code_len;
    for (size_t i = 0; i + 4 <= code_len; i += 4) {
        if (code[i] == 0x1B && code[i+1] == 0 && code[i+2] == 0 && code[i+3] == 0) {
            code_end = i + 4;
        }
    }

    std::vector<Instruction> instructions;
    size_t pos = 0;

    auto get_str_at = [&](uint32_t offset) -> std::string {
        if (offset >= code_len) return {};
        const char* s_ptr = reinterpret_cast<const char*>(code + offset);
        size_t slen = 0;
        while (offset + slen < code_len && s_ptr[slen] != '\0') {
            slen++;
        }
        std::string raw(s_ptr, slen);
        return clean_text(cp932_to_utf8(raw));
    };

    while (pos < code_end) {
        uint32_t addr = static_cast<uint32_t>(pos);
        if (pos + 4 > code_len) break;
        uint32_t op = read_u32(code + pos);
        pos += 4;

        Instruction inst;
        inst.addr = addr;
        inst.op = op;

        if (op == 0x003) { // push_string
            if (pos + 4 > code_len) break;
            uint32_t str_off = read_u32(code + pos);
            pos += 4;
            inst.str_val = get_str_at(str_off);
        } else if (op == 0x07F) { // line(file, line)
            if (pos + 8 > code_len) break;
            pos += 8;
        } else if (op == 0x000 || op == 0x001 || op == 0x002 || op == 0x008 || op == 0x009 || op == 0x00A || op == 0x019 || op == 0x03F) {
            if (pos + 4 > code_len) break;
            inst.arg_u32 = read_u32(code + pos);
            inst.arg_i32 = read_i32(code + pos);
            pos += 4;
        }
        instructions.push_back(std::move(inst));
    }

    std::unordered_set<uint32_t> consumed_str_addrs;
    std::map<uint32_t, std::string> voices;

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& inst = instructions[i];
        if (inst.op == 0x1A5) { // snd_::f_1a5() (voice play)
            for (int j = static_cast<int>(i) - 1; j >= std::max(0, static_cast<int>(i) - 6); --j) {
                if (instructions[j].op == 0x003) {
                    voices[inst.addr] = instructions[j].str_val;
                    consumed_str_addrs.insert(instructions[j].addr);
                    break;
                }
            }
        } else if (inst.op == 0x14B) { // msg_::f_14b() (ruby)
            int count = 0;
            for (int j = static_cast<int>(i) - 1; j >= std::max(0, static_cast<int>(i) - 6); --j) {
                if (instructions[j].op == 0x003) {
                    consumed_str_addrs.insert(instructions[j].addr);
                    count++;
                    if (count >= 2) break;
                }
            }
        }
    }

    std::vector<DialogueItem> results;
    std::string current_voice;

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& inst = instructions[i];
        auto it_v = voices.find(inst.addr);
        if (it_v != voices.end()) {
            current_voice = it_v->second;
        }

        if (inst.op == 0x164 || inst.op == 0x168) {
            for (int j = std::max(0, static_cast<int>(i) - 40); j < static_cast<int>(i); ++j) {
                const auto& prev = instructions[j];
                if (prev.op == 0x003 && consumed_str_addrs.find(prev.addr) == consumed_str_addrs.end()) {
                    if (has_non_ascii(prev.str_val) && !ends_with(prev.str_val, ".bss") && !ends_with(prev.str_val, ".mpg")) {
                        DialogueItem d;
                        d.type = ItemType::Choice;
                        d.text = prev.str_val;
                        results.push_back(std::move(d));
                        consumed_str_addrs.insert(prev.addr);
                    }
                }
            }
        } else if (inst.op == 0x140 || inst.op == 0x143) {
            std::vector<std::pair<uint32_t, std::string>> candidates;
            for (int j = static_cast<int>(i) - 1; j >= std::max(0, static_cast<int>(i) - 10); --j) {
                const auto& prev = instructions[j];
                if (consumed_str_addrs.find(prev.addr) != consumed_str_addrs.end()) continue;
                if (prev.op == 0x003) {
                    candidates.insert(candidates.begin(), {prev.addr, prev.str_val});
                } else if ((prev.op >= 0x140 && prev.op < 0x160) || (prev.op >= 0x160 && prev.op < 0x180)) {
                    break;
                }
            }

            std::vector<std::pair<uint32_t, std::string>> filtered;
            for (const auto& c : candidates) {
                if (has_non_ascii(c.second) || starts_with(c.second, "「") || starts_with(c.second, "『")) {
                    filtered.push_back(c);
                }
            }

            if (filtered.size() == 1) {
                DialogueItem d;
                d.type = ItemType::Line;
                d.text = filtered[0].second;
                d.voice = current_voice;
                results.push_back(std::move(d));
                consumed_str_addrs.insert(filtered[0].first);
                current_voice.clear();
            } else if (filtered.size() >= 2) {
                std::string speaker = filtered[filtered.size() - 2].second;
                std::string text = filtered.back().second;

                if (starts_with(speaker, "「") || speaker.size() > text.size()) {
                    std::swap(speaker, text);
                }

                DialogueItem d;
                d.type = ItemType::Line;
                d.speaker = speaker;
                d.text = text;
                d.voice = current_voice;
                results.push_back(std::move(d));

                for (const auto& item : filtered) {
                    consumed_str_addrs.insert(item.first);
                }
                current_voice.clear();
            }
        }
    }

    return results;
}

} // namespace bgi
