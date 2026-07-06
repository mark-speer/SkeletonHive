# SkeletonHive — Architecture and Modernization Roadmap

This document captures the findings of the full-codebase architecture review
(July 2026), the decisions made during the Phase 1 modernization pass, the
Tracktion Engine (TE) alignment guidelines the code now follows, the
implemented Phases 1–5, and the planned backlog for Phases 6–8.

The benchmark for interaction design is **Ableton Live**: fast editing
workflows, minimal clicks, immediate drag feedback, predictable snapping, and
keyboard-driven operation. The benchmark for implementation is **idiomatic
JUCE + Tracktion Engine**: let the engine own the model, undo, and audio
graph; keep the UI a thin, change-driven view.

---

## 1. System overview

```
SkeletonHiveApplication (juce::JUCEApplication)
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
| PluginRack                | Track footer slots + **PluginTrayComponent** (bottom device chain) + `PluginBrowser`/`PluginScanner` |
| WaveformCache             | `WaveformCache` LRU service sharing `te::SmartThumbnail` per `AudioFile` hash |
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

9. **Known deviation — `skeletonHiveTrackKind` property.** TE `AudioTrack`s are
   content-agnostic; the persisted `skeletonHiveTrackKind` property is UI sugar
   for the MIDI/AUDIO badge and drag-to-create defaults, with the clip-type
   inspection fallback in `EngineHelpers::getTrackKind`. It must never gate
   engine behaviour, only presentation. Candidate for removal in Phase 2 in
   favour of pure clip-type inference.

10. **Known deviation — clip group properties.** Two-tier grouping uses custom
    `ValueTree` properties: `skeletonHiveClipGroup` (inner group) and
    `skeletonHiveClipGroupOuter` (super-group spanning multiple inner groups), plus
    optional `skeletonHiveClipGroupColour`. Group-aware move, resize, duplicate,
    and cross-track moves use `EngineHelpers::getGroupedPeers()`. TE has no
    first-class clip-group concept; this is additive, undo-safe, and persists with
    the edit.

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
- **UI telemetry (lock-free audit)**: a single `UiTelemetryHub` timer (30 Hz)
  drives playhead position updates and level-meter repaints. Meters read
  `LevelMeasurer::getLevelCache()` (dB → gain conversion in `LevelMeter`);
  playhead reads `transport.getPosition()`. No custom queues or atomics —
  TE owns thread safety on the audio side.
- **WaveformCache**: `AudioClipComponent` acquires shared `SmartThumbnail`
  instances from an LRU cache keyed by `AudioFile` hash. Thumbnails are held
  only while clips are visible (horizontal culling releases them; cache evicts
  LRU entries with ref-count 1).
- **Lane backgrounds**: `LaneBackgroundCache` rasterises grid/bar backgrounds
  into `juce::Image` tiles keyed by zoom/view/track height. Zoom invalidates
  the cache instead of repainting every clip. `TimelineOpenGLRenderer` can blit
  the same cached images via OpenGL textures when attached.
- **Track virtualization**: `TimelineComponent` keeps a lightweight
  `TrackRowInfo` list for all tracks but only instantiates lane/header/footer
  components for rows intersecting the vertical viewport (± margin).
- **Mixer**: `MixerPanel` incrementally adds/removes/reorders `ChannelStrip`
  instances on track list changes (`FlaggedAsyncUpdater`); master strip persists.
- **Clip paint**: horizontal clip culling unchanged; `MidiClipComponent` caches
  note previews to an `Image` invalidated on note `ValueTree` changes.
- **Change coalescing**: `ValueTree` bursts are coalesced by
  `FlaggedAsyncUpdater` into one rebuild/layout on the message thread.
- **Timers**: transport readout remains 15 Hz in `TransportBar`; playhead and
  meters share the 30 Hz `UiTelemetryHub`.

---

## 5. Phase 2 status

### Implemented (workflow / routing — prior commits + this pass)

- Ripple editing, clip fade handles, folder/return tracks, clip grouping with
  colour, scale-aware MIDI input, piano roll zoom/draw/step/chord tools,
  multi-send buses, plugin DnD, racks/macros, sidechain picker, wet/dry,
  solo-device monitoring.

### Implemented (performance / infrastructure — this pass)

- **`WaveformCache`** — LRU shared `SmartThumbnail` layer (`Source/Engine/WaveformCache.*`).
- **`LaneBackgroundCache`** — CPU grid/bar tile cache (`Source/UI/Arrangement/LaneBackgroundCache.*`).
- **`TimelineOpenGLRenderer`** — optional OpenGL blit of lane cache textures (`juce_opengl`).
- **Vertical track virtualization** — visible-range lane pooling in `TimelineComponent`.
- **Incremental `MixerPanel`** — per-track strip add/remove/reorder without full rebuild.
- **`UiTelemetryHub`** — consolidated 30 Hz playhead + meter polling.

### Implemented (Phase 2 polish)

- **Nested clip groups** — inner (`skeletonHiveClipGroup`) + outer
  (`skeletonHiveClipGroupOuter`) tiers; Ctrl+G creates inner or outer group as
  appropriate; Ctrl+Shift+G removes one level; dual corner tags.
- **Grouped resize** — resize start/end on any grouped member shifts all peers by
  the same edge delta; ripple exclusion respects inner and outer ids.
- **Fade curve type UI** — right-click fade handles or clip menu (Linear / Convex /
  Concave / S-Curve); Alt+click cycles type; uses TE `setFadeInType` /
  `setFadeOutType`.
- **Cross-track clip moves** — vertical drag feedback in `TimelineComponent`;
  `EngineHelpers::moveClipGroupToTrack()` with validation and group offset.
- **Folder drag/reparent** — track header drag (`skeletonHiveTrackDrag` payload),
  drop zones (above / below / into folder / promote), context-menu Move Up/Down/
  Out of Folder; `Edit::moveTrack` via `EngineHelpers`.

### Remaining Phase 2 polish (optional)

- Measure UI telemetry at 200+ tracks (lane-level LOD implemented; see Phase 3).

---

## 6. Phase 3 backlog

### Implemented (this pass)

- **Adaptive timeline clip LOD** (`TimelineLOD.h`) — three detail levels driven by
  `pixelsPerBeat` and on-screen clip width:
  - **Summary** (far zoom): solid clip blocks; no waveforms, MIDI previews, fade
    curves, or thumbnail retention.
  - **Overview**: clip name + MIDI note-density bars; waveforms/fades deferred.
  - **Detail** (near zoom): full waveforms, MIDI note previews, fade curves.
  Wired through `AudioClipComponent`, `MidiClipComponent`, and
  `TrackLaneComponent::updateClipBounds` (thumbnail/preview release at coarse LOD).
- **Lane-level LOD** (`LaneClipSummaryPaint`, `TimelineLOD::useLaneLevelRendering`) —
  at `pixelsPerBeat <= 4`, clip `ClipComponent`s are hidden and summaries are painted
  in `TrackLaneComponent::paint`: per-clip blocks when width ≥ 2 px, bar-bucket strips
  at the lane bottom for sub-pixel clips. Selection and double-click work at this zoom;
  move/resize/fade require zooming in.
- **Multi-output instrument routing** (`MultiOutputRouting`, `MultiOutputConfigDialog`)
  — instrument slot → **Configure Outputs…**; TE `ExternalPlugin::setBusLayout`,
  child tracks, `assignTrackAsInput` / `InputDeviceInstance::setTarget`.
- **Sidechain routing matrix** (`SidechainRouting`, `SidechainMatrixPanel`,
  `SidechainMenu`) — edit-wide matrix panel (transport **Sidechain** toggle) with
  radio selection of one source audio track per sidechain-capable plugin; quick-pick
  submenu + **Sidechain Routing…** entry on plugin slots; TE `sidechainSourceID` +
  `guessSidechainRouting`. One source per plugin (TE model). Rack-internal plugins
  excluded (`canSidechain()` false). Sidechain sources stay audible when muted.

### Remaining

- *(none — Phase 3 backlog complete)*

### Implemented (plugin hardening + session-safe persistence)

- **Persistent plugin blacklist** — scan no longer clears `KnownPluginList` blacklist or dead-man's-pedal state; crashed plugins stay blacklisted across sessions (TE property storage). `PluginScanner::rescanFailedPlugins()` and per-file retry are explicit opt-in. Failed-plugins UI in the plugin browser lists blacklisted entries with Retry / Retry All.
- **Plugin load-failure UX** — tray slots, track-footer slots, and plugin windows surface loading/failed states (amber/red tint, tooltips with TE `getLoadError()`). Failed inserts show an alert instead of failing silently.
- **VST3 editor resize** — `PluginWindow` resizes to follow editor content size changes (`childBoundsChanged` / `resizeToFitEditorContent`). Runtime plugin hosting remains in-process; sandboxing covers scan-time only (TE child-process scanner + blacklist).
- **Versioned autosave** — 60s timer writes TE `saveTempVersion()` when the edit is dirty (never overwrites the project file). Manual save creates rotating snapshots in `<project>/Autosave/` (newest 10 retained) and deletes the temp version.
- **Crash recovery** — on Open, if `.tmp_<project>` exists and is newer than the saved file, offers Recover autosaved version / Open last saved / Cancel.
- **Save As + dirty tracking** — Save As button and Ctrl/Cmd+Shift+S; window title shows `*` when dirty; unsaved-changes prompts on New/Open/Close.
- **External-change + multi-instance guards** — before save, warns if the project file mtime changed externally (Overwrite / Reload / Save As / Cancel). Sidecar `<project>.lock` (PID + timestamp) warns when opening a project that appears live in another instance; stale locks are cleaned automatically.

---

## 7. Plugin Tray / Device Chain (Phase 3)

Bottom-panel device chain aligned with Ableton Live workflow patterns (not visual
copy). All model state lives in TE `PluginList` / `ValueTree`; UI is a thin view.

```
PluginTrayComponent          ← selected track's user chain (horizontal viewport)
 ├─ PluginSlotComponent × N  ← bypass, drag, context menu, collapse
 └─ Output node label

