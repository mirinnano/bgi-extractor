#include "arc.hpp"
#include "dsc.hpp"
#include "parser.hpp"
#include "presets.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

struct Options {
    std::vector<std::string> inputs;
    std::string output_dir = "./extracted_text";
    std::string format = "txt"; // "txt" or "json"
    bool include_voice = true;
    bool archive_mode = true;
    bool individual_mode = true;
    bool single_file_mode = true;
    bool use_presets = true;
    bool raw_dump = false;
    bool verbose = false;
};

void print_help(const char* prog_name) {
    std::cout << "bgi-extractor - BGI / Ethornell Engine Script Text Extractor\n"
              << "Copyright (c) 2026 mirinnano. MIT License.\n\n"
              << "Usage: " << prog_name << " [options] <game_dir_or_arc_files...>\n\n"
              << "Arguments:\n"
              << "  <inputs...>              Path to game directory or individual .arc archive files\n\n"
              << "Options:\n"
              << "  -o, --output <dir>       Output directory (default: ./extracted_text)\n"
              << "  -f, --format <txt|json>  Output format: 'txt' or 'json' (default: txt)\n"
              << "  --no-voice               Exclude voice IDs from dialogue lines\n"
              << "  --no-archive             Do not generate per-archive combined files\n"
              << "  --no-individual          Do not generate individual script files\n"
              << "  --no-single-file         Do not generate full single combined file\n"
              << "  --no-presets             Disable game-specific title/chapter presets\n"
              << "  --raw                    Dump decompressed raw bytecode scripts (.bss)\n"
              << "  -v, --verbose            Show verbose parsing logs\n"
              << "  -h, --help               Display this help message\n\n"
              << "Examples:\n"
              << "  " << prog_name << " /path/to/game_directory\n"
              << "  " << prog_name << " -o ./output data01101.arc data01102.arc\n"
              << "  " << prog_name << " -f json -o ./json_out /path/to/game\n";
}

Options parse_args(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            std::exit(0);
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) opts.output_dir = argv[++i];
        } else if (arg == "-f" || arg == "--format") {
            if (i + 1 < argc) opts.format = argv[++i];
        } else if (arg == "--no-voice") {
            opts.include_voice = false;
        } else if (arg == "--no-archive") {
            opts.archive_mode = false;
        } else if (arg == "--no-individual") {
            opts.individual_mode = false;
        } else if (arg == "--no-single-file") {
            opts.single_file_mode = false;
        } else if (arg == "--no-presets") {
            opts.use_presets = false;
        } else if (arg == "--raw") {
            opts.raw_dump = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (!arg.empty() && arg[0] != '-') {
            opts.inputs.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            std::exit(1);
        }
    }
    return opts;
}

std::vector<std::string> resolve_arc_files(const std::vector<std::string>& inputs) {
    std::vector<std::string> arc_files;
    for (const auto& in : inputs) {
        if (fs::is_directory(in)) {
            for (const auto& entry : fs::directory_iterator(in)) {
                if (entry.is_regular_file() && entry.path().extension() == ".arc") {
                    std::string fn = entry.path().filename().string();
                    // Process script archives or all archives if valid
                    if (fn.rfind("data01", 0) == 0 || fn.rfind("script", 0) == 0 || fn.rfind("scenario", 0) == 0) {
                        arc_files.push_back(entry.path().string());
                    }
                }
            }
        } else if (fs::is_regular_file(in)) {
            arc_files.push_back(in);
        }
    }
    std::sort(arc_files.begin(), arc_files.end());
    return arc_files;
}

