# ADR 0001 — JUCE dependency pinning

- Status: accepted
- Date: 2026-08-12
- Owners: SkeletonHive maintainers
- Related work items: SH-001

## Context

Tracktion Engine can fetch JUCE through CPM with `JUCE_CPM_DEVELOP`, which
resolves `juce-framework/JUCE#develop`. A floating develop tip makes CI and
local builds non-reproducible and can silently diverge from the JUCE revision
recorded as Tracktion's `modules/juce` submodule.

SkeletonHive already pins Tracktion Engine and applies checked-in patches under
`cmake/patches`. JUCE must be pinned with the same discipline.

## Decision

1. Fetch JUCE via CMake `FetchContent` at an explicit commit SHA before
   Tracktion Engine is configured.
2. Keep `JUCE_CPM_DEVELOP` forced OFF so Tracktion does not pull
   `JUCE#develop`.
3. Skip Tracktion's SSH JUCE submodule (`GIT_SUBMODULES ""`) and rely on the
   pre-fetched JUCE target (`juce::juce_core`) instead.
4. Record both pins in root `CMakeLists.txt` as
   `SKELETONHIVE_JUCE_GIT_TAG` and `SKELETONHIVE_TRACKTION_GIT_TAG`.
5. When advancing Tracktion Engine, bump the JUCE pin to the
   `modules/juce` submodule SHA of that Tracktion commit in the same change.

Initial pins for SH-001:

- Tracktion Engine: `53a32a4dcc2d73186b6cee4600b5dd51c65a0cae`
- JUCE: `37c894f83d379179b2070d437ccd0f1cd9af9576`
  (submodule SHA of the Tracktion pin above)

## Alternatives considered

- Continue using `JUCE_CPM_DEVELOP` / `#develop` — rejected; fails SH-001
  reproducibility.
- Vendor JUCE under `external/` — workable but heavier; FetchContent pin is
  enough while Tracktion remains FetchContent-based.
- Enable Tracktion's git submodule clone — rejected; the upstream submodule URL
  is SSH-only and breaks unauthenticated CI/local clones.

## Consequences

- Configure/build results are tied to reviewed commits.
- Advancing Tracktion requires an explicit paired JUCE bump and a full CI
  matrix re-run.
- Existing local `build/_deps` trees may need a clean reconfigure after the
  pin lands so CPM's previous `develop` checkout is not reused incorrectly.

## Validation

- Windows, macOS, and Ubuntu CI configure and build against the pinned SHAs.
- README documents FetchContent pins and the patch policy.
- Release tags are cut only from commits whose CI matrix is green.
