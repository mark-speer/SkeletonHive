# Validated baseline

Evidence date: 2026-08-12

Baseline commit:
8db7a68409a97d3ddd9dec16db1f61971c2c621f

This document records demonstrated behavior only. It does not infer runtime
correctness from source files, controls, feature lists, or successful
compilation on another platform.

## Automated build evidence

GitHub Actions run (SH-001):
[CI run 31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990)

Pull request: [#2](https://github.com/mark-speer/SkeletonHive/pull/2)

| Environment | Configure | Build | Package | Evidence result |
|---|---:|---:|---:|---|
| Windows latest, Visual Studio 18 2026, Release | Pass | Pass | Pass | Build and artifact upload verified |
| Ubuntu latest, Ninja, Release | Pass | Pass | Pass | Build and artifact upload verified |
| macOS latest, Ninja, Release | Pass | Pass | Pass | Build and artifact upload verified |

Dependency pins on this commit (ADR 0001):

- Tracktion Engine: `53a32a4dcc2d73186b6cee4600b5dd51c65a0cae`
- JUCE: `37c894f83d379179b2070d437ccd0f1cd9af9576`
- `JUCE_CPM_DEVELOP` forced OFF

Local corroboration:

- Windows Release incremental build after NamPlugin publish fix (`build/`)
- Fresh Windows configure + Release build against the pinned JUCE FetchContent
  tree (`build-sh001/`)

This establishes a three-OS CI build/package result for the SH-001 commit. It
does not establish that the application launches, produces correct audio,
preserves projects, or works with a particular device or plugin.

### Superseded failures

| Commit / run | Observation | Superseded by |
|---|---|---|
| `3b65796` / [31539254267](https://github.com/mark-speer/SkeletonHive/actions/runs/31539254267) | Ubuntu configure Fail; macOS Build Fail (Xcode generator; `std::atomic<std::shared_ptr>`) | SH-001 matrix above |
| `30dd538` / [31552281072](https://github.com/mark-speer/SkeletonHive/actions/runs/31552281072) | Ubuntu Pass; macOS Build Fail | SH-001 macOS Pass |

SH-001 code changes that unblocked macOS:

- Portable NAM model publish via `std::atomic_*` on a plain `shared_ptr`
  (`NamPlugin`) instead of `std::atomic<std::shared_ptr<nam::DSP>>`
- JUCE FetchContent pin (no floating `#develop`)

## Automated test evidence

- No repository-level CTest registration or automated unit/integration test
  suite was found at the baseline commit.
- CMake defines a NAM smoke-test executable, but it is not registered as a
  repository test or run by the current CI workflow.
- Debug stress commands log information but do not enforce pass/fail
  thresholds.

Current automated behavioral evidence: none recorded. Tracked as SH-006.

## Manual product evidence

No repeatable manual results are recorded in the repository for:

- application launch and shutdown;
- audio-device setup;
- audio or MIDI recording;
- arrangement editing and undo/redo;
- plugin scanning, loading, automation, state restore, or sandbox recovery;
- Session launch timing;
- save, reopen, migration, autosave, or crash recovery;
- export audio correctness;
- long-session or high-track-count stability.

These behaviors may work, but their repository status is Not recorded until a
result using validation/MANUAL_TESTS.md is committed or linked.

## Interpretation of existing feature claims

README.md and ARCHITECTURE.md demonstrate broad implementation intent and code
presence. Their feature and implemented labels do not, by themselves, promote
a workflow to automated, human-validated, cross-platform, or released status.

## Review observations requiring triage

These are source-review observations. They are not accepted defects until the
maintainer confirms them or a reproduction establishes impact.

| ID | Observation | Evidence to obtain | Roadmap |
|---|---|---|---|
| RT-01 | SessionManager starts a 30 Hz timer and processes pending launches from timerCallback, allowing musical launch detection to lag a polling interval. | Deterministic launch-offset measurement and scheduler design | SH-005 |
| RT-02 | SandboxedPluginInstance obtains a SpinLock-protected coordinator snapshot from processBlock. | Audio-thread trace/review and non-blocking replacement | SH-003 |
| SBX-01 | Plugin sandboxing defaults on for supported VST3 effects while host-side parameter automation, sidechain/multi-output, instruments, AU, and some platform behavior remain deferred. | Explicit support matrix and workflow tests | SH-004 |
| LIFE-01 | PluginScanner queues message-thread callbacks capturing raw this after worker activity. | Shutdown/destruction reproduction and lifetime-safe callback design | SH-002 |
| PERF-01 | EngineBenchmarkHarness records diagnostic timings without thresholds; one cache-stat helper returns a placeholder value. | Measurable workloads and pass/fail criteria | SH-006 |
| DOC-01 | The accumulated architecture roadmap contains conflicting phase summaries and obsolete deferred-work statements. | Current-state architecture and evidence-based roadmap review | Phase 0 docs |

Source locations:

- [SessionManager.cpp](../../Source/Engine/SessionManager.cpp)
- [SandboxedPluginInstance.cpp](../../Source/Engine/PluginHost/SandboxedPluginInstance.cpp)
- [AppSettings.cpp](../../Source/Engine/AppSettings.cpp)
- [PluginHostHelpers.cpp](../../Source/Engine/PluginHost/PluginHostHelpers.cpp)
- [PluginScanner.cpp](../../Source/Engine/PluginScanner.cpp)
- [EngineBenchmarkHarness.cpp](../../Source/Engine/EngineBenchmarkHarness.cpp)

## Baseline promotion rules

Update this document only when evidence changes:

- link the exact commit and CI run for automated results;
- record the binary commit, environment, device/plugin configuration, steps,
  and result for manual validation;
- preserve failed results until a later result supersedes them;
- never use a different platform or adjacent workflow as substitute evidence.
