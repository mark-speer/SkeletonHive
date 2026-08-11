# SkeletonHive Architecture

SkeletonHive is a cross-platform arrangement- and session-view DAW built on
**JUCE** and **Tracktion Engine (TE)**. Ableton Live is the interaction
benchmark (fast editing, predictable snap, keyboard-first). TE owns the
edit model, undo stack, and audio graph; the application UI is a thin,
change-driven view over that model.

For delivery history and remaining product work, see [ROADMAP.md](ROADMAP.md).
For rules that must not be broken when changing code, see
[CONTRIBUTING.md](CONTRIBUTING.md).

---

## Runtime shape

One product binary (`SkeletonHive`) serves three roles, selected at startup
from the command line (`Source/App/Main.cpp`):

```
Host DAW (normal launch)
  SkeletonHiveApplication
    ├─ te::Engine
    ├─ ProjectManager          ← sole owner of te::Edit
    └─ MainWindow / MainContentComponent

Same executable, child roles
  ├─ Plugin scanner child      (TE --PluginScan:…)
  └─ VST3 effect sandbox worker (--SkeletonHivePluginHost:…)
```

The host never loads a second competing audio engine. Scanner and sandbox
reuse the same binary for deployment simplicity and crash isolation.

---

## Layering and ownership

| Layer | Responsibility | Location |
|-------|----------------|----------|
| App shell | Engine lifetime, settings, look-and-feel, window | `Source/App/` |
| Engine-facing helpers | Mutations, routing, persistence, session model, plugin host | `Source/Engine/` |
| UI | Views and controllers bound to the live Edit | `Source/UI/` |
| Tracktion Engine | Model, graph, devices, render, undo | FetchContent / vendored TE |

**Edit ownership.** Only `ProjectManager` owns `te::Edit`. Every UI panel that
holds an `Edit&` is destroyed before a project swap and recreated after:

`releaseEditUI()` → replace Edit → `rebuildEditUI()`.

Do not cache an `Edit&` (or raw pointers into Edit-owned objects) in anything
that outlives a New/Open.

**Undo.** User-visible mutations go through `edit.getUndoManager()`. Passing
`nullptr` for undo on an interactive edit is a bug.

**UI boundary.** UI code talks to TE objects or `EngineHelpers`. It does not
touch `juce::AudioDeviceManager` or the playback graph directly.

**Change-driven UI.** Components listen to `ValueTree` / transport /
parameter notifications and coalesce with async updaters. Timers are reserved
for continuously moving displays (playhead, meters, time readout).

---

## Core subsystems

| Concern | Role | Entry points |
|---------|------|--------------|
| Transport | Facade over `te::TransportControl` | `TransportController`, `UI/Transport/` |
| Arrangement | Timeline view over TE tracks/clips; zoom/scroll/snap in `EditViewState` | `UI/Arrangement/` |
| Session | Clip slots / scenes as additive state; launch via TE transport APIs | `SessionManager`, `UI/Session/` |
| Mixer / routing | Volume/pan, aux send/return, sidechain, multi-out | `UI/Mixer/`, `SidechainRouting`, `MultiOutputRouting` |
| Plugins | Scan, insert, tray/rack UI, native catalog, optional VST3-effect sandbox | `PluginScanner`, `NativePluginCatalog`, `Engine/PluginHost/`, `UI/Plugins/` |
| MIDI editing | Piano roll and CC/PB/AT lanes over `te::MidiList` | `UI/Midi/` |
| Automation | Lanes and Read/Touch/Latch over TE automation | `UI/Automation/` |
| Persistence | Save/load, autosave, lock file, crash recovery | `ProjectManager` |
| Export / freeze | Offline render and track freeze via TE | `ExportManager`, `ExtendedUIBehaviour` |
| Content browser | Samples, clips, grooves, preview, DnD | `ContentLibraryManager`, `UI/Browser/` |
| Control | MIDI learn and generic control-surface bank | `MidiLearnController`, `ControlSurfaceManager` |

View/session preferences that must persist with the project live under the
Edit's `EDITVIEWSTATE` child (`EditViewState`), not in free-floating component
members.

