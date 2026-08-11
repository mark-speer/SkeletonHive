# SkeletonHive product vision

Status: draft for maintainer confirmation

## Product intent supported by the repository

SkeletonHive is an open, cross-platform digital audio workstation built with
JUCE and Tracktion Engine. It combines an arrangement-based production
workflow with a Session-style performance workflow.

The existing product direction emphasizes:

- fast, low-friction audio and MIDI editing;
- keyboard-driven operation and immediate visual feedback;
- a thin UI over an engine-owned model, undo system, and audio graph;
- support for third-party plugins without allowing one plugin to destroy the
  entire session;
- project persistence and recovery that protect creative work;
- useful behavior across Windows, macOS, and Linux.

Ableton Live is an interaction reference, not a feature-count commitment.
SkeletonHive should learn from proven workflows without treating parity with
another mature DAW as a meaningful definition of done.

## What success should mean

Before breadth, a user should be able to depend on a narrow production loop:

1. Configure an audio or MIDI device.
2. Create or open a project.
3. Record or import material.
4. Edit it without timing, undo, or state corruption.
5. Use supported plugins predictably.
6. Save, close, reopen, and recover the project.
7. Export an audibly correct result.

For a performance workflow, clip and scene launches must be musically
deterministic. For every workflow, stability and preservation of the user's
work outrank the number of visible features.

## Product principles

- Prefer a smaller dependable workflow to broad unverified coverage.
- Make state and timing behavior predictable.
- Let Tracktion Engine own the model and audio graph unless an accepted
  decision demonstrates why it cannot.
- Keep failure visible and recoverable.
- Measure responsiveness and audio correctness; do not infer them from code
  shape.
- Treat cross-platform behavior as part of the product, not post-release
  cleanup.
- Preserve user work through crashes, plugin failures, project changes, and
  version upgrades.

## Maintainer decisions still needed

The maintainer should confirm these before feature expansion resumes:

| Decision | Why it matters |
|---|---|
| Primary user and first complete workflow | Determines which capabilities form the real MVP |
| Arrangement-first, Session-first, or equal priority | Determines where architectural effort goes first |
| Officially supported operating systems | Defines the required release matrix |
| Supported plugin formats and sandbox expectations | Defines compatibility promises and test load |
| Minimum audio hardware and buffer-size targets | Defines real-time performance acceptance |
| Meaning of the first stable release | Prevents version numbers from outrunning validation |
| Long-term AGPL, dual-license, or commercial intent | Affects contributor and dependency decisions |

Until those choices are recorded, ROADMAP.md prioritizes stabilization and
evidence rather than additional feature parity.
