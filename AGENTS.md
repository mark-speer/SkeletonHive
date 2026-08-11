# SkeletonHive agent instructions

These instructions apply to every automated coding agent working in this
repository. Human task instructions take precedence when they explicitly
conflict with this file.

## Start from repository truth

Do not use prior conversations, remembered plans, or a feature list as proof
of current project state.

Before changing anything:

1. Confirm the current branch, commit, and working-tree status.
2. Read docs/INDEX.md.
3. Read docs/validation/VALIDATED_BASELINE.md, docs/ROADMAP.md, and
   AGENT_HANDOFF.md exactly once.
4. Follow the task-routing table in docs/INDEX.md and read only the additional
   documents relevant to the task.
5. Inspect the affected code and its tests before proposing an implementation.

Do not repeatedly reload unchanged canonical documents during one task. If a
document is already fully available in context and has not changed, use that
copy.

Before implementation, briefly report:

- current validated baseline;
- current roadmap item and acceptance criteria;
- relevant constraints or unresolved validation;
- proposed branch and exact task scope.

Stop if the task conflicts with the validated baseline, current roadmap, or a
documented architectural decision. Report the conflict instead of guessing.

## Scope and branch discipline

- One task or tightly related change set per branch and pull request.
- Use branch names in the form agent/<work-item>-<short-description>.
- Never commit directly to master.
- Do not broaden the task to nearby cleanup, features, or refactors.
- Preserve unrelated changes in a dirty working tree.
- Prefer small, reviewable commits with one coherent purpose.
- Do not move or replace an existing release tag.

## Status and evidence

Code presence is not validation. Use these evidence levels:

1. Proposed
2. Code present
3. Build verified
4. Automated tests passed
5. Human workflow validated
6. Cross-platform validated
7. Released

Never promote a capability beyond the evidence recorded in
docs/validation/VALIDATED_BASELINE.md and the compatibility matrix. A source
inspection may establish Code present; it cannot establish runtime behavior.

Every implementation pull request must identify:

- the work-item ID and acceptance criteria;
- automated checks run and their results;
- manual checks still required;
- platforms and configurations actually tested;
- documentation or handoff updates.

## Audio and real-time constraints

Treat every audio callback and code reachable from it as hard real-time code.

- No locks, blocking waits, heap allocation, filesystem access, logging, UI
  calls, ValueTree mutation, or message-thread callbacks in the audio path.
- Do not use message-thread or UI timers to schedule musical events. Musical
  state changes must be scheduled against engine block/sample time.
- Publish control state to the audio thread through a reviewed non-blocking
  ownership or snapshot mechanism.
- Bound all loops and failure paths in audio processing.
- Prove thread ownership and lifetime across asynchronous callbacks. Do not
  capture a raw this pointer unless destruction cancels or safely invalidates
  every queued callback.
- When behavior is timing-sensitive, add a deterministic test or measurement
  plan before calling it complete.

If a proposed change cannot meet these rules, stop and write an architectural
decision record before implementation.

## Tracktion Engine and project state

- Prefer Tracktion Engine APIs over parallel model, transport, routing, undo,
  or persistence implementations.
- Send every user-visible model mutation through the Edit UndoManager.
- Keep edit lifetime rules explicit; no component may retain an Edit reference
  across project replacement.
- Keep UI code under Source/UI and engine-facing behavior under Source/Engine.
- UI components observe engine/model state; they do not become a second source
  of truth.
- Never hand-edit generated dependency files under build or build/_deps.
  Checked-in patches belong under cmake/patches and must fail loudly when stale.

Read docs/ARCHITECTURE.md for work that changes engine ownership, routing,
persistence, plugin hosting, session behavior, or major UI composition.

## Validation

Run the narrowest useful checks first, followed by all required repository
checks before handoff. If a check cannot run, record exactly why; do not
silently convert it to a pass.

Automated checks do not replace the manual workflows in
docs/validation/MANUAL_TESTS.md. A human pass must identify the binary, host
environment, operating system, audio device where relevant, and observed
result.

## Documentation closeout

Before handing off a completed change:

1. Update docs/validation/VALIDATED_BASELINE.md only with new evidence.
2. Update docs/validation/COMPATIBILITY_MATRIX.md for platforms actually run.
3. Update docs/ROADMAP.md when a work item changes state.
4. Update AGENT_HANDOFF.md with the exact branch, commit, completed work,
   pending checks, and next recommended action.
5. Keep README.md descriptive; do not use it as the authoritative status
   ledger.

## Agent provenance

When an agent materially authors a commit, record the agent/tool in a commit
trailer or pull-request description. Record the model only when it is known
from the active session. Never infer a model from writing style, tool choice,
or prior commits.
