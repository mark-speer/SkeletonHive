# Agent handoff

Last updated: 2026-08-11

This file is the short operational handoff. It must describe the repository as
it is, not the intended product or remembered conversation.

## Repository state

- Upstream: mark-speer/SkeletonHive
- Upstream default branch: master
- Baseline commit used to initialize this framework:
  3b6579643ad608710d02abe8ec7338286475b2f0
- Current work item: SH-000 — establish repository truth and contribution
  controls
- Working branch: agent/agent-operating-framework
- Change type: documentation and process only

Before continuing from this handoff, fetch upstream and confirm that the
baseline commit is still current. Rebase or merge deliberately if it is not.

## Evidence currently recorded

- Windows Release configuration, build, portable packaging, and artifact
  upload passed in GitHub Actions for the baseline commit.
- The Windows release workflow also built and uploaded release assets.
- Ubuntu failed during CMake configuration.
- macOS configured but failed during compilation.
- No repository-level automated unit or integration test suite is registered.
- No repeatable manual product-validation results are recorded in the
  repository.

See docs/validation/VALIDATED_BASELINE.md for evidence links and details.

## Scope completed by SH-000

- Establish a mandatory, token-conscious agent reading order.
- Separate product direction, architecture inventory, roadmap priority, and
  validation evidence.
- Add contribution and pull-request expectations.
- Define a stabilization roadmap before additional feature expansion.
- Preserve the existing architecture document as implementation history while
  clarifying that its implemented labels are not validation claims.

## Known risks awaiting triage

These are review observations, not accepted defects until reproduced or
confirmed:

- Session launches are detected by a 30 Hz timer rather than scheduled at
  engine block/sample time.
- The sandboxed plugin audio callback obtains a SpinLock-protected
  coordinator snapshot.
- Plugin sandboxing defaults on while automation, sidechain, multi-output, and
  other bridge capabilities remain incomplete.
- PluginScanner queues asynchronous callbacks that capture a raw this pointer.
- The benchmark harness logs timings but does not enforce thresholds or prove
  all asynchronous work completed.

## Next recommended action

SH-001 — restore a reproducible green build matrix.

Acceptance requires:

1. Windows, macOS, and Ubuntu configure and build on pinned dependencies.
2. Required CI checks protect master.
3. Build instructions match the dependency mechanism actually used.
4. Release tags are immutable and releases are created only from a green
   commit.
5. The validated baseline and compatibility matrix link the passing runs.

Do not begin another feature phase while SH-001 is incomplete.

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