PluginBrowserComponent       ← search, category/vendor filters, favorites/recent
PluginDragManager            ← cross-track + reorder + browser payloads
TrackPluginChainModel        ← user-chain indices, instrument-first validation
PluginStateManager           ← favorites, recent, copy/paste clipboard (user data)
PluginPresetManager          ← ValueTree preset save/load
EngineHelpers                ← insert/move/duplicate/cross-track chain ops
MultiOutputRouting           ← multi-out bus detect, child tracks, TE input routing
MultiOutputConfigDialog      ← Configure Outputs… panel (instrument slots)
```

**Workflow:** click track header → tray shows chain; double-click slot → plugin
editor; drag slot/browser → reorder or move between tracks; Delete/Ctrl+D/C/V
when tray focused; instrument plugins forced to chain start on MIDI tracks.

**Rack chains:** rack slots show an expand chevron (or context menu) to inline
nested devices from `RackType::getPlugins()` in ValueTree order. Internal slots
support bypass, remove, and drag-reorder via `RackType` state `moveChild` with
undo. Rack `ValueTree` listeners on expanded racks coalesce tray rebuilds.
Macro knobs remain in a `RackMacroPanel` CallOutBox. TE rack audio routing is
connection-based; reordering updates display/ValueTree order but does not
automatically rewire serial connections (limitation of TE's graph model).

**Undo:** reorder uses `pluginList.state.moveChild` with `Edit::getUndoManager()`.
Rack-internal reorder uses `RackType::getUndoManager()`. Rename uses
`plugin.state.setProperty`. TE owns plugin add/remove undo. Multi-out route
changes use the Edit `UndoManager` on plugin/track `ValueTree` properties and
`InputDeviceInstance::setTarget`.

**Multi-output limitations:** TE's `TrackWaveInputDeviceNode` forwards stereo
(ch 0–1) from the source track post-chain; `targetIndex` on input destinations
is persisted but not yet honoured in TE's playback graph for per-bus taps. Bus
layout activation and child-track wiring are in place; per-pair audio separation
depends on TE graph support or future channel-matrix work.

**Performance:** tray rebuild coalesced via `AsyncUpdater`; only selected track
is built; horizontal scroll for long chains.

---

## 8. Phase 4 — core production loop (implemented)

### Export / render
- **`ExportManager`** (`Source/Engine/ExportManager.*`) — bounce via `te::Renderer`:
  WAV/FLAC, 44.1–96 kHz, 16/24/32-bit, entire project or loop selection, master
  plugins included, dithering below 32-bit. Transport **Export** button and
  Ctrl/Cmd+Shift+E. `ExtendedUIBehaviour::runTaskWithProgressBar` runs TE render
  tasks (export **and** track freeze) behind a modal cancellable progress dialog.

### Metronome, count-in, punch
- Transport **Click** toggle (`edit.clickTrackEnabled`), volume slider
  (`setClickTrackVolume`, 0.2–1.0) and count-in combo (`Edit::setCountInMode`:
  off / 1 bar / 2 bars / 2 beats / 1 beat).
- **Punch fixed**: the Punch button now drives `edit.recordingPunchInOut` (it
  previously only set a dead local flag). TE derives the punch range from the
  loop in/out markers, so the loop brace doubles as the punch region;
  the misleading `setPunchRange` (which called `setLoopRange`) was removed.

### Automation panel (resurrected)
- **`AutomationPanel`** (`Source/UI/Automation/AutomationPanel.*`) — bottom
  panel toggled by the transport **Auto** button; follows the selected track.
  Lanes for volume/pan plus every parameter with automation; an "Add lane…"
  combo exposes **all** of the track's automatable parameters.
- **`AutomationLaneComponent`** rewritten: maps the visible timeline range,
  normalised-value editing (click adds, drag moves with grid snapping,
  right-click deletes points), live value marker, repaints on
  `curveHasChanged`/`currentValueChanged`. All edits use the Edit UndoManager.
- **Read/Touch/Latch** set the track's `te::AutomationMode` and toggle
  `AutomationRecordManager` read/write, so control moves during playback are
  recorded by TE itself (touch punches out on release, latch on stop).

### Recording workflow
- Arm button arms via `armTrackWithDefaultInput`: tracks with only MIDI clips
  get all physical/virtual MIDI inputs, others get the first wave input;
  right-click the arm button for an input-assignment picker (multi-target
  `InputDeviceInstance::setTarget`, falls back to move when a device only
  allows one target). "+ MIDI" tracks get MIDI inputs assigned at creation.

### Markers
- Ruler paints marker flags (TE `MarkerManager` / `MarkerClip`), right-click:
  add / rename / delete. **M** adds a marker at the playhead; **Alt+Left/Right**
  jump to the previous/next marker (falls back to 0:00 going left).

### Tempo map
- Ruler right-click: **Insert Tempo Change Here…** (BPM prompt) and **Insert
  Time Signature Here** (submenu), remove either when clicked near a change
  flag; changes are painted in the ruler (orange = tempo, blue = time-sig,
  index 0 is never treated as a "change"). Tempo edits trigger a full timeline
  re-layout via `TimelineRulerComponent::onTempoMapChanged` since the
  beat↔time mapping shifts.
- The transport BPM slider / time-sig combo now edit the setting **at the
  playhead** (`getTempoAt`/`getTimeSigAt`) and follow it during playback,
  instead of always editing `getTempo(0)`.

### Track freeze
- Track-header context menu **Freeze/Unfreeze Track** using
  `AudioTrack::setFrozen (…, individualFreeze)` (TE background render +
  freeze-point plugin). Frozen tracks show a blue tint + FROZEN tag in the
  header and repaint on the `frozen`/`frozenIndividually` properties.

---

## 10. Phase 5 — Control & UX (implemented)

### Application settings
- **`AppSettings`** (`Source/Engine/AppSettings.*`) — persisted user preferences
  (theme, autosave interval, default project folder, keyboard mappings) in
  `%AppData%/SkeletonHive/app.settings`.

### Look and feel
- **`AppLookAndFeel`** + **`AppColours`** (`Source/UI/AppLookAndFeel.*`) — shared
  dark/light palettes applied at startup; core panels (timeline, plugin tray,
  automation, plugin slots) read colours via `AppLookAndFeel::getCurrentTheme()`.

### Keyboard commands
- **`AppCommands`** + `ApplicationCommandManager` in `MainContentComponent` —
  central command registry covering transport, timeline, plugin tray, and piano
  roll shortcuts. Defaults mirror the prior hard-coded bindings; user overrides
  persist via `AppSettings`. Context routing: piano-roll window focus, plugin
  selection/tray focus, then global/timeline commands.

### Preferences dialog
- **`PreferencesDialog`** (`Source/UI/Settings/PreferencesDialog.*`) — opened
  from transport **Prefs** (formerly Audio-only). Tabs: **General** (project
  folder, autosave interval), **Appearance** (dark/light theme), **Keyboard**
  (`KeyMappingEditorComponent`), **Audio** (`AudioDeviceSelectorComponent` on
  TE's device manager).

### MIDI learn
- **`MidiLearnController`** + `EngineHelpers::startParameterMidiLearn` — TE
  `ParameterControlMappings` / `MidiLearnState` integration. Transport **Learn**
  toggle arms global learn mode; right-click channel-strip fader/pan → **MIDI
  Learn…** / **Remove MIDI Mapping**. Status bar shows the active assignment
  target.

---

## 11. Conventions for contributors

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

---

## 12. Phase 6 — Editing Depth (implemented)

**Theme:** Close the deferred Tier 3 editing backlog from the Phase 4 plan. Make
audio and MIDI clips as editable in-place as Ableton Live's Detail View and
Take Lanes — without leaving the arrangement view.

Phases 1–5 delivered arrangement editing, routing, the production loop, and
control/UX. Phase 6 closes the remaining **studio-workflow** gaps vs. Live 12:
MIDI CC/modulation lanes, audio takes/comping, clip-level audio properties, and
in-place consolidate/bounce utilities.

### Tier 1 — MIDI editor lanes (implemented)

Extend the existing velocity-lane pattern in `PianoRollEditor`:

- **CC lanes** — one lane per controller number (1/7/11/64…); pencil + drag
  editing via `te::MidiList` with the Edit `UndoManager`.
- **Pitch-bend lane** — 14-bit curve or segmented editing.
- **Lane selector** — tab strip or combo (Velocity | CC | Pitch Bend |
  Aftertouch).
- **Stretch goal (Live 12):** per-note MPE expression (pitch bend / pressure
  per note) when TE exposes it on `MidiNote`.

**Files:** `Source/UI/Midi/MidiLaneEditor.*`,
`Source/UI/Midi/MidiControllerLaneComponent.*`, `Source/UI/Midi/MidiLaneViewport.h`.

### Tier 2 — Audio clip properties (implemented)

Clip-level control without opening an external editor:

- **Gain, transpose, reverse** — `ClipInspectorPanel` above the plugin tray,
  bound to TE `AudioClipBase` properties with undo.
- **Speed / time-stretch** — speed ratio (25–400%) and time-stretch mode combo
  (`setSpeedRatio`, `setTimeStretchMode`). Warp markers / `WarpTimeManager` UI
  deferred — TE warp depth needs a separate spike.
- **Clip colour / name / loop** — TE `Clip::setName` / `setColour`; loop toggle
  + loop-length beats via `setLoopRangeBeats` / `disableLooping`.

**Files:** `Source/UI/Arrangement/ClipInspectorPanel.*`.

### Tier 3 — Takes and comping (implemented)

- **Loop recording → take lanes** stacked under the parent clip via `TakeLaneStack` / `TakeLaneComponent`.
- **Comp lane** — swipe/select active regions across takes; promote comp to
  main clip via `WaveCompManager` / `MidiCompManager` APIs.
- TE take/comp model used directly (`addTake`, `changeSectionIndexAtTime`, `flattenTake`).

**Files:** `Source/UI/Arrangement/TakeLaneComponent.*`, take/comp helpers in `EngineHelpers`.

### Tier 4 — In-place production utilities (implemented)

- **Consolidate** — bounce selected clips in place via `ExportManager::renderScopeToFile`
  + `EngineHelpers::consolidateClips` (context menu + Ctrl/Cmd+J); one WAV clip
  per affected track replaces sources.
- **Flatten** — `EngineHelpers::flattenTrackToAudioClip` renders the device chain
  to a timeline clip and removes user plugins (track header **Flatten to Audio Clip…**).
- **Linked track editing** — `EngineHelpers::expandWithGroupedPeers` propagates
  clip-inspector and fade-curve edits to grouped clips across tracks.

**Files:** `Source/Engine/ExportManager.*`, consolidate/flatten helpers in
`EngineHelpers`, context menus in `TrackComponents.cpp`.

### Success criteria

A user can loop-record vocals, comp a take, edit mod-wheel automation in the
piano roll, transpose a sample clip, and consolidate a stem — all without
leaving the arrangement view.

### Dependencies and risks

- **Warp/stretch depth** depends on TE `WaveAudioClip` API surface — spike
  before Tier 2 implementation.
- **Comping** may require additive persistence if TE lacks first-class take
  lanes; follow clip-group conventions (§2.10).
- **Multi-out per-bus taps** (§7) may block drum-kit comp workflows until TE
  graph support improves — track alongside Tier 3.

---

## 13. Phase 7 — Browser and Content Workflow (Tier 1–4 implemented)

**Theme:** Live's left-hand Browser is how users *find* sound. SkeletonHive
has a left-hand **BrowserPanel** (Places, Samples, Clips, Grooves, Plugins)
and a Live-style bottom **DetailPanelStack** (Devices | Clip).

Depends on Phase 6 Tier 2 (clip inspector) for a coherent detail-panel stack —
satisfied.

```
BrowserPanel
 ├─ Places      (project folder, user library, favorites)
 ├─ Samples     (WAV/AIFF/FLAC scan + preview)
 ├─ Clips       (saved clip presets / project excerpts)
 ├─ Grooves     (project groove pool + apply to selection)
 ├─ Plugins     (PluginBrowser embedded tab)
 └─ PreviewPlayer (shared te::SmartThumbnail / transport preview)
