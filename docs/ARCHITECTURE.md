# Arrange DAW — Architecture and Modernization Roadmap

This document captures the findings of the full-codebase architecture review
(July 2026), the decisions made during the Phase 1 modernization pass, the
Tracktion Engine (TE) alignment guidelines the code now follows, and the
backlog for Phases 2 and 3.

The benchmark for interaction design is **Ableton Live**: fast editing
workflows, minimal clicks, immediate drag feedback, predictable snapping, and
keyboard-driven operation. The benchmark for implementation is **idiomatic
JUCE + Tracktion Engine**: let the engine own the model, undo, and audio
graph; keep the UI a thin, change-driven view.

---

## 1. System overview

```
ArrangeApplication (juce::JUCEApplication)
 ├─ te::Engine                     (ExtendedUIBehaviour / ExtendedEngineBehaviour)
 ├─ ProjectManager                 (owns te::Edit, te::SelectionManager, autosave)
 └─ MainWindow
     └─ MainContentComponent      (composition root for all edit-bound UI)
         ├─ TransportController / TransportBar        [TransportManager]
         ├─ TimelineComponent                          [TimelineEngine + ClipManager]
         │    ├─ EditViewState    (zoom/scroll/grid/snap state, per-Edit ValueTree)
         │    ├─ TimelineGrid     (tempo-aware grid drawing + snapping, shared)
         │    ├─ TimelineRulerComponent (bars ruler, loop brace, scrubbing)
         │    ├─ TrackHeader/Footer/LaneComponent (per track)
         │    │    └─ ClipComponent / AudioClipComponent / MidiClipComponent
         │    └─ PlayheadOverlay
         ├─ PianoRollEditor                             [MidiEditor]
         ├─ MixerPanel → ChannelStrip (+ master strip)  [RoutingGraph UI]
         ├─ PluginBrowser / PluginWindow / PluginScanner [PluginRack]
         └─ AutomationLaneComponent
```

Mapping to the target subsystem names from the design brief:

| Target subsystem          | Realised by                                          |
|---------------------------|------------------------------------------------------|
| TransportManager          | `TransportController` + `TransportBar` over `te::TransportControl` |
| TimelineEngine            | `EditViewState` + `TimelineGrid` + `TimelineComponent` |
| ClipManager               | TE's `ClipTrack`/`Clip` + `EngineHelpers` (duplicate, group) |
| TrackManager              | TE's `TrackList`; `EngineHelpers::getOrInsert…` helpers |
| MidiEditor                | `PianoRollEditor` (notes edited via `te::MidiList`)  |
| RoutingGraph              | TE plugin graph; `AuxSendPlugin`/`AuxReturnPlugin` via `EngineHelpers` |
| PluginRack                | Track footer plugin slots + `PluginBrowser`/`PluginScanner` |
| WaveformCache             | `te::SmartThumbnail` (dedicated LRU cache deferred to Phase 2) |
| UndoRedoSystem            | `te::Edit::getUndoManager()` — single source of truth |
| ProjectPersistenceLayer   | `ProjectManager` over `te::EditFileOperations`       |

### Ownership and lifetime rules

- `te::Edit` is owned by `ProjectManager` only. **Every UI panel that holds a
  `te::Edit&` is destroyed and recreated when the Edit is replaced.**
  `MainContentComponent::releaseEditUI()` runs *before* the project swap (so
  destructors can unregister listeners from a live Edit) and
  `rebuildEditUI()` runs after. Never cache an `Edit&` in a component that
  outlives project switches.
- Components register `ValueTree`/`ChangeBroadcaster`/parameter listeners in
  their constructor and **must remove them in their destructor** (see
  `TransportBar::~TransportBar`, `ChannelStrip::~ChannelStrip`).
- Clips/notes are referenced by `Ptr` (reference-counted) or by their
  `ValueTree` state — never by raw pointer across async boundaries. The piano
  roll stores its selection as note `ValueTree`s so it survives object
  reallocation and undo/redo.

---

## 2. Tracktion Engine alignment — rules the code now follows

These were the main "fighting the framework" patterns found in the review and
how they were resolved. They double as guidelines for future work.

