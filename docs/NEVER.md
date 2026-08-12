# SkeletonHive non-negotiable constraints

These rules protect audio correctness, user work, and repository truth. A
change that genuinely requires an exception must first document and accept an
architectural decision.

## Never confuse implementation with validation

- Never call a feature complete because classes, controls, or menu items exist.
- Never convert a skipped, unavailable, flaky, or manually unobserved check
  into a pass.
- Never advertise cross-platform support from one successful platform.
- Never allow README or roadmap claims to outrun the validated baseline.

## Never violate the audio thread

- Never take a lock or perform a blocking wait in an audio callback.
- Never allocate, access the filesystem, log, mutate ValueTrees, or call UI or
  message-thread code from the audio path.
- Never schedule a musical event from a UI/message-thread polling timer.
- Never introduce unbounded processing into an audio callback.
- Never replace a measurable timing requirement with a visual smoke test.

## Never create ambiguous ownership

- Never retain an Edit reference in an object that can outlive project
  replacement.
- Never capture raw object lifetime across asynchronous work without a
  cancellation, weak-reference, or equivalent invalidation strategy.
- Never let the UI become a second source of truth for engine/model state.
- Never bypass the Edit UndoManager for a user-visible model mutation.

## Never hide failure

- Never silently fall back from a requested plugin, device, format, or routing
  mode to materially different behavior.
- Never enable an incomplete experimental subsystem by default without clear
  compatibility boundaries and recovery behavior.
- Never discard or overwrite a project to recover from a parsing, migration,
  plugin, or external-change error.

## Never undermine reproducibility

- Never follow an unpinned dependency branch in a release build.
- Never hand-edit build, build/_deps, or another generated dependency tree.
- Never merge red required checks.
- Never publish a release from a commit that fails its supported-platform
  matrix.
- Never move, delete, or replace a published release tag.

## Never let an agent invent scope or history

- Never treat prior chat as more authoritative than the repository.
- Never let an agent expand one work item into nearby features or cleanup
  without explicit approval.
- Never infer which model produced a commit when the session did not record it.
- Never close a task without an exact handoff of completed and pending
  validation.
