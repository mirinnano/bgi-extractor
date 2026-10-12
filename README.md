# bgi-extractor

Fast, standalone CLI tool for **decrypting, unpacking, and extracting resources** (scripts, images, audio, dialogues) from **BGI / Ethornell** visual novel engine archives.

[日本語のドキュメント (Japanese README)](#日本語ドキュメント)

---

## Overview

`bgi-extractor` is a modern C++17 command-line tool designed to decrypt and unpack resources from games built with the **BGI (Buriko General Interpreter) / Ethornell** engine.

It handles proprietary archive packing, compression, encryption algorithms, and bytecode parsing without any external dependencies.

### Supported Resource Types & Formats
- **Archives**: `BURIKO ARC20` (`.arc`)
- **Scripts**: `DSC FORMAT 1.00` (Adaptive Huffman + LZSS compression) decrypted to `.bss` bytecode.
- **Images**: `CompressedBG___` (CBG) and `DSC` GRP images decrypted and saved as standard `.bmp`.
- **Audio**: `BSE 1.0` encryption and `bw  ` audio wrappers unpacked to playable `.ogg` (Ogg Vorbis) and `.wav`.
- **Dialogue / Text**: Full opcode-level disassembler that correlates speakers, dialogue text, choices, and voice IDs into clean `.txt` or `.json`.

---

## Features

- **Zero External Dependencies**: Standard C++17 only. No third-party image or audio libraries needed.
- **Blazing Fast**: Decrypts archives and thousands of resources in milliseconds.
- **Comprehensive Output Control**:
  - **Decrypt Mode (`-d`)**: Decrypt all resources to native formats (`.bss`, `.bmp`, `.ogg`, `.wav`).
  - **Text Mode (`-t`)**: Parse scripts into dialogue text with speaker names and voice IDs.
  - **Raw Mode (`--raw`)**: Unpack archives without decryption.
  - **List Mode (`-l`)**: Inspect archive contents, dimensions, audio types, and encryption.
- **Granular Filters**: Extract only scripts (`--scripts`), images (`--images`), or audio (`--audio`).
- **Cross-Platform**: Tested on macOS (Apple Silicon & Intel), Linux, and Windows (MSVC).

---

## Building from Source

### Prerequisites
- C++17 compliant compiler (Clang 10+, GCC 9+, or MSVC 2019+)
- GNU Make or CMake 3.15+

### Build with Make (macOS / Linux)
```bash
git clone https://github.com/mirinnano/bgi-extractor.git
cd bgi-extractor
make -j
```

### Build with CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Usage

```bash
bgi-extractor [options] <game_directory_or_arc_files...>
```

### Actions (Output Control)

| Option | Description |
|---|---|
| `-d, --decrypt` | **Default**: Decrypt and unpack all resources (`.bss`, `.bmp`, `.ogg`, `.wav`) |
| `-t, --text` | Parse scripts and extract dialogue text with speakers, choices, and voice IDs |
| `--raw` | Unpack raw archive contents without decryption |
| `-l, --list` | List archive files, sizes, and encryption types without extraction |

### Resource Filters (for `-d`)

| Option | Description |
|---|---|
| `--scripts` | Extract and decrypt script files only (`.bss`) |
| `--images` | Extract and decrypt image files only (`.bmp`) |
| `--audio` | Extract and decrypt audio files only (`.ogg` / `.wav`) |

### Text Options (for `-t`)

| Option | Description |
|---|---|
| `-f, --format <txt|json>` | Text output format: `txt` or `json` (default: `txt`) |
| `--no-voice` | Exclude voice audio IDs from dialogue text |
| `--no-archive` | Skip generating merged archive text files |
| `--no-individual` | Skip generating individual per-script text files |
| `--no-single-file` | Skip generating full consolidated text file |
| `--no-presets` | Disable game-specific chapter/title presets |

### General Options

| Option | Description |
|---|---|
| `-o, --output <dir>` | Output directory (default: `./output`) |
| `-v, --verbose` | Show verbose diagnostic logging |
| `-h, --help` | Display help and usage information |

---

## Examples

### 1. Decrypt All Resources in an Archive or Directory
```bash
# Decrypt all resources to ./output
./bgi-extractor -o ./output /path/to/game_directory

# Decrypt images only from graphics archive
./bgi-extractor --images -o ./images sysgrp.arc

# Decrypt voice / BGM audio only
./bgi-extractor --audio -o ./bgm data05000.arc
```

### 2. Inspect Archive Contents
```bash
./bgi-extractor -l data01101.arc sysgrp.arc
```

Output:
```text
=== Archive: sysgrp.arc (338 entries) ===
Name                            Size  Type
-------------------------------------------------------
SGCG100000                      8299  Image (426x42 32bpp)
SGCG200000                     40246  Image (1280x960 32bpp)
...
```

### 3. Extract Dialogue Text
```bash
# Extract scenario text as plain text
./bgi-extractor -t -o ./text /path/to/game_directory

# Extract scenario text as JSON
./bgi-extractor -t -f json -o ./json_text data01101.arc
```

Text Output format:
```text
【由岐】「間宮卓司……」 (yuki_000003)
【卓司】「なんでこんな所に水上さんがいるの？」 (taku_000001)
　街の喧噪の中に立っている時とは打って変わって、ここはとても静かだ……。
[選択肢] キスする
[選択肢] そんな事出来ない
```

---

<a name="日本語ドキュメント"></a>
## 日本語ドキュメント

BGI / Ethornell エンジンのゲームアーカイブから、暗号化リソース（スクリプト、画像、音声）を**復号して展開・出力**する高速なC++ CLIツールです。引数指定により、復号展開、シナリオテキスト抽出、生データ展開、リソース一覧表示など多彩な出力制御が可能です。

### 主な機能
- **リソースの自動復号展開 (`-d`)**:
  - スクリプト: `DSC FORMAT 1.00` を復号して `.bss` バイトコードで保存
  - 画像: `CBG` / `GRP` 形式を復号して標準の `.bmp` で保存
  - 音声: `BSE` 暗号化および `bw` ヘッダを解除して純粋な `.ogg` / `.wav` で保存
- **シナリオテキスト抽出 (`-t`)**:
  - スクリプト内のオペコードを解析し、セリフ、話者名、ボイスID、選択肢を整形して `.txt` または `.json` で出力。
- **リソースフィルタ**:
  - `--scripts`（スクリプトのみ）、`--images`（画像のみ）、`--audio`（音声のみ）で必要なアセットだけを高速抽出可能。
- **外部依存ゼロ**: C++17標準ライブラリのみで完結。

---

## License

This project is licensed under the [MIT License](LICENSE).
