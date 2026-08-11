# SkeletonHive roadmap

This roadmap controls priority. The accumulated implementation inventory and
historical phase plan remain in ARCHITECTURE.md.

## Evidence model

Every capability tracks evidence independently:

| Evidence | Meaning |
|---|---|
| Proposed | Desired behavior and acceptance criteria exist |
| Code present | Relevant implementation exists in the repository |
| Build verified | The affected target builds in a named environment |
| Automated | Relevant automated checks pass |
| Human validated | The workflow was observed in a real build |
| Cross-platform | Required environments passed |
| Released | A validated commit was published immutably |

An implemented label in the historical architecture document maps only to
Code present unless stronger evidence appears in the validated baseline.

## Current objective: Phase 0 — establish a dependable baseline

Feature expansion is paused until the Phase 0 exit criteria are met.

### Now

#### SH-000 — Repository truth and contribution controls

Status: in progress

Acceptance:

- canonical product, architecture, roadmap, validation, and handoff concerns
  are separated;
- agents follow a scoped required-reading path;
- contributors have a pull-request evidence template;
- current build and validation status is recorded without inflating claims.

#### SH-001 — Reproducible green build matrix

Status: proposed; highest priority after SH-000

Acceptance:

- Windows, macOS, and Ubuntu configure and build in CI;
- JUCE, Tracktion Engine, runners, and toolchain expectations are pinned or
  deliberately versioned;
- local build instructions match the real dependency mechanism;
- required checks protect master;
- release creation is gated on supported-platform checks;
- the compatibility matrix links passing evidence.

#### SH-002 — Test seam and automated baseline

Status: proposed

Acceptance:

- testable engine/domain code can build without launching the GUI;
- CTest or an equivalent runner executes in CI;
- deterministic tests cover grid/tempo conversion, MIDI scale/groove logic,
  routing/state serialization, project save/reopen/migration, and plugin-host
  protocol/shared-memory behavior;
- one plugin-free render fixture verifies audio output within documented
  tolerances;
- debug benchmarks report enforceable thresholds or remain explicitly
  diagnostic rather than tests.

#### SH-003 — Real-time and concurrency audit

Status: proposed

Acceptance:

- every custom audio callback and callback-reachable path is inventoried;
- locks, waits, allocations, filesystem access, logging, UI calls, and
  ValueTree access are either absent or removed;
- cross-thread state publication and ownership are documented;
- asynchronous callbacks have explicit lifetime protection;
- representative buffer sizes and failure paths are measured.

### Next

#### SH-004 — Engine-timed Session launch

Replace message-thread polling with block/sample-aligned scheduling. Add
deterministic launch-quantization tests and measure worst-case launch error.

#### SH-005 — Plugin sandbox compatibility boundary

Make sandboxing opt-in while incomplete, then validate parameter and automation
round trips, state restore, editor lifecycle, crashes, hangs, restart limits,
sidechain, multi-output, instruments, and supported operating systems.

#### SH-006 — Project and asynchronous lifetime hardening

Audit project replacement, plugin scanning, background analysis, rendering,
recovery, and shutdown for use-after-free, stale Edit references, and queued
callbacks.

#### SH-007 — Decompose high-coupling modules

After behavior has tests, split large modules along existing responsibilities:
clip editing, routing, plugin chains, render/persistence, Session control, and
major UI composition. Refactoring must preserve validated behavior.

### Later

Re-evaluate the historical Phase 11 and Phase 12 feature inventory only after
Phase 0 exits. Product work should be selected from confirmed user and
maintainer priorities, not from another DAW's feature count.

## Phase 0 exit criteria

Phase 0 is complete only when:

1. Supported-platform CI is green and required.
2. Dependencies and release inputs are reproducible.
3. Automated tests exercise the core state and audio paths.
4. The narrow production loop in VISION.md has recorded human validation.
5. Critical real-time and lifetime observations are resolved or accepted in
   decision records.
6. Plugin sandbox support and defaults match the published compatibility
   boundary.
7. The validated baseline, compatibility matrix, roadmap, and handoff agree.

## Work-item template

Each new item should specify:

- ID and user/developer problem;
- scope and explicit exclusions;
- affected architecture and threads;
- acceptance criteria;
- automated evidence;
- required human workflows and platforms;
- migration, recovery, and rollback considerations;
- documentation and handoff impact.
