# SkeletonHive Roadmap

Product delivery log and backlog. This is **not** the architecture document —
see [ARCHITECTURE.md](ARCHITECTURE.md) for system shape, ownership, threading,
and invariants.

UX benchmark: Ableton Live 12–class workflows. Implementation benchmark:
idiomatic JUCE + Tracktion Engine (engine owns model/undo/graph; UI is a thin
view).

---

## Status summary

| Phase | Status | Focus | Live 12 analogy |
|-------|--------|-------|-----------------|
| 1 | Implemented | Foundation, arrangement, piano roll, routing | Core edit workflows |
| 2 | Implemented | Workflow polish, performance infra | Groups, ripple, racks |
| 3 | Implemented | LOD, multi-out, sidechain, plugin hardening | Device chain depth |
| 4 | Implemented | Export, record, automation, markers, freeze | Production loop |
| 5 | Implemented | Shortcuts, prefs, theme, MIDI learn | Control surface |
| 6 | Implemented | MIDI CC lanes, clip inspector, comping, consolidate/flatten | Detail View, Take Lanes |
| 7 | Implemented | Unified browser, hot-swap, presets, clip library, groove pool, detail stack | Browser, Hot-Swap |
| 8 | Tier 1–4 done | Session grid, scenes, launch quantize, arrangement bridge, performance, scale/probability, virtualization | Session View |
| 9 | Tier 1–4 done | Native instruments/effects foundation (built-in synth, sampler, drum rack, 4OSC editor) | Simpler, Sampler, Drum Rack, Operator |
| 10 | Implemented | Warp engine, native effects rack, audio-to-MIDI, VST3 effect sandbox (MVP), engine benchmark harness | Warp markers, Live's built-in device library, crash isolation |
| 11 | Tier 1 done | Generic control-surface API, Push/APC-style profile, MPE, Ableton Link | Push/APC integration, MPE, Link |
| 12 | Planned | Version history, collaboration scoping, light-theme finish, accessibility audit | Project polish |

**Suggested order for remaining work:** finish Phase 11 (after sandbox
hardening where hardware sessions need isolation) → Phase 12 (lowest
architectural coupling; can slot in opportunistically).

---

## Delivered (Phases 1–10, condensed)

Phases 1–10 shipped the arrangement/session studio loop on TE:

- **Foundation** — undo-safe Edit mutations, Edit lifetime rebuild on
  New/Open, change-driven UI, tempo-aware timeline, piano roll, mixer,
  plugin scan/insert.
- **Workflow depth** — ripple, fades, folders/returns, clip groups, racks,
  sidechain, multi-out, timeline LOD/virtualization, waveform/lane caches.
- **Production loop** — export, metronome/count-in/punch, automation panel,
  arm/record, markers, tempo map, freeze, autosave/crash recovery.
- **Control / UX** — prefs, themes, command manager, MIDI learn.
- **Detail editing** — MIDI CC/PB/AT lanes, clip inspector, takes/comping,
  consolidate/flatten, warp markers.
- **Browser & Session** — sample/clip/groove browsers, hot-swap/presets,
  Session View + arrangement bridge, follow actions, scale/probability.
- **Native content & robustness** — TE built-ins + dedicated editors, custom
  effects, audio→MIDI (aubio), VST3 **effect** sandbox MVP, benchmark harness.

Subsystem entry points and architectural limits (sandbox scope, rack rewiring,
multi-out taps, session launch model) are documented in
[ARCHITECTURE.md](ARCHITECTURE.md).

---

## Phase 11 — Control Surfaces, MPE & Live Ecosystem Interop

**Theme:** Live's hardware-first, expressive-performance workflow.

- **Tier 1 — Generic MIDI control-surface API (implemented).**
  `ControlSurfaceManager` extends MIDI learn / `ParameterControlMappings` with
  project-persisted bindings (`CONTROLSTATE` under `EDITVIEWSTATE`): fader-bank
  volume/pan (8 tracks per bank), transport play/stop, scene/slot launch, and
  session-grid LED feedback via `juce::MidiOutput`. A generic fader-bank script
  installs on first project load when no bindings exist.

- **Tier 2 — Push/APC-style hardware profile (planned).** Mapping layer over
  the existing session grid slot-state model — largely a hardware-facing view
  rather than a new engine surface.

- **Tier 3 — MPE end-to-end (planned).** Per-note pitch-bend/pressure in the
  piano roll; gated on what `te::MidiNote` exposes.

- **Tier 4 — Ableton Link (planned).** Tempo/transport sync with other apps on
  top of centralised `TransportControl` / tempo-sequence timing.

**Files (Tier 1):** `Source/Engine/ControlSurfaceManager.*`, extensions to
`MidiLearnController`. Planned: `AbletonLinkBridge`, hardware profile layer.

**Risks:** Prefer stronger plugin isolation before heavy hardware/MPE
performance sessions. Tier 3 depends on TE's `MidiNote` API surface.

---

## Phase 12 — Collaboration, Scale & Finish

**Theme:** product polish with low architectural coupling; pick up
opportunistically.

- **Tier 1 — Project version history.** Browsable timeline beyond rotating
  `Autosave/` snapshots.
- **Tier 2 — Collaboration scoping.** No live co-editing; shared/exportable
  project packages only.
- **Tier 3 — Light-theme finish.** Complete `AppLookAndFeel` / `AppColours`
  coverage across panels.
- **Tier 4 — Accessibility and performance audit.** Screen-reader support,
  scalable UI, closing profiling pass against the Phase 10 baseline.

**Files:** extensions to `ProjectManager`, theme resources; no new core
subsystems expected.

---

## Backlog — ARA 2 hosting (optional build)

**Theme:** Celemony ARA 2 clip analysis/editing (Melodyne-class) via Tracktion
Engine's existing host stack.

- **Tier 1 — Build + clip MVP (implemented, opt-in).**
  `-DSKELETONHIVE_ENABLE_ARA=ON` + `SKELETONHIVE_ARA_SDK_PATH` wires
  `TRACKTION_ENABLE_ARA` / `JUCE_PLUGINHOST_ARA`. Clip inspector and context
  menu open TE's ARA editor; ARA plugins are never sandboxed. See
  [ARA.md](ARA.md) for SDK setup and smoke tests.
- **Tier 2 — Hardening (planned).** Offline render/freeze of ARA clips, Session
  View copy/parking of ARA archive state, optional CI job with SDK present.

**Files:** `CMakeLists.txt`, `Engine/AraHelpers.*`, `PluginHostHelpers`,
`ClipInspectorPanel`, arrangement clip menu.

**Risks:** In-process only; requires licensed ARA plugin for real QA.

---

## Debug stress helpers (debug builds)

- **Ctrl+Shift+Alt+T** — add 200 MIDI tracks; log Session grid rebuild timing
  and live slot component count.
- **Ctrl+Shift+Alt+B** — engine benchmark suite (populate audio tracks, freeze,
  render); log timings to the debug console.
