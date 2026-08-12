# Validated baseline

Evidence date: 2026-08-11

Baseline commit:
3b6579643ad608710d02abe8ec7338286475b2f0

This document records demonstrated behavior only. It does not infer runtime
correctness from source files, controls, feature lists, or successful
compilation on another platform.

## Automated build evidence

GitHub Actions run:
[CI run 31539254267](https://github.com/mark-speer/SkeletonHive/actions/runs/31539254267)

| Environment | Configure | Build | Package | Evidence result |
|---|---:|---:|---:|---|
| Windows latest, Visual Studio 18 2026, Release | Pass | Pass | Pass | Build and artifact upload verified |
| Ubuntu latest, Unix Makefiles, Release | Fail | Not run | Not run | Configure failure |
| macOS latest, Xcode, Release | Pass | Fail | Not run | Compilation failure |

The associated Windows release job also passed configuration, build,
packaging, and asset upload:
[release run 31539258393](https://github.com/mark-speer/SkeletonHive/actions/runs/31539258393).

This establishes a Windows CI build/package result. It does not establish that
the application launches, produces correct audio, preserves projects, or
works with a particular device or plugin.

### Observed build blockers

- Ubuntu dependency discovery fails during JUCE configuration; the job does
  not currently reach application compilation.
- macOS reaches application compilation but fails on the current
  std::atomic<std::shared_ptr<nam::DSP>> usage.

Exact fixes and supported toolchains belong to SH-001 and must be validated by
new runs before this table changes.

## Automated test evidence

- No repository-level CTest registration or automated unit/integration test
  suite was found at the baseline commit.
- CMake defines a NAM smoke-test executable, but it is not registered as a
  repository test or run by the current CI workflow.
- Debug stress commands log information but do not enforce pass/fail
  thresholds.

Current automated behavioral evidence: none recorded.

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

| ID | Observation | Evidence to obtain |
|---|---|---|
| RT-01 | SessionManager starts a 30 Hz timer and processes pending launches from timerCallback, allowing musical launch detection to lag a polling interval. | Deterministic launch-offset measurement and scheduler design |
| RT-02 | SandboxedPluginInstance obtains a SpinLock-protected coordinator snapshot from processBlock. | Audio-thread trace/review and non-blocking replacement |
| SBX-01 | Plugin sandboxing defaults on for supported VST3 effects while host-side parameter automation, sidechain/multi-output, instruments, AU, and some platform behavior remain deferred. | Explicit support matrix and workflow tests |
| LIFE-01 | PluginScanner queues message-thread callbacks capturing raw this after worker activity. | Shutdown/destruction reproduction and lifetime-safe callback design |
| PERF-01 | EngineBenchmarkHarness records diagnostic timings without thresholds; one cache-stat helper returns a placeholder value. | Measurable workloads and pass/fail criteria |
| DOC-01 | The accumulated architecture roadmap contains conflicting phase summaries and obsolete deferred-work statements. | Current-state architecture and evidence-based roadmap review |

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