1. **All model mutations route through the Edit's UndoManager.**
   Every `ValueTree::setProperty` / `MidiList` / `MidiNote` mutation passes
   `&edit.getUndoManager()`. Ctrl+Z / Ctrl+Shift+Z (and Cmd equivalents) are
   wired in `MainContentComponent::keyPressed` via `edit.undo()/redo()`.
   *Never pass `nullptr` for an undo manager on a user-visible edit.*

2. **Use TE's quantisation, not hand-rolled math.**
   `PianoRollEditor::quantiseNotes()` uses `te::QuantisationType` +
   `roundBeatToNearest`, mapping the current grid interval to the nearest
   named quantise type.

3. **Tempo/time-signature math goes through `edit.tempoSequence`.**
   `TimelineGrid` iterates real bars via `toBarsAndBeats`/`toTime` so the
   lane grid, the ruler, and snapping are all derived from the same
   tempo-sequence evaluation. Snapping is anchored to the start of the bar
   the time falls in, matching the drawn grid across time-sig changes.

4. **Sends/returns use TE's internal plugins.**
   `EngineHelpers::getOrCreateReturnTrack()` (an `AudioTrack` hosting
   `te::AuxReturnPlugin`) and `getOrCreateAuxSend()` (a `te::AuxSendPlugin`
   inserted just before the track's `VolumeAndPanPlugin`). No custom routing
   code; the engine builds the graph.

5. **The master bus uses `edit.getMasterVolumePlugin()` /
   `getMasterPluginList()`** — exposed as a dedicated `ChannelStrip` in the
   mixer rather than omitted.

6. **Plugin chain operations use TE APIs**: `te::Plugin::setEnabled()` for
   bypass, `PluginList` state `moveChild` (with undo manager) for reorder,
   `Plugin::deleteFromParent()` for removal.

7. **Change-driven UI, not polling.** Components listen to the relevant
   `ValueTree`s (`te::ValueTreeAllEventListener`), `te::AutomatableParameter`
   listeners (fader/pan), or the transport `ChangeBroadcaster`, and coalesce
   updates with `FlaggedAsyncUpdater`. The only timers left are for
   continuously-moving displays (playhead, meters, time readout), which is
   the correct pattern.

8. **Selection uses `te::SelectionManager`** for clips/plugins/tracks. The
   piano roll keeps a local note selection (note `ValueTree`s) because notes
   are sub-clip objects; this mirrors the SelectionManager idiom (prune on
   external change, survive undo) without forcing note objects to be
   `Selectable`.

9. **Known deviation — `arrangeTrackKind` property.** TE `AudioTrack`s are
   content-agnostic; the persisted `arrangeTrackKind` property is UI sugar
   for the MIDI/AUDIO badge and drag-to-create defaults, with the clip-type
   inspection fallback in `EngineHelpers::getTrackKind`. It must never gate
   engine behaviour, only presentation. Candidate for removal in Phase 2 in
   favour of pure clip-type inference.

10. **Known deviation — `arrangeClipGroup` property.** Minimal clip grouping
    is a custom `ValueTree` property (`arrangeClipGroup`, a shared UUID) plus
    group-aware drag in `ClipComponent`. TE has no first-class clip-group
    concept, so this is additive, undo-safe, and persists with the edit.

---

## 3. What Phase 1 changed (implemented in this pass)

### Foundation / correctness
- Real undo/redo keyboard shortcuts; every previously-`nullptr` undo-manager
  argument fixed (piano roll was the main offender).
- Fixed a dangling-pointer crash in the piano roll right-click delete path
  (note deleted, then dereferenced).
- Fixed stale `te::Edit&` references after New/Open Project:
  `releaseEditUI()` → swap Edit → `rebuildEditUI()`, and `TransportBar` now
  unregisters its transport listener in its destructor.
- Removed the dead `TransportController::onTransportChanged` callback and its
  duplicated polling path; `TransportController` is now a plain command
  facade over `te::TransportControl`.
- Plugin scanner now scans **all** configured paths and **all** enabled
  plugin formats (VST3/AU/…), not just the first VST3 directory; the scan
  runs on a background thread pool with progress reporting and dead-man's
  pedal handling.
- Plugin insertion goes to the end of the user chain (before the volume
  plugin) instead of always index 0.
- Removed dead code (`AudioProcessorEditorContentComp`, unused RootItem
  forward declaration).

### Arrangement view
- **Virtualization**: clip components are culled against the visible
  viewport (plus a half-viewport margin) in
  `TrackLaneComponent::updateClipBounds`; grid/bar painting is clipped to the
  dirty region so paint cost is O(visible), not O(timeline length).
- **Zoom without rebuilds**: horizontal zoom (Ctrl+wheel, anchored at the
  cursor) and vertical zoom (Ctrl+Shift+wheel, track height 36–240 px)
  re-layout existing components; track components are only rebuilt when the
  track list itself changes.
- **Independent vertical scroll**: the previously-dead `viewY` state is wired
  to the timeline viewport and the header viewport is kept in sync.
- **Unified grid**: one tempo-aware implementation for lane backgrounds, grid
  lines, ruler and snapping (see §2.3).
- **Loop brace**: draggable loop start/end handles and loop-region move in
  the ruler, bound to `transport.getLoopRange()/setLoopRange()`; clicking
  elsewhere in the ruler scrubs the playhead (snapped; Alt bypasses snap).
- **Clip duplication**: Ctrl+D duplicates the selection after itself;
  Ctrl+drag tears off a copy (duplicate stays at origin). Implemented by
  cloning clip state with a fresh `EditItemID` (TE's own split-clip pattern).
- **Minimal clip grouping**: Ctrl+G assigns a shared group id,
  Ctrl+Shift+G ungroups; dragging any member moves the whole group by the
  same snapped delta; grouped clips show a corner tag. This is the first
  step toward ripple editing (Phase 2).
- Delete/Backspace removes selected clips.

### Piano roll (rewritten)
- Multi-select: click, Shift-click add, Ctrl-click toggle, marquee drag on
  empty space; selection survives undo/redo via note-state tracking.
- Distinct **move vs resize** drag modes with edge handles and hover
  cursors; double-click creates a note (one grid interval) and immediately
  enters resize mode; right-click deletes.
- Snapping reuses `TimelineGrid` intervals (Alt bypasses, snap toggle
  respected) instead of independent proportional math.
- **Velocity lane**: per-note bars; drag a selected note's bar to set the
  whole selection, or paint across bars pencil-style.
- **Ghost notes** from other MIDI clips on the same track, drawn dimmed.
- **Scale highlighting** (root + major/minor) and **fold to active notes**.
- **Quantize** (TE `QuantisationType`) and **humanize** (±10% grid timing
  jitter, ±10 velocity) on the selection or, when nothing is selected, all
  notes.
- Keyboard: Delete, Ctrl+A, Ctrl+D duplicate-after-selection, arrow-key
  nudge (±grid / ±semitone, Shift+up/down = octave), Q/H/F, Escape.
- **MIDI audition** on click/create/pitch-drag via
  `te::AudioTrack::playGuideNote`.
- Region-based repaints (grid vs velocity lane) instead of full-component
  repaint per edit.

### Routing / mixer / plugin rack
- Track footer plugin slots: click opens the plugin window; right-click menu
  for **bypass** (`setEnabled`), **move earlier/later** (undoable
  `moveChild`), **remove**. Slots grey out when bypassed and rebuild on
  plugin list changes.
- **Master bus strip** in the mixer (volume/pan from the master volume
  plugin, meter from the master plugin list).
- **Aux send/return**: "+Send" on a channel strip creates/finds "Return A"
  and inserts a pre-fader `AuxSendPlugin`; the strip then shows a send-level
  slider. Return tracks don't get sends on themselves.
- Channel strips are **two-way bound**: fader/pan follow
  `volParam`/`panParam` listeners (so automation and plugin edits reflect),
  mute/solo follow track-state `ValueTree` changes, and the mixer rebuilds
  when tracks are added/removed.

---

## 4. Performance notes

Current state and the reasoning behind it:

- **Audio thread**: the app contains no custom audio-thread code — all DSP
  and graph processing is TE's. The UI communicates with playback through
  TE's own thread-safe APIs (`TransportControl`, `AutomatableParameter`,
  `LevelMeasurer::getLevelCache`). Keep it that way: never add locks,
  allocations, or `ValueTree` access on the audio thread.
- **Paint cost**: grid drawing is clipped to `Graphics::getClipBounds`;
  off-screen clip components are `setVisible(false)` so they neither paint
  nor hit-test. Waveforms use `te::SmartThumbnail` (background-generated,
  cached).
- **Change coalescing**: `ValueTree` bursts (e.g. drag = many property
  changes) are coalesced by `FlaggedAsyncUpdater` into one rebuild/layout on
  the message thread.
- **Timers**: playhead 30 Hz, meters 30 Hz, transport readout 15 Hz. These
  repaint tiny regions (`PlayheadOverlay` repaints a 2 px strip).
- Known remaining costs (acceptable at current scale, tracked for Phase 2):
  full lane `repaint()` on zoom, `MixerPanel` full rebuild on any track
  add/remove, `MidiClipComponent` re-renders its note preview every paint.

---

## 5. Phase 2 backlog (documented, not implemented)

Workflow / editing:
- **Ripple editing** across all clips on a track (foundation: clip grouping
  and `EngineHelpers::duplicateClip` already exist).
- Clip **fade handles** and crossfades (TE `fadeIn`/`fadeOut` properties).
- **Folder/Group/Return track types** surfaced as distinct wrappers
  (`te::FolderTrack` exists; return tracks currently plain audio tracks with
  an `AuxReturnPlugin`). Remove the `arrangeTrackKind` property in favour of
  pure clip-type inference at the same time.
- Clip grouping **persistence semantics** (group-aware duplicate/delete,
  nested groups) and group colour.
- **Scale-aware MIDI input** and highlight-scale note entry in the piano
  roll; groove/humanize templates beyond uniform jitter.
- Piano roll: horizontal zoom/scroll within long clips, note-length
  drawing tools, chord/step input.

Routing / devices:
- **Macro system** via `te::RackType` macro parameters; instrument and audio
  effect **racks** (device containers).
- **Drag-and-drop plugin reordering** (slots are click/menu only today) and
  drag from browser onto tracks/clips.
- Multiple send buses (A/B/C…), pre/post fader toggle per send
  (`AuxPosition`), sidechain source pickers (TE supports sidechain inputs on
  plugins).
- Wet/dry on device chain, solo-device monitoring.

Performance / infrastructure:
- Dedicated **WaveformCache** LRU layer above `SmartThumbnail` for very
  large projects.
- Audit and formalize **lock-free UI↔engine messaging** for meters/positions
  (currently TE's built-ins; fine, but should be measured at 200+ tracks).
- **OpenGL-accelerated timeline** rendering; cached lane images invalidated
  by dirty time-ranges instead of full repaints on zoom.
- Track virtualization (lanes are laid out but all created; cull lane
  *components* for 500+ track sessions).
- Incremental `MixerPanel` updates instead of full rebuilds.

## 6. Phase 3 backlog

- VST3-specific sandboxing / quirks handling (out-of-process scanning is
  already in place via TE's child-process scanner).
- Sidechain routing UI with full source matrix.
- Multi-output instrument routing (drum-sampler style output pairs to child
  tracks).
- Adaptive timeline level-of-detail (LOD) rendering: summaries at far zoom,
  full detail near.
- Collaborative / session-safe persistence (edit-file merge strategy,
  autosave versioning).

---

## 7. Conventions for contributors

- UI code lives under `Source/UI/…`, engine-facing helpers under
  `Source/Engine/…`. UI never talks to `juce::AudioDeviceManager` or the
  graph directly — always through TE objects or `EngineHelpers`.
- Any new mutating operation must: (a) pass the Edit's `UndoManager`,
  (b) be reachable by mouse *and* keyboard where sensible, (c) update the UI
  via change notification, never by direct cross-component calls.
- Prefer `te::` APIs over reimplementation. If TE has a class whose name
  matches what you're about to write, read it first
  (`build/_deps/tracktion_engine-src/modules/tracktion_engine/…`).
- New persistent view state goes in `EditViewState` (per-Edit `ValueTree`
  child `EDITVIEWSTATE`), not in component members.
