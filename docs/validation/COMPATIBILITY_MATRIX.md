# Compatibility matrix

This matrix records tested combinations. Blank or Not recorded means unknown,
not unsupported and not passing.

Status terms:

- Pass — the named workflow was observed successfully at the linked commit.
- Fail — the named workflow was attempted and did not meet its pass criteria.
- Partial — some stated steps passed, but the complete workflow did not.
- Not recorded — no repository evidence exists.
- Not applicable — the combination is intentionally outside the test.

## Automated build matrix

Baseline:
8db7a68409a97d3ddd9dec16db1f61971c2c621f

Run: [CI 31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990)  
PR: [#2](https://github.com/mark-speer/SkeletonHive/pull/2)

| OS / runner | Toolchain | Configure | Build | Package | Evidence |
|---|---|---:|---:|---:|---|
| Windows latest | Visual Studio 18 2026, Release | Pass | Pass | Pass | [CI 31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990) |
| Ubuntu latest | Ninja, Release | Pass | Pass | Pass | [CI 31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990) |
| macOS latest | Ninja, Release | Pass | Pass | Pass | [CI 31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990) |

Pins: Tracktion `53a32a4d…`, JUCE `37c894f8…` (ADR 0001).

### Release packaging policy

- Published `v*` tags are immutable.
- Cut releases only from commits whose Windows + macOS + Ubuntu CI matrix is green.
- The Release workflow re-runs that matrix, then uploads **Windows** portable
  zip assets only. macOS/Linux release archives are not published yet.

## Application and audio-device matrix

| OS | Launch | Built-in output | Low-latency backend | Record audio | Record MIDI | Long-session stability |
|---|---:|---:|---:|---:|---:|---:|
| Windows | Not recorded | Not recorded | ASIO: Not recorded | Not recorded | Not recorded | Not recorded |
| macOS | Not recorded | Not recorded | Core Audio: Not recorded | Not recorded | Not recorded | Not recorded |
| Linux | Not recorded | Not recorded | ALSA: Not recorded | Not recorded | Not recorded | Not recorded |

Record interface model, driver version, sample rate, buffer size, and input/output
configuration with each result.

## Plugin-host matrix

| Format / mode | Windows | macOS | Linux | Required evidence |
|---|---:|---:|---:|---|
| Tracktion/JUCE native devices | Not recorded | Not recorded | Not recorded | Insert, edit, automate, save/reopen, render |
| VST3 effect in process | Not recorded | Not recorded | Not recorded | Scan, load, parameters, automation, state, editor, render |
| VST3 instrument in process | Not recorded | Not recorded | Not recorded | MIDI input, audio output, state, editor, render |
| VST3 effect sandboxed | Not recorded | Not recorded | Not recorded | Audio, parameters, state, editor, crash/hang/restart |
| VST3 sidechain | Not recorded | Not recorded | Not recorded | Routing, audio result, save/reopen |
| VST3 multi-output | Not recorded | Not recorded | Not recorded | Bus selection, separation, save/reopen, render |
| Audio Unit | Not applicable | Not recorded | Not applicable | Scan, effect/instrument, state, editor, render |

Name the exact plugin and version. One plugin does not establish format-wide
compatibility; retain a list of representative plugins behind each summary.

## Project reliability matrix

| Workflow | Windows | macOS | Linux |
|---|---:|---:|---:|
| New project, save, close, reopen | Not recorded | Not recorded | Not recorded |
| Save As and external-change warning | Not recorded | Not recorded | Not recorded |
| Autosave and recovery | Not recorded | Not recorded | Not recorded |
| Two-instance project lock | Not recorded | Not recorded | Not recorded |
| Missing or failed plugin recovery | Not recorded | Not recorded | Not recorded |
| WAV export | Not recorded | Not recorded | Not recorded |
| FLAC export | Not recorded | Not recorded | Not recorded |
| Session-to-Arrangement capture | Not recorded | Not recorded | Not recorded |

## Evidence entry

For every new result record:

- commit and binary configuration;
- date and tester;
- OS and toolchain;
- audio interface, driver, sample rate, and buffer size where relevant;
- plugin name/version and sandbox mode where relevant;
- manual-test IDs performed;
- Pass, Fail, or Partial result;
- linked CI run, log, screenshot, recording, or issue.
