# Agent handoff

Last updated: 2026-08-12

This file is the short operational handoff. It must describe the repository as
it is, not the intended product or remembered conversation.

## Repository state

- Upstream: mark-speer/SkeletonHive
- Upstream default branch: master
- Working branch: `agent/SH-001-green-build-matrix`
- Tip commit: `8db7a68409a97d3ddd9dec16db1f61971c2c621f`
- Pull request: https://github.com/mark-speer/SkeletonHive/pull/2
- Work item: SH-001 — restore a reproducible green build matrix (**acceptance met**)
- Change type: build fix (NamPlugin publish), JUCE pin, release gates, Phase 0 docs

## Evidence recorded

- CI [31555627990](https://github.com/mark-speer/SkeletonHive/actions/runs/31555627990):
  Windows, macOS, and Ubuntu configure/build/package **Pass** on `8db7a68`.
- Local Windows Release builds against both the prior tree and a fresh
  pinned-JUCE configure (`build-sh001/`) succeeded.
- No application launch, audio, plugin, or save/recovery manual evidence yet.

See docs/validation/VALIDATED_BASELINE.md and COMPATIBILITY_MATRIX.md.

## Files / subsystems changed

- `Source/Engine/Effects/NamPlugin.*` — portable atomic shared_ptr publish
- `CMakeLists.txt` — FetchContent JUCE pin; `JUCE_CPM_DEVELOP` OFF
- `docs/decisions/0001-juce-dependency-pinning.md` — ADR
- `.github/workflows/release.yml` — three-OS matrix gate before Windows assets
- README, CONTRIBUTING, ROADMAP Phase 0, validation docs, this handoff

## Human validation

- Not run for product workflows (launch/audio/plugins/save).

## Validation still pending

- Branch protection / required checks on master (GitHub settings; confirm by
  maintainer).
- Product manual tests in docs/validation/MANUAL_TESTS.md.

## Next recommended action

Merge PR #2 when ready, then open **SH-002** on
`agent/SH-002-plugin-scanner-lifetime` (PluginScanner lifetime-safe async
callbacks / LIFE-01).

Queued after that: SH-003 (sandbox SpinLock) → SH-004 (sandbox default) →
SH-005 (Session launch ADR + TE-time scheduling) → SH-006 (CTest baseline).

Do not resume Phase 11 Tiers 2–4 or Phase 12 feature work until Phase 0
stabilization items are cleared or explicitly deferred by the maintainer.

## Closeout template

Replace the sections above at the end of each task with:

- exact branch and commit;
- work item and acceptance criteria completed;
- files or subsystems changed;
- automated checks and results;
- human validation completed;
- validation still pending;
- known risks or follow-up items;
- next recommended roadmap item.