```

### Tier 1 — Sample browser (implemented)

- Scan configurable library paths (persist in `AppSettings`).
- **Hover preview** with auto-stop; **drag to timeline** creates
  `WaveAudioClip` at drop beat.
- **OS file drag** onto track lanes creates clips at snapped beat.
- Search, sort (name / date / duration), favorites and recent via
  `ContentLibraryManager`.
- **Library** preferences page for folder management.

**Files:** `Source/UI/Browser/*`, `Source/Engine/ContentLibraryManager.*`,
`Source/Engine/PreviewPlayer.*`, `Source/Engine/ContentDragManager.*`.

**Deferred from Tier 1 spec:** drag to empty MIDI track → auto-insert sampler
(plugin-specific; wave clip drop works on any `ClipTrack`).

### Tier 2 — Hot-swap and preset workflow (implemented)

- **Hot-swap plugin** — tray slot context menu → Replace…; parameter mapping via
  TE preset/state transfer (`EngineHelpers::replacePluginOnTrack` /
  `replacePluginInRack`).
- **Preset browser** per device — named presets in user library folders with
  categories; session A/B compare in `PresetBrowserPanel`.
- **Default device chains** — Preferences → Devices; applied when user adds
  audio/MIDI tracks (`AppSettings` + `applyDefaultDeviceChain`).

**Files:** `Source/UI/Plugins/PluginPickerDialog.*`,
`Source/UI/Plugins/PresetBrowserPanel.*`, extended `PluginPresetManager.*`,
`PreferencesDialog` Devices page, `EngineHelpers` replace/chain helpers.

**Rack limitation:** TE rack serial connections are not auto-rewired on replace
(same as §7 reorder note); hot-swap preserves slot index but cross-type swaps
inside serial racks may need manual rewiring.

### Tier 3 — Project content (implemented)

- **Clip library** — drag arrangement clips to browser to save; drag back to
  instantiate.
- **Export selection to library** — one-click from clip context menu.
- **Collect All and Save** — copy external audio into project folder on save
  (TE `EditFileOperations` helpers).

**Files:** `Source/Engine/ClipLibraryManager.*`, `Source/UI/Browser/ClipsBrowserTab.*`.

### Tier 4 — Arrangement ergonomics (implemented)

- **Detail panel stack** — fixed-height bottom area with **Devices | Clip**
  tabs (`DetailPanelStack`); no add/remove layout thrash.
- **Roaming focus** — `syncRoamingFocus()` resolves track from selection/clip
  and drives plugin tray, clip inspector, automation panel, and plugin browser.
- **Global groove pool** — `GroovePoolManager` with built-in + user templates
  persisted per project (`.skeletonhive/*.grooves.xml`); apply from Grooves tab,
  clip context menu, or Shift+H; piano roll humanize shares the pool.
- **Plugins browser tab** — `PluginBrowser` embedded in left `BrowserPanel`;
  Transport **Plugins** button opens browser to Plugins tab.

**Files:** `Source/UI/Detail/DetailPanelStack.*`,
`Source/Engine/GrooveTemplate.h`, `Source/Engine/GrooveEngine.*`,
`Source/Engine/GroovePoolManager.*`, `Source/UI/Browser/GroovesBrowserTab.*`,
`MainWindow` roaming-focus wiring.

### Success criteria

Import a folder of samples, audition by hovering, drag a kick onto bar 1,
hot-swap the compressor on the bass track, and save a reusable clip preset.

### Dependencies and risks

- **Rack serial reorder wiring** (§7) — documented limitation for rack-internal
  hot-swap (Tier 2 implemented with slot-preserving replace; serial graph not
  auto-rewired).
- Preview playback must stay off the audio thread; reuse `WaveformCache` /
  `SmartThumbnail` patterns from §4.

---

## 14. Phase 8 — Session View and Performance Mode (Tier 1 implemented)

**Theme:** Arrangement View is the studio; Session View is Live's performance
half. This is the largest new subsystem — clip launching, scenes, and
Session ↔ Arrangement capture.

```
SessionViewComponent
 ├─ SessionGrid (tracks × scenes matrix)
 ├─ ClipSlotComponent (empty / loaded / playing / recording states)
 ├─ SceneLaunchColumn
 └─ SessionTransportBridge (quantized launch, follow actions)

SessionArrangementBridge
 └─ Record Session → Arrangement, Capture & Insert, Duplicate loop to arrangement
```

### Tier 1 — Session grid MVP (implemented)

- **Toggle Arrangement ↔ Session** (Tab key, Live-style).
- Each track gets **N clip slots** (default 8, expandable); each slot holds a
  reference to a `te::Clip` or a session-only clip committed to the Edit on
  demand.
- **Launch / stop** per slot; **scene launch** (column) with stop-others vs.
  additive-launch preference.
- **Quantization** — none / 1 bar / 1/2 / 1/4 / 1/8 (reuse `TimelineGrid`
  snap intervals).
- **Browser drops** — samples and clip presets can be dropped into session slots.
- **Transport + looped clips playback** — TE has no native clip-launcher API;
  session clips are tagged in clip state, parked off-timeline when inactive,
  activated at beat 0 with looping when launched.

**Model:** session slots as `SESSIONSTATE` / `SLOT` ValueTree children under
`EDITVIEWSTATE` pointing at clip IDs (additive-property pattern from §2.10).

**Files:** `Source/UI/Session/*`, `Source/Engine/SessionManager.*`,
`Source/Engine/SessionArrangementBridge.*` (stub for Tier 2),
`MainWindow` view toggle, `TransportBar` launch-quantize controls.

### Tier 2 — Session ↔ Arrangement bridge

- **Record to Arrangement** — launched clips write material into arrangement
  lanes while playing.
- **Capture & Insert** — grab currently playing session material into a new
  arrangement clip at the playhead.
- **Duplicate loop to arrangement** — one-click commit of a session loop.

### Tier 3 — Performance features

- **Follow actions** — per slot: None | Play Next | Play Previous | Play Random
  | Stop.
- **Legato launch** — continue playback position when re-triggering.
- **Macro performance panel** — rack macros + mapped parameters in a grid
  (extends `RackMacroPanel`).
- **MIDI mapping to clip slots** — extend `MidiLearnController` for note/CC →
  slot launch.

### Tier 4 — Scale and Live 12 alignment

- **Key/scale clip constraint** — clip-level scale lock for launched MIDI
  clips.
- **Probability and iteration** — per-note probability in session clips.
- **200+ track telemetry** — finish optional Phase 2 benchmark (§5); session
  grid must pool slot components like lane virtualization (§4).

### Success criteria

Build a beat in Session View, launch scenes live with quantization, record the
performance into the arrangement, and have the result look hand-arranged.

### Dependencies and risks

- Start Tier 1 after Phase 6 comping (optional but makes record → comp →
  arrange coherent).
- Phase 7 browser integration enables dragging samples directly into session
  slots.
- Session launch timing must use TE transport APIs only — no custom audio-thread
  scheduling (§4, §2.7).

### Explicitly deferred (Phase 9+)

- Built-in instruments (Simpler/Sampler/Operator equivalents).
- Push/APC hardware profiles.
- Collaboration / cloud / Ableton Link.
- Full light-theme polish across every panel (Phase 5 foundation exists).

---

## 15. Phase roadmap summary

| Phase | Status | Focus | Live 12 analogy |
|-------|--------|-------|-----------------|
| 1 | Implemented | Foundation, arrangement, piano roll, routing | Core edit workflows |
| 2 | Implemented | Workflow polish, performance infra | Groups, ripple, racks |
| 3 | Implemented | LOD, multi-out, sidechain, plugin hardening | Device chain depth |
| 4 | Implemented | Export, record, automation, markers, freeze | Production loop |
| 5 | Implemented | Shortcuts, prefs, theme, MIDI learn | Control surface |
| 6 | Implemented | MIDI CC lanes, clip inspector, comping, consolidate/flatten | Detail View, Take Lanes |
| 7 | Implemented | Unified browser, hot-swap, presets, clip library, groove pool, detail stack | Browser, Hot-Swap |
| 8 | Tier 1 done | Session grid, scenes, launch quantize (Tier 2+ bridge planned) | Session View |

**Suggested implementation order:** Phase 6 → Phase 7 (after 6 Tier 1–2) →
Phase 8 (largest lift; Tier 1 can start once Phase 6 comping is in place).
