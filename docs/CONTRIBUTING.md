# Contributing to SkeletonHive

Short guardrails for humans and tools. The "why" lives in
[ARCHITECTURE.md](ARCHITECTURE.md). Delivery status lives in
[ROADMAP.md](ROADMAP.md).

SkeletonHive is a TE-first DAW. Most serious failures at this scale come from
quietly breaking ownership, undo, or realtime boundaries — not from missing
UI features. Prefer small, reviewable changes that respect the invariants
below.

---

## Architectural invariants

Treat these as must-not-break unless an explicit architecture change (with
human review) updates [ARCHITECTURE.md](ARCHITECTURE.md) first:

1. **Single Edit owner.** Only `ProjectManager` owns `te::Edit`. On New/Open,
   destroy edit-bound UI first (`releaseEditUI`), swap the Edit, then
   `rebuildEditUI`. Do not retain `Edit&` or raw pointers into Edit-owned
   objects across that boundary.

2. **Undo for every user-visible mutation.** Pass
   `&edit.getUndoManager()` (or the appropriate TE undo API). Never use
   `nullptr` undo for interactive edits.

3. **Realtime safety.** No locks, heap allocations, or `ValueTree` access on
   the audio thread, except the documented sandbox shared-memory / atomics
   path under `Source/Engine/PluginHost/`. Do not invent a second audio
   callback beside TE for arrangement playback.

4. **UI talks to TE / `EngineHelpers` only.** No direct
   `juce::AudioDeviceManager` or playback-graph manipulation from
   `Source/UI/`.

5. **No parallel model.** Workflow gaps TE lacks (clip groups, session slots,
   etc.) use additive Edit `ValueTree` properties. Do not add a second
   source of truth or a side-car project format for those concepts.

6. **TE patches are checked in.** Pin TE via `GIT_TAG`. Apply fixes only
   through `cmake/patches/` + `cmake/apply_tracktion_patches.cmake`. Never
   hand-edit `build/_deps/` — those trees are disposable.

7. **Sandbox scope is deliberate.** Out-of-process hosting is an MVP for
   VST3 **effects**. Expanding to instruments, AU, sidechain/multi-out, or
   full editor embedding is high-risk architecture work, not a drive-by
   feature.

---

## Where new code goes

| Kind of change | Prefer |
|----------------|--------|
| Views, panels, mouse/keyboard interaction | `Source/UI/…` |
| Edit mutations, routing, persistence, session model, plugin host | `Source/Engine/…` |
| App lifetime, window shell, global settings wiring | `Source/App/…` |
| Tracktion Engine bugfixes | `cmake/patches/` (not `build/_deps/`) |

New persistent view/session state belongs in `EditViewState` /
`EDITVIEWSTATE`, not in long-lived component fields.

Prefer existing `te::` APIs over reimplementation. If TE already has a type
whose name matches what you are about to write, read it first.

---

## Before merging large changes

Ask explicitly:

- Does this **fight TE** (custom graph, custom undo, bypassing transport)?
- Does this add a **second model** beside the Edit?
- Does this **cross the realtime boundary** without a documented protocol?
- Does this **cache Edit-owned objects** past `releaseEditUI`?
- If TE must change, is there a **checked-in patch** and pin update?

If the answer to the first four is unclear, stop and revisit
[ARCHITECTURE.md](ARCHITECTURE.md) before landing the change.

---

## Verify the change actually runs

Before re-prompting or stacking speculative fixes:

1. Edit real sources under `Source/` or `cmake/patches/` — not copies under
   `build/` or `build/_deps/`.
2. Confirm the target rebuilt (link step / artefact mtime under
   `build/SkeletonHive_artefacts/`).
3. Launch the binary you just built.
4. Prefer a repro, crash site, or log line over another speculative edit.
5. Keep the diff scoped so one commit maps to one coherent concern.
