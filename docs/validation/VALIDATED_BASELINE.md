# Validated baseline

Evidence date: 2026-08-12

Baseline commit:
pending — SH-001 branch `agent/SH-001-green-build-matrix` (update to the
merge commit SHA and CI run once the three-OS matrix is green on that commit)

Prior framework baseline (PR #1 documentation only):
3aa4228f3a802bb66697790c36af533a70bee96c

This document records demonstrated behavior only. It does not infer runtime
correctness from source files, controls, feature lists, or successful
compilation on another platform.

## Automated build evidence

### SH-001 local Windows evidence (NamPlugin publish fix)

| Environment | Configure | Build | Package | Evidence result |
|---|---:|---:|---:|---|
| Windows 10/11, Visual Studio 18 2026, Release (`build/`) | Pass (existing tree) | Pass | Not run | Incremental Release build of `SkeletonHive` after replacing `std::atomic<std::shared_ptr<nam::DSP>>` with `std::atomic_*` shared_ptr publish |
| Windows 10/11, Visual Studio 18 2026 (`build-sh001/`) | Pass | Not run | Not run | Fresh configure with pinned JUCE FetchContent (`37c894f8…`) and `JUCE_CPM_DEVELOP=OFF` |

### Pre-SH-001 CI (master after Linux/Ninja fixes; macOS still red)

GitHub Actions run on `30dd538` (pre-framework tip that fixed Ubuntu; macOS Build failed):
[CI run 31552281072](https://github.com/mark-speer/SkeletonHive/actions/runs/31552281072)

| Environment | Configure | Build | Package | Evidence result |
|---|---:|---:|---:|---|
| Windows latest, Visual Studio 18 2026, Release | Pass | Pass | Pass | Build and artifact upload verified |
| Ubuntu latest, Ninja, Release | Pass | Pass | Pass | Artifact uploaded |
| macOS latest, Ninja, Release | Pass | Fail | Not run | Build failure; artifact `build-log-macos-latest` |

Older baseline at `3b65796` (Ubuntu configure fail, macOS compile fail under Xcode generator) is superseded for Ubuntu by the Ninja/deps fixes above. macOS remains Fail until SH-001 CI is green.

### Observed build blockers (historical → SH-001)

- Ubuntu dependency discovery previously failed during JUCE configuration; fixed
  on master by completing Linux apt deps and switching CI to Ninja (`8f000ba`).
- macOS previously failed on `std::atomic<std::shared_ptr<nam::DSP>>` (Apple
  libc++). SH-001 replaces that with portable `std::atomic_load` /
  `std::atomic_store` / `std::atomic_exchange` /
  `std::atomic_compare_exchange_strong` on a plain `shared_ptr` in
  `NamPlugin`. Cross-platform CI confirmation is still required before marking
  macOS Pass.
- JUCE previously floated on CPM `#develop`; SH-001 pins JUCE to the Tracktion
  `modules/juce` submodule SHA (ADR 0001).

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