int main(int argc, char* argv[]) {
    Options opts = parse_args(argc, argv);

    if (opts.inputs.empty()) {
        std::cerr << "Error: No input files or directories specified.\n\n";
        print_help(argv[0]);
        return 1;
    }

    auto arc_files = resolve_arc_files(opts.inputs);
    if (arc_files.empty()) {
        std::cerr << "Error: No matching .arc archive files found.\n";
        return 1;
    }

    std::cout << "==================================================\n";
    std::cout << " bgi-extractor - BGI Script Text Extractor\n";
    std::cout << "==================================================\n";
    std::cout << "Target archives: " << arc_files.size() << " files\n";
    std::cout << "Output directory: " << opts.output_dir << "\n";
    std::cout << "Output format:    " << opts.format << "\n\n";

    fs::create_directories(opts.output_dir);
    if (opts.archive_mode) fs::create_directories(fs::path(opts.output_dir) / "00_archives");
    if (opts.individual_mode) fs::create_directories(fs::path(opts.output_dir) / "01_scripts");
    if (opts.raw_dump) fs::create_directories(fs::path(opts.output_dir) / "02_raw_scripts");

    size_t total_scripts = 0;
    size_t total_lines = 0;
    std::vector<std::string> all_sections_text;
    std::vector<std::string> all_sections_json;

    auto start_time = std::chrono::steady_clock::now();

    for (const auto& arc_path_str : arc_files) {
        fs::path arc_path(arc_path_str);
        std::string arc_name = arc_path.filename().string();

        auto arc = bgi::ArcReader::open(arc_path_str);
        if (!arc) {
            if (opts.verbose) {
                std::cerr << "Skipping non-BURIKO ARC file: " << arc_name << "\n";
            }
            continue;
        }

        std::string sec_folder = arc_path.stem().string();
        std::string sec_title = arc_name;

        if (opts.use_presets) {
            auto preset = bgi::PresetRegistry::lookup_section(arc_name);
            if (preset) {
                sec_folder = preset->folder_name;
                sec_title = preset->title;
            }
        }

        std::cout << "Processing [" << sec_title << "] (" << arc_name << ")... " << std::flush;

        std::vector<std::string> sec_lines_text;
        std::vector<std::string> sec_lines_json;

        sec_lines_text.push_back("==================================================");
        sec_lines_text.push_back(" " + sec_title + " (" + arc_name + ")");
        sec_lines_text.push_back("==================================================\n");

        size_t arc_script_cnt = 0;
        size_t arc_line_cnt = 0;

        for (const auto& entry : arc->entries()) {
            auto crypted = arc->read_entry(entry);
            if (crypted.empty()) continue;

            auto decrypted = bgi::decompress_dsc(crypted.data(), crypted.size());
            if (decrypted.empty() || !bgi::ScriptParser::is_valid_script(decrypted.data(), decrypted.size())) {
                continue;
            }

            arc_script_cnt++;
            total_scripts++;

            if (opts.raw_dump) {
                fs::path raw_dir = fs::path(opts.output_dir) / "02_raw_scripts" / sec_folder;
                fs::create_directories(raw_dir);
                std::ofstream raw_f(raw_dir / (entry.name + ".bss"), std::ios::binary);
                raw_f.write(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
            }

            auto dialogues = bgi::ScriptParser::parse(decrypted.data(), decrypted.size());
            arc_line_cnt += dialogues.size();
            total_lines += dialogues.size();

            std::vector<std::string> script_text;
            std::vector<std::string> script_json;

            script_text.push_back("--- [" + sec_title + "] " + entry.name + " ---");
            script_text.push_back("Lines: " + std::to_string(dialogues.size()) + "\n");

            for (const auto& item : dialogues) {
                script_text.push_back(item.format(opts.include_voice));
                script_json.push_back(item.to_json());
            }

            if (opts.individual_mode) {
                fs::path ind_dir = fs::path(opts.output_dir) / "01_scripts" / sec_folder;
                fs::create_directories(ind_dir);

                if (opts.format == "json") {
                    std::ofstream out_f(ind_dir / (entry.name + ".json"));
                    out_f << "[\n";
                    for (size_t i = 0; i < script_json.size(); ++i) {
                        out_f << "  " << script_json[i] << (i + 1 < script_json.size() ? ",\n" : "\n");
                    }
                    out_f << "]\n";
                } else {
                    std::ofstream out_f(ind_dir / (entry.name + ".txt"));
                    for (const auto& line : script_text) {
                        out_f << line << "\n";
                    }
                }
            }

            for (const auto& line : script_text) sec_lines_text.push_back(line);
            sec_lines_text.push_back("");
            for (const auto& line : script_json) sec_lines_json.push_back(line);
        }

        std::cout << arc_script_cnt << " scripts, " << arc_line_cnt << " lines.\n";

        if (opts.archive_mode) {
            fs::path arch_dir = fs::path(opts.output_dir) / "00_archives";
            if (opts.format == "json") {
                std::ofstream out_f(arch_dir / (sec_folder + ".json"));
                out_f << "[\n";
                for (size_t i = 0; i < sec_lines_json.size(); ++i) {
                    out_f << "  " << sec_lines_json[i] << (i + 1 < sec_lines_json.size() ? ",\n" : "\n");
                }
                out_f << "]\n";
            } else {
                std::ofstream out_f(arch_dir / (sec_folder + ".txt"));
                for (const auto& line : sec_lines_text) {
                    out_f << line << "\n";
                }
            }
        }

        std::string sec_joined_text;
        for (const auto& l : sec_lines_text) sec_joined_text += l + "\n";
        all_sections_text.push_back(sec_joined_text);

        for (const auto& j : sec_lines_json) all_sections_json.push_back(j);
    }

    if (opts.single_file_mode) {
        if (opts.format == "json") {
            std::ofstream out_f(fs::path(opts.output_dir) / "all_dialogues.json");
            out_f << "[\n";
            for (size_t i = 0; i < all_sections_json.size(); ++i) {
                out_f << "  " << all_sections_json[i] << (i + 1 < all_sections_json.size() ? ",\n" : "\n");
            }
            out_f << "]\n";
        } else {
            std::ofstream out_f(fs::path(opts.output_dir) / "all_dialogues.txt");
            for (const auto& sec_t : all_sections_text) {
                out_f << sec_t << "\n\n";
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " Extraction complete!\n";
    std::cout << " Total scripts extracted: " << total_scripts << "\n";
    std::cout << " Total dialogue lines:   " << total_lines << "\n";
    std::cout << " Elapsed time:           " << std::fixed << std::setprecision(2) << elapsed_ms << " ms\n";
    std::cout << " Output saved to:        " << fs::absolute(opts.output_dir).string() << "\n";
    std::cout << "--------------------------------------------------\n";

    return 0;
}
