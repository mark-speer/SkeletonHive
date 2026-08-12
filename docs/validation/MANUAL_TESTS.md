# Manual validation plan

Use these checks to establish product behavior that compilation and unit tests
cannot prove. Do not test irreplaceable projects or unlicensed/untrusted
content.

## Evidence header

Record this before testing:

- commit SHA and build configuration;
- operating system and version;
- compiler/toolchain;
- SkeletonHive binary path;
- audio interface and driver;
- sample rate and buffer size;
- MIDI device;
- plugin name/version and sandbox mode;
- tester and date.

A Pass requires every stated pass criterion. Record Partial or Fail with the
first failed step and preserve the project/log needed to reproduce it.

## MT-01 — Launch and clean shutdown

1. Launch the exact newly built binary with default settings.
2. Open Preferences and confirm available audio/MIDI devices populate.
3. Close the application normally.
4. Relaunch it.

Pass: no hang or crash; the expected settings persist; the launched binary is
confirmed to be the one under test.

## MT-02 — Project lifecycle and undo

1. Create a project with one audio and one MIDI track.
2. Add, move, resize, duplicate, and delete clips.
3. Undo and redo each operation.
4. Save, close, reopen, and compare track, clip, tempo, routing, and selection-
   relevant state.

Pass: edits are undoable, reopening preserves intended project state, and no
stale UI or Edit references appear after New/Open transitions.

## MT-03 — Audio record and edit

1. Select the intended input and arm one audio track.
2. Record at least 30 seconds with count-in and metronome.
3. Repeat with loop/punch behavior.
4. Trim, fade, move, duplicate, consolidate, and play the result.

Pass: recorded timing and channel routing are correct; edits are audible and
undoable; playback has no unexplained dropout or corruption.

## MT-04 — MIDI record and edit

1. Record MIDI into a native instrument.
2. Edit notes, velocity, one CC lane, pitch bend, and clip scale settings.
3. Quantize, undo, save, reopen, and render.

Pass: note/controller state and timing survive undo and reopen; playback and
render agree with the edit.

## MT-05 — Plugin hosting in process

For each representative VST3/AU effect and instrument:

1. Scan and insert the plugin.
2. Open, resize, close, and reopen its editor.
3. Change and automate parameters.
4. Save, close, reopen, and compare plugin state.
5. Render an audible result.

Pass: editor lifecycle, parameters, automation, state, routing, and render are
correct without hanging the application.

## MT-06 — Sandboxed plugin behavior

1. Repeat MT-05 with supported sandbox mode enabled.
2. Exercise bypass, parameter changes, automation, preset/state restore, and
   editor open/close.
3. Force a controlled bridge exit while stopped and during playback.
4. Exercise a plugin hang or timeout using a safe test fixture.
5. Confirm restart limits and permanent-failure behavior.

Pass: supported behavior matches the documented compatibility boundary; bridge
failure does not terminate SkeletonHive; audio-thread deadlines remain
bounded; project state remains recoverable.

If a feature such as automation, sidechain, multi-output, editor embedding, or
instrument support is intentionally unavailable, mark it Not supported rather
than passing the workflow.

## MT-07 — Session launch timing

1. Create clips with sharp transients aligned to their loop starts.
2. Launch individual clips and scenes at each quantization setting.
3. Repeat at supported sample rates and 64, 128, 256, and 512 sample buffers.
4. Capture the output or engine timing trace and measure launch error against
   the requested musical boundary.
5. Repeat follow actions, legato launch, stop, and Session-to-Arrangement
   capture.

Until a timing tolerance is accepted in a decision record, report measured
offsets rather than a subjective Pass.

## MT-08 — Save, autosave, and recovery

1. Save a project, make it dirty, and allow an autosave interval to elapse.
2. Terminate a disposable test instance without a normal save.
3. Reopen and exercise each offered recovery choice.
4. Modify the project externally and exercise overwrite, reload, Save As, and
   cancel behavior.
5. Open the same disposable project from two instances and verify lock
   behavior.

Pass: no choice silently destroys the last recoverable state; recovered state
matches the selected option; stale locks can be cleared safely.

## MT-09 — Export correctness

1. Build a plugin-free reference project with known tones, MIDI events,
   automation, tempo changes, and silence.
2. Export the full project and a loop selection to each supported format,
   sample rate, and bit depth.
3. Check duration, channels, peak level, silence, event timing, and audible
   result.
4. Compare deterministic fixtures to stored numerical tolerances when those
   tests exist.

Pass: exported scope, timing, routing, processing, and format match the request.

## MT-10 — Scale and endurance

1. Exercise the documented 200-track Arrangement and Session stress actions.
2. Record component counts and rebuild/render timing.
3. Run continuous playback, editing, saving, plugin editor operations, and
   Session launches for a defined duration.
4. Record CPU, memory, dropouts/xruns, deadline misses, and crashes.

Pass only against accepted numerical thresholds. Without thresholds, retain
the run as diagnostic evidence.

## Result template

| Field | Result |
|---|---|
| Test ID | |
| Commit/configuration | |
| Environment/devices/plugins | |
| Result | Pass / Fail / Partial / Not supported |
| Observations | |
| Evidence link | |
| Follow-up issue | |
