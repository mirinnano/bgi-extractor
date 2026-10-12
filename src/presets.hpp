#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace bgi {

struct SectionInfo {
    std::string folder_name;
    std::string title;
};

class PresetRegistry {
public:
    static std::optional<SectionInfo> lookup_section(const std::string& arc_filename) {
        static const std::unordered_map<std::string, SectionInfo> subahibi_presets = {
            {"data01101.arc", {"01_Down_the_Rabbit-Hole_1", "第1章 Down the Rabbit-Hole I"}},
            {"data01102.arc", {"01_Down_the_Rabbit-Hole_2", "第1章 Down the Rabbit-Hole II"}},
            {"data01201.arc", {"02_Its_my_own_Invention", "第2章 It's my own Invention"}},
            {"data01301.arc", {"03_Looking-glass_Insects", "第3章 Looking-glass Insects"}},
            {"data01401.arc", {"04_Jabberwocky_1", "第4章 Jabberwocky I"}},
            {"data01501.arc", {"05_Which_Dreamed_It", "第5章 Which Dreamed It"}},
            {"data01601.arc", {"06_Jabberwocky_2", "第6章 Jabberwocky II"}},
            {"data01701.arc", {"07_素晴らしき日々", "第7章 素晴らしき日々 (Knockin' on heaven's door)"}},
            {"data01801.arc", {"08_Extra", "Extra"}},
            {"data01900.arc", {"09_Scenes", "各種サブシーン・エピソード"}},
            {"data01999.arc", {"10_Ending", "エンディング"}},
            {"data01000.arc", {"99_Dev_Test", "開発テストスクリプト"}}
        };

        auto it = subahibi_presets.find(arc_filename);
        if (it != subahibi_presets.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

} // namespace bgi
