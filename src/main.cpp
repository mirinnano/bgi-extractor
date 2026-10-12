#include "arc.hpp"
#include "dsc.hpp"
#include "parser.hpp"
#include "presets.hpp"
#include "decompressor.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

enum class Action {
    Decrypt,  // Default: Decrypt and extract resources
    Text,     // Parse scripts and extract dialogue text
    Raw,      // Extract archives without decryption
    List      // List archive contents
};

struct Options {
    std::vector<std::string> inputs;
    std::string output_dir = "./output";
    Action action = Action::Decrypt;

    // Filters
    bool filter_scripts = false;
    bool filter_images = false;
    bool filter_audio = false;

    // Text mode options
    std::string format = "txt"; // "txt" or "json"
    bool include_voice = true;
    bool archive_mode = true;
    bool individual_mode = true;
    bool single_file_mode = true;
    bool use_presets = true;

    bool verbose = false;

    bool matches_filter(bgi::FileKind kind) const {
        if (!filter_scripts && !filter_images && !filter_audio) return true;
        if (filter_scripts && kind == bgi::FileKind::Script) return true;
        if (filter_images && kind == bgi::FileKind::Image) return true;
        if (filter_audio && kind == bgi::FileKind::Audio) return true;
        return false;
    }
};

void print_help(const char* prog_name) {
    std::cout << "bgi-extractor - BGI / Ethornell Engine Decryptor & Extractor\n"
              << "Copyright (c) 2026 mirinnano. MIT License.\n\n"
              << "Usage: " << prog_name << " [options] <game_dir_or_arc_files...>\n\n"
              << "Actions (Output Control):\n"
              << "  -d, --decrypt            Decrypt and unpack all game resources (Default)\n"
              << "                           (Scripts to .bss, Images to .bmp, Audio to .ogg/.wav)\n"
              << "  -t, --text               Parse decrypted scripts and extract dialogue text\n"
              << "  --raw                    Unpack raw archive contents without decryption\n"
              << "  -l, --list               List archive files and encryption types without extraction\n\n"
              << "Resource Filters (for -d/--decrypt):\n"
              << "  --scripts                Extract and decrypt scripts only\n"
              << "  --images                 Extract and decrypt images only\n"
              << "  --audio                  Extract and decrypt audio only\n\n"
              << "Text Extraction Options (for -t/--text):\n"
              << "  -f, --format <txt|json>  Text format: 'txt' or 'json' (default: txt)\n"
              << "  --no-voice               Exclude voice audio IDs from dialogue text\n"
              << "  --no-archive             Skip generating merged archive/chapter text files\n"
              << "  --no-individual          Skip generating individual per-script text files\n"
              << "  --no-single-file         Skip generating full single combined text file\n"
              << "  --no-presets             Disable game-specific title/chapter presets\n\n"
              << "General Options:\n"
              << "  -o, --output <dir>       Output directory (default: ./output)\n"
              << "  -v, --verbose            Show verbose diagnostic logs\n"
              << "  -h, --help               Display this help message\n\n"
              << "Examples:\n"
              << "  " << prog_name << " /path/to/game_dir                 # Decrypt all resources to ./output\n"
              << "  " << prog_name << " --scripts -o ./scripts data*.arc  # Decrypt scripts only (.bss)\n"
              << "  " << prog_name << " -t -f json -o ./text data*.arc    # Extract dialogue text as JSON\n"
              << "  " << prog_name << " -l data01101.arc                  # List contents of archive\n";
}