---

## Threading model

Three domains matter:

1. **Message / UI thread** — ValueTree edits, undo, plugin insert/reorder,
   session scheduling, most IPC, editor open/close.
2. **TE audio / device callback** — TE owns the main graph. The app does not
   install a competing device callback for arrangement playback.
3. **Sandbox worker** — optional out-of-process VST3 **effects**: shared-memory
   audio round-trip on a realtime path; load/state/editor on the worker
   message thread; host-side watchdog on the message thread.

Hard rules:

- No locks, heap allocations, or `ValueTree` access on the audio thread
  (except the documented sandbox SHM/atomics path).
- Heavy work (scan, export, freeze, audio→MIDI) runs off the UI thread with
  progress reporting through `ExtendedUIBehaviour`.
- Session clip launch is quantized on the message thread using TE transport
  APIs — not a custom audio-thread scheduler.

Playhead and meters read TE-safe caches from a single UI telemetry timer
(`UiTelemetryHub`); they never drive the graph.

---

## Key data flows

### Audio

```
Device I/O (via TE)
  → te::Edit playback graph
       ├─ In-process plugins (natives, AU, VST3 instruments, non-sandboxed effects)
       └─ SandboxedPluginInstance (optional VST3 effects → SHM → worker)
  → Master / device output
```

### Plugin load

```
PluginScanner (background) → KnownPluginList (+ persistent blacklist)
Insert → EngineHelpers / TE PluginManager
  └─ if sandbox pref + VST3 + not instrument
       → SandboxedPluginInstance → same-exe worker
```

### Project save / load

```
Open:  lock check → optional .tmp recover → load Edit → setupEdit → rebuild UI
Save:  rotating Autosave/ snapshots → EditFileOperations::save
Timer: saveTempVersion only (never overwrites the project file)
```

---

## Intentional TE deviations

TE has no first-class concept for several Live-like workflows. SkeletonHive
stores those as **additive** `skeletonHive*` (and related) properties on
existing Edit `ValueTree` state — for example track-kind badges, clip groups,
session slots, scale lock, note probability, and control-surface bindings.

Rules for these extensions:

- They persist with the Edit and participate in undo where they represent
  user edits.
- They must not gate TE engine behaviour (routing, playback graph, render).
  Presentation and workflow only, unless a dedicated helper explicitly
  documents otherwise (e.g. session clip parking/activation).
- Prefer extending this pattern over inventing a parallel model or file format.

---

## Known limits and risks

These are architectural constraints, not temporary UI polish:

- **Plugin sandbox is an MVP.** VST3 effects only. Instruments, AU, sidechain /
  multi-out through the bridge, full embedded editor forwarding, and host-side
  automation passthrough remain incomplete. Treat expansion as high-risk work.
- **Rack serial graph.** Reordering or hot-swapping inside TE racks updates
  ValueTree/display order; it does not always rewire serial audio connections.
- **Multi-out taps.** Bus layout and child-track wiring exist; per-bus audio
  separation is limited by TE's current graph behaviour.
- **Session launching** depends on TE transport + message-thread quantization,
  not a dedicated clip-launcher DSP path. Timing and scale assumptions differ
  from Live's engine.
- **Native sampler depth** is bounded by TE `SamplerPlugin` (e.g. ADSR / held
  loop playback gaps).

A DAW at this scale stays correct only if humans keep owning these boundaries.
See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## Tracktion Engine dependency policy

TE is pinned (`GIT_TAG` in `CMakeLists.txt`) and patched via checked-in files
under [`cmake/patches/`](../cmake/patches/), applied by
[`cmake/apply_tracktion_patches.cmake`](../cmake/apply_tracktion_patches.cmake).

- Never hand-edit `build/_deps/`. Those trees are disposable.
- If a patch fails to apply after a pin bump, fix the patch or the pin —
  configure must fail loudly rather than silently skip.

---

## Related docs

- [ROADMAP.md](ROADMAP.md) — phase status and remaining backlog
- [CONTRIBUTING.md](CONTRIBUTING.md) — invariants and placement rules
- [README.md](../README.md) — build, features, packaging
