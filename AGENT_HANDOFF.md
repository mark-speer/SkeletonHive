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
- Current work item: SH-001 — restore a reproducible green build matrix
- Change type: build fix (NamPlugin publish), JUCE pin, release gates, Phase 0 docs

## Scope completed on this branch (pending commit/CI closeout)

- Replaced `std::atomic<std::shared_ptr<nam::DSP>>` in `NamPlugin` with portable
  `std::atomic_*` free functions on a plain `shared_ptr` (macOS libc++ compile
  blocker from PR #1 / VALIDATED_BASELINE).
- Pinned JUCE via FetchContent to Tracktion's `modules/juce` submodule SHA;
  forced `JUCE_CPM_DEVELOP` OFF; added ADR 0001.
- Release workflow now re-validates Windows + macOS + Ubuntu before uploading
  Windows assets; README/CONTRIBUTING document immutable tags and green-only
  releases.
- ROADMAP Phase 0 lists SH-001…SH-006; follow-ups queued after SH-001.

## Evidence currently recorded

- Local Windows Release build of `SkeletonHive` succeeded after the NamPlugin
  change (existing `build/` tree).
- Fresh Windows configure with pinned JUCE succeeded (`build-sh001/`).
- Prior master CI (`30dd538`): Windows Pass, Ubuntu Pass, macOS Build Fail.
- Three-OS CI on this branch: **not yet run** (requires push of the branch
  commit). Do not mark SH-001 complete or promote macOS to Pass until that
  matrix is green and linked here / in VALIDATED_BASELINE.

See docs/validation/VALIDATED_BASELINE.md and COMPATIBILITY_MATRIX.md.

## SH-001 acceptance checklist

1. Windows, macOS, and Ubuntu configure and build on pinned dependencies —
   **local Windows yes; macOS/Ubuntu pending CI**.
2. Required CI checks protect master — **unchanged; still required on GitHub**.
3. Build instructions match the dependency mechanism actually used — **done
   (README + ADR 0001)**.
4. Release tags immutable; releases only from green commits — **documented;
   Release workflow matrix gate added**.
5. Validated baseline and compatibility matrix link passing runs — **partial;
   update with this branch's CI URL when green**.

## Known risks awaiting triage (queued)

Do not start these on the SH-001 branch. After SH-001 merges:

1. **SH-002** — PluginScanner lifetime (LIFE-01)
2. **SH-003** — Sandbox coordinator SpinLock (RT-02)
3. **SH-004** — Sandbox default / support matrix (SBX-01)
4. **SH-005** — Session launch ADR then TE-time scheduling (RT-01)
5. **SH-006** — Automated behavioral test baseline

## Next recommended action

1. Commit and push `agent/SH-001-green-build-matrix`.
2. Confirm GitHub Actions CI is green on Windows, macOS, and Ubuntu.
3. Update VALIDATED_BASELINE / COMPATIBILITY_MATRIX / this handoff with the
   exact commit and CI run URL; mark SH-001 Done in ROADMAP.
4. Open SH-002 (`agent/SH-002-plugin-scanner-lifetime`) as the next PR.

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
