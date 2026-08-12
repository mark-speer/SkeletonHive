# ARA 2 hosting

Optional Celemony **Audio Random Access** (ARA 2) support for clip-level
analysis/editing (e.g. Melodyne), hosted by Tracktion Engine.

Default builds leave ARA **disabled** so CI and machines without the SDK keep
working.

## Enable at configure time

```bash
git clone --recursive --branch releases/2.3.0 https://github.com/Celemony/ARA_SDK
cmake -B build -DSKELETONHIVE_ENABLE_ARA=ON -DSKELETONHIVE_ARA_SDK_PATH="/path/to/ARA_SDK"
cmake --build build --config Release
```

Configure fails loudly if `SKELETONHIVE_ENABLE_ARA=ON` but the SDK path is
missing or incomplete (expects `ARA_API/ARAInterface.h`).

## Product surface

| Action | Where |
|--------|--------|
| Choose ARA time-stretch mode | Clip inspector stretch combo (ARA listed only when enabled) |
| Open ARA editor | Clip inspector **Open ARA**; clip context menu **Open ARA Editor...** |
| Double-click ARA clip | Opens ARA editor when the clip is already using ARA |
| Convert ARA analysis to MIDI | Clip context menu (when clip is using ARA); distinct from aubio audio→MIDI |

Implementation uses TE APIs (`TimeStretcher::Mode::ara`, `showARAWindow`,
`araConvertToMIDI`) via [`Source/Engine/AraHelpers.*`](../Source/Engine/AraHelpers.h).

## Policy

- **Always in-process.** `PluginHostHelpers::shouldSandboxDescription` returns
  false when `PluginDescription::hasARAExtension` is set.
- **One ARA document per Edit** (TE / Celemony model); no parallel SkeletonHive
  ARA schema.

## Smoke test checklist

Requires an installed ARA 2 plugin (typically Melodyne) and an ARA-enabled build.

1. Scan plugins; confirm the `(ARA)` variant appears in the known plugin list.
2. Drop an audio clip; set stretch mode to **ARA** (or use **Open ARA**).
3. Edit notes/regions in the ARA UI; close and reopen the editor.
4. Save project → reopen → confirm ARA edits restore and playback matches.
5. Export / freeze the track containing the ARA clip; listen for silence or
   missing analysis.
6. With sandbox enabled in Preferences, confirm ARA plugins still load
   in-process (no sandbox worker for ARA descriptions).
