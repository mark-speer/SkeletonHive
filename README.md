# SkeletonHive

Cross-platform arrangement-view digital audio workstation built with **JUCE** and **Tracktion Engine**.

## Features

- Multi-track arrangement timeline (audio + MIDI)
- MIDI sequencing with piano roll editor (velocity, CC, pitch bend, aftertouch lanes)
- Audio and MIDI recording with punch/loop modes, metronome and count-in
- Audio export/bounce to WAV or FLAC
- VST3 plugin hosting (AU on macOS); VST3 effects can run in a crash-isolated sandbox process (Preferences → Devices)
- Mixer with level meters, sends and sidechain routing
- Parameter automation (Read / Touch / Latch) with editable lanes
- Arrangement markers and tempo/time-signature map editing
- Track freeze
- Customizable keyboard shortcuts and preferences (theme, autosave, audio)
- MIDI controller learn for mixer fader/pan parameters; generic control-surface fader bank (CC 1–8 volume, CC 9–16 pan) with session-grid LED feedback
- Audio clip inspector (gain, transpose, reverse, speed, loop) in the arrangement view
- Take lanes and comping for audio/MIDI clips (loop-record takes, comp editing, flatten)
- In-place consolidate (Ctrl/Cmd+J) and flatten-to-audio for device chains
- Linked clip-group editing (inspector + fade curves propagate to grouped peers)
- Plugin hot-swap (Replace…) with state transfer; per-device preset browser with A/B compare
- Default device chains for new audio/MIDI tracks (Preferences → Devices)
- Native instruments: Sampler, Drum Rack, and 4OSC Synth with dedicated editors (waveform regions, mod matrix)
- Sample browser with library scan, hover preview, and drag-to-timeline import (auto-inserts Sampler on empty MIDI tracks)
- Clip library with drag-to-save presets, export-to-library, and Collect All and Save
- Live-style detail panel (Devices | Clip tabs) with roaming track focus across tray, inspector, and automation
- Global groove pool with project persistence; apply to MIDI clip selection (Shift+H) or from Grooves browser tab
- Plugin browser embedded in left browser panel (Plugins tab)
- Session View grid (Tab toggle) with clip slots, scene launch, launch quantization, and browser drag-to-slot import
- Session ↔ Arrangement bridge: Record to Arrangement (Rec>), Capture & Insert (Shift+C), and Commit Loop to Arrangement
- Session performance features: per-slot follow actions, legato launch, MIDI note mapping to slots, and a bottom-docked rack macro performance panel (Alt+P in Session View)
- MIDI clip scale lock (root/scale/Scale Lock in Clip inspector) with session playback filtering for out-of-scale notes
- Per-note probability and iteration lanes in the piano roll; session clips re-roll on each loop cycle
- Session grid virtualization for large track counts (visible-row slot pooling)

## Roadmap

Phases 1–10 and Phase 9 Tier 4 / Phase 11 Tier 1 are implemented. Phase 11 Tiers 2–4 (Push/APC profile, MPE, Link) and Phase 12 remain planned. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (§14–§16).

**Debug stress tests (debug builds only):**
- **Ctrl+Shift+Alt+T** — add 200 MIDI tracks; log Session grid rebuild timing and live slot component count.
- **Ctrl+Shift+Alt+B** — run engine benchmark suite (populate audio tracks, freeze, render); log timings to the debug console.

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
