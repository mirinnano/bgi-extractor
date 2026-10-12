# bgi-extractor

Fast, standalone CLI tool for extracting scenario scripts, dialogues, character names, choices, and voice IDs from **BGI / Ethornell** visual novel engine games.

[日本語のドキュメント (Japanese README)](#日本語ドキュメント)

---

## Features

- **Zero External Dependencies**: Built purely with C++17. No heavy third-party libraries required.
- **Blazing Fast**: Decrypts and disassembles over 60,000+ lines of script in under 300 ms.
- **Intelligent Dialogue Parsing**:
  - Automatically correlates character speakers with dialogue lines.
  - Links voice audio file IDs (e.g. `yuki_000001`) to corresponding spoken lines.
  - Extracts interactive choices (`[選択肢]`).
  - Filters out engine control codes, graphics commands, BGM triggers, sound effects, and furigana (ruby) definitions.
- **Multiple Output Formats**: Supports clean, human-readable plain text (`.txt`) and structured JSON (`.json`).
- **Flexible Organization**: Generates all-in-one consolidated text, per-archive chapter texts, and individual per-script files.
- **Cross-Platform**: Fully tested on macOS (Apple Silicon / Intel), Linux, and Windows (MSVC).

---

## Tested Games & Compatibility

Designed for visual novel titles running on the **BGI (Buriko General Interpreter) / Ethornell** engine:

- **Archive formats**: `BURIKO ARC20`
- **Compression / Encryption**: `DSC FORMAT 1.00` (Adaptive Huffman + LZSS compression)
- **Bytecode**: `BurikoCompiledScriptVer1.00`

### Verified Titles:
- *素晴らしき日々 ～不連続存在～ 15th Anniversary Edition* (*Subarashiki Hibi 15th*)
- Works with standard BGI / Ethornell script archives (`data01*.arc`, `script.arc`, etc.).

---

## Building from Source

### Prerequisites
- C++17 compliant compiler (Clang 10+, GCC 9+, or MSVC 2019+)
- CMake 3.15+ (or simply GNU Make)

### Build with CMake (Recommended)
```bash
git clone https://github.com/mirinnano/bgi-extractor.git
cd bgi-extractor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The compiled executable will be located at `build/bgi-extractor` (or `build/Release/bgi-extractor.exe` on Windows).

### Build with Make (macOS / Linux)
```bash
make -j
```

---

## Usage

```bash
bgi-extractor [options] <game_directory_or_arc_files...>
```

### Command-line Options

| Option | Description |
|---|---|
| `-o, --output <dir>` | Specify output directory (default: `./extracted_text`) |
| `-f, --format <txt|json>` | Output format: `txt` or `json` (default: `txt`) |
| `--no-voice` | Do not include voice audio IDs in the extracted output |
| `--no-archive` | Skip generating merged archive/chapter text files |
| `--no-individual` | Skip generating individual per-script text files |
| `--no-single-file` | Skip generating the single all-in-one consolidated text file |
| `--no-presets` | Disable built-in game chapter/title presets |
| `--raw` | Dump decrypted raw script bytecode files (`.bss`) |
| `-v, --verbose` | Enable verbose diagnostic logging |
| `-h, --help` | Display help and usage information |

### Examples

**Extract entire game directory:**
```bash
./bgi-extractor -o ./output /path/to/game_directory
```

**Extract specific archives with JSON output:**
```bash
./bgi-extractor -f json -o ./json_output data01101.arc data01102.arc
```

---

## Output Examples

### Plain Text (`.txt`)
```text
【由岐】「間宮卓司……」 (yuki_000003)
【卓司】「なんでこんな所に水上さんがいるの？」 (taku_000001)
【由岐】「私が何処にいようが関係ない……」 (yuki_000006)
　街の喧噪の中に立っている時とは打って変わって、ここはとても静かだ……。
[選択肢] キスする
[選択肢] そんな事出来ない
```

### Structured JSON (`.json`)
```json
[
  {
    "type": "line",
    "speaker": "由岐",
    "text": "「間宮卓司……」",
    "voice": "yuki_000003"
  },
  {
    "type": "line",
    "speaker": "卓司",
    "text": "「なんでこんな所に水上さんがいるの？」",
    "voice": "taku_000001"
  },
  {
    "type": "line",
    "text": "　街の喧噪の中に立っている時とは打って変わって、ここはとても静かだ……。"
  },
  {
    "type": "choice",
    "text": "キスする"
  }
]
```

---

<a name="日本語ドキュメント"></a>
## 日本語ドキュメント

BGI / Ethornell エンジンのゲームアーカイブから、シナリオテキスト、セリフ、話者名、ボイスID、選択肢を高精度に抽出する高速なC++ CLIツールです。

### 特徴
- **外部ライブラリ不要**: C++17標準ライブラリのみで完結。
- **超高速処理**: 6万行以上のシナリオ全編を約0.3秒で解析・抽出。
- **高精度なテキスト判定**: オペコードをバイトコードレベルで逆アセンブルし、ルビ定義や画像・演出コマンドなどのシステム制御コードを綺麗に除外。
- **ボイスID・話者名の紐付け**: `【話者名】「セリフ」 (ボイスID)` の形式で抽出。
- **TXT / JSON 対応**: 機械処理（自然言語処理・翻訳・スクリプト分析等）に便利なJSONフォーマットもサポート。

---

## License

This project is licensed under the [MIT License](LICENSE).