Options parse_args(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            std::exit(0);
        } else if (arg == "-d" || arg == "--decrypt") {
            opts.action = Action::Decrypt;
        } else if (arg == "-t" || arg == "--text") {
            opts.action = Action::Text;
        } else if (arg == "--raw") {
            opts.action = Action::Raw;
        } else if (arg == "-l" || arg == "--list") {
            opts.action = Action::List;
        } else if (arg == "--scripts") {
            opts.filter_scripts = true;
        } else if (arg == "--images") {
            opts.filter_images = true;
        } else if (arg == "--audio") {
            opts.filter_audio = true;
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

std::vector<std::string> resolve_arc_files(const std::vector<std::string>& inputs, Action act) {
    std::vector<std::string> arc_files;
    for (const auto& in : inputs) {
        if (fs::is_directory(in)) {
            for (const auto& entry : fs::directory_iterator(in)) {
                if (entry.is_regular_file() && entry.path().extension() == ".arc") {
                    std::string fn = entry.path().filename().string();
                    if (act == Action::Text) {
                        // For text extraction, prioritize script archives
                        if (fn.rfind("data01", 0) == 0 || fn.rfind("script", 0) == 0 || fn.rfind("scenario", 0) == 0) {
                            arc_files.push_back(entry.path().string());
                        }
                    } else {
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

    auto arc_files = resolve_arc_files(opts.inputs, opts.action);
    if (arc_files.empty()) {
        std::cerr << "Error: No matching .arc archive files found.\n";
        return 1;
    }

    if (opts.action == Action::List) {
        std::cout << "Listing " << arc_files.size() << " archives:\n\n";
        for (const auto& path_str : arc_files) {
            auto arc = bgi::ArcReader::open(path_str);
            if (!arc) continue;
            std::cout << "=== Archive: " << fs::path(path_str).filename().string() 
                      << " (" << arc->entries().size() << " entries) ===\n";
            std::cout << std::left << std::setw(30) << "Name" 
                      << std::right << std::setw(12) << "Size" 
                      << "  " << std::left << "Type\n";
            std::cout << std::string(55, '-') << "\n";
            for (const auto& entry : arc->entries()) {
                auto data = arc->read_entry(entry);
                auto res = bgi::Decryptor::decrypt(data.data(), data.size(), entry.name);
                std::string type_name = "Raw";
                if (res.kind == bgi::FileKind::Script) type_name = "Script (DSC)";
                else if (res.kind == bgi::FileKind::Image) type_name = "Image (" + std::to_string(res.width) + "x" + std::to_string(res.height) + " " + std::to_string(res.bpp) + "bpp)";
                else if (res.kind == bgi::FileKind::Audio) type_name = "Audio";
                std::cout << std::left << std::setw(30) << entry.name 
                          << std::right << std::setw(12) << entry.size 
                          << "  " << type_name << "\n";
            }
            std::cout << "\n";
        }
        return 0;
    }

    std::cout << "==================================================\n";
    std::cout << " bgi-extractor - BGI Decryptor & Extractor\n";
    std::cout << "==================================================\n";
    std::cout << "Mode:             " << (opts.action == Action::Text ? "Text Extraction" : (opts.action == Action::Raw ? "Raw Unpack" : "Resource Decrypt")) << "\n";
    std::cout << "Target archives:  " << arc_files.size() << " files\n";
    std::cout << "Output directory: " << opts.output_dir << "\n";
    if (opts.action == Action::Text) {
        std::cout << "Text format:      " << opts.format << "\n";
    }
    std::cout << "\n";

    fs::create_directories(opts.output_dir);

    auto start_time = std::chrono::steady_clock::now();
    size_t total_processed = 0;
    size_t total_lines = 0;

    // TEXT EXTRACTION MODE
    if (opts.action == Action::Text) {
        if (opts.archive_mode) fs::create_directories(fs::path(opts.output_dir) / "00_archives");
        if (opts.individual_mode) fs::create_directories(fs::path(opts.output_dir) / "01_scripts");

        std::vector<std::string> all_sections_text;
        std::vector<std::string> all_sections_json;

        for (const auto& arc_path_str : arc_files) {
            fs::path arc_path(arc_path_str);
            std::string arc_name = arc_path.filename().string();
            auto arc = bgi::ArcReader::open(arc_path_str);
            if (!arc) continue;

            std::string sec_folder = arc_path.stem().string();
            std::string sec_title = arc_name;
            if (opts.use_presets) {
                auto preset = bgi::PresetRegistry::lookup_section(arc_name);
                if (preset) {
                    sec_folder = preset->folder_name;
                    sec_title = preset->title;
                }
            }

            std::cout << "Parsing [" << sec_title << "] (" << arc_name << ")... " << std::flush;
            std::vector<std::string> sec_lines_text;
            std::vector<std::string> sec_lines_json;
            size_t arc_script_cnt = 0;
            size_t arc_line_cnt = 0;

            sec_lines_text.push_back("==================================================");
            sec_lines_text.push_back(" " + sec_title + " (" + arc_name + ")");
            sec_lines_text.push_back("==================================================\n");

            for (const auto& entry : arc->entries()) {
                auto crypted = arc->read_entry(entry);
                if (crypted.empty()) continue;

                auto decrypted = bgi::decompress_dsc(crypted.data(), crypted.size());
                if (decrypted.empty() || !bgi::ScriptParser::is_valid_script(decrypted.data(), decrypted.size())) {
                    continue;
                }

                arc_script_cnt++;
                total_processed++;

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
                        for (const auto& line : script_text) out_f << line << "\n";
                    }
                }

                for (const auto& line : script_text) sec_lines_text.push_back(line);
                sec_lines_text.push_back("");
                for (const auto& line : script_json) sec_lines_json.push_back(line);
            }

            std::cout << arc_script_cnt << " scripts, " << arc_line_cnt << " dialogue lines.\n";

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
                    for (const auto& line : sec_lines_text) out_f << line << "\n";
                }
            }

            std::string sec_joined;
            for (const auto& l : sec_lines_text) sec_joined += l + "\n";
            all_sections_text.push_back(sec_joined);
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
                for (const auto& sec_t : all_sections_text) out_f << sec_t << "\n\n";
            }
        }
    }
    // DECRYPT OR RAW UNPACK MODE
    else {
        for (const auto& arc_path_str : arc_files) {
            fs::path arc_path(arc_path_str);
            std::string arc_name = arc_path.filename().string();
            auto arc = bgi::ArcReader::open(arc_path_str);
            if (!arc) continue;

            fs::path out_sub = fs::path(opts.output_dir) / arc_path.stem();
            fs::create_directories(out_sub);

            std::cout << "Extracting [" << arc_name << "] (" << arc->entries().size() << " entries)... " << std::flush;
            size_t arc_extracted = 0;

            for (const auto& entry : arc->entries()) {
                auto raw_data = arc->read_entry(entry);
                if (raw_data.empty()) continue;

                if (opts.action == Action::Raw) {
                    std::ofstream out_f(out_sub / entry.name, std::ios::binary);
                    out_f.write(reinterpret_cast<const char*>(raw_data.data()), raw_data.size());
                    arc_extracted++;
                    total_processed++;
                } else {
                    // Decrypt mode
                    auto res = bgi::Decryptor::decrypt(raw_data.data(), raw_data.size(), entry.name);
                    if (!opts.matches_filter(res.kind)) continue;

                    std::string out_name = entry.name;
                    if (!res.suggested_ext.empty()) {
                        out_name += res.suggested_ext;
                    }

                    if (res.kind == bgi::FileKind::Image && (res.bpp == 24 || res.bpp == 32) && res.width > 0 && res.height > 0) {
                        bgi::Decryptor::save_as_bmp((out_sub / out_name).string(), res.width, res.height, res.bpp, res.data.data());
                    } else {
                        std::ofstream out_f(out_sub / out_name, std::ios::binary);
                        out_f.write(reinterpret_cast<const char*>(res.data.data()), res.data.size());
                    }
                    arc_extracted++;
                    total_processed++;
                }
            }
            std::cout << arc_extracted << " files extracted.\n";
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " Process completed successfully!\n";
    if (opts.action == Action::Text) {
        std::cout << " Total scripts parsed:    " << total_processed << "\n";
        std::cout << " Total dialogue lines:    " << total_lines << "\n";
    } else {
        std::cout << " Total files extracted:   " << total_processed << "\n";
    }
    std::cout << " Elapsed time:            " << std::fixed << std::setprecision(2) << elapsed_ms << " ms\n";
    std::cout << " Output directory:        " << fs::absolute(opts.output_dir).string() << "\n";
    std::cout << "--------------------------------------------------\n";

    return 0;
}
