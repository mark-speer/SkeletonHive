# SkeletonHive

Cross-platform arrangement-view digital audio workstation built with **JUCE** and **Tracktion Engine**.

## Features

- Multi-track arrangement timeline (audio + MIDI)
- MIDI sequencing with piano roll editor
- Audio and MIDI recording with punch/loop modes
- VST3 plugin hosting (AU on macOS)
- Mixer with level meters
- Parameter automation (Read / Touch / Latch)
- Project save/load (`.tracktionedit` format)

## Requirements

- CMake 3.22+
- C++20 compiler (MSVC 2022, Xcode 15+, GCC 11+ / Clang 14+)
- Git (for dependency fetch)

### Windows
- Visual Studio 2022 with "Desktop development with C++"
- Optional: ASIO driver for low-latency audio

### macOS
- Xcode 15+

### Linux
- `build-essential`, `libasound2-dev`, `libfreetype6-dev`, `libx11-dev`, `libxrandr-dev`, `libxcursor-dev`, `libxinerama-dev`, `libgl1-mesa-dev`

## Build

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows with Visual Studio 2022 or 2026:

```powershell
# Visual Studio 2026 (VS 18)
cmake -B build -G "Visual Studio 18 2026" -A x64

# Visual Studio 2022 (VS 17)
cmake -B build -G "Visual Studio 17 2022" -A x64

cmake --build build --config Release
```

Run:

```bash
./build/SkeletonHive_artefacts/Release/SkeletonHive.exe   # Windows
./build/SkeletonHive_artefacts/Release/SkeletonHive        # macOS/Linux
```

## Plugin paths

| OS | Default VST3 scan paths |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3` |
| macOS | `~/Library/Audio/Plug-Ins/VST3`, `/Library/Audio/Plug-Ins/VST3` |
| Linux | `~/.vst3`, `/usr/lib/vst3`, `/usr/local/lib/vst3` |

## License

This project uses Tracktion Engine (GPLv3 or commercial) and JUCE (separate license required for closed-source distribution). See [Tracktion Engine LICENCE.md](https://github.com/Tracktion/tracktion_engine/blob/develop/LICENCE.md) and [JUCE licensing](https://juce.com/licensing).
