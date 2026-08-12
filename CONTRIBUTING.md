# Contributing to SkeletonHive

SkeletonHive welcomes focused contributions that improve a documented user
workflow, correctness, stability, testability, or maintainability.

## Before starting

1. Read docs/INDEX.md.
2. Confirm the requested work appears in docs/ROADMAP.md or discuss the scope
   before implementing it.
3. Review docs/validation/VALIDATED_BASELINE.md so the change begins from the
   recorded project truth.
4. For agent-assisted work, follow AGENTS.md.

## Branches and pull requests

- Fork the repository and branch from the current upstream master.
- Use a descriptive branch such as agent/SH-001-green-build-matrix or
  fix/plugin-scanner-lifetime.
- Keep one work item or cohesive correction in each pull request.
- Do not mix feature work, broad formatting, dependency upgrades, and
  refactoring unless the acceptance criteria require them together.
- Open substantial work as a draft pull request early.
- Never rewrite a published release tag.

## Pull-request requirements

Every pull request should include:

- the problem and work-item ID;
- what changed and what deliberately did not change;
- acceptance criteria;
- automated checks with exact results;
- manual validation completed and still required;
- operating systems, toolchains, audio devices, and plugin formats actually
  exercised;
- real-time or ownership impact;
- documentation and handoff updates.

Screenshots, logs, measurements, or minimal reproduction projects are preferred
over statements such as works for me.

## Definition of done

A change is not done merely because the code exists or compiles. The roadmap
and validated baseline distinguish code presence, successful builds, automated
tests, human workflow validation, cross-platform validation, and release.

Only claim the evidence level actually demonstrated.

## Audio changes

Changes touching processing, transport, Session launch, plugin hosting,
routing, control surfaces, recording, rendering, or shared state must address
the real-time rules in AGENTS.md. Explain:

- which threads execute the changed code;
- whether locks, allocation, waits, or asynchronous callbacks are involved;
- how ownership remains valid during project changes, plugin failures, and
  shutdown;
- how timing or audio correctness was measured.

## Dependency changes

Pin dependency revisions. Do not edit generated dependency trees. Tracktion
Engine corrections belong in checked-in, idempotent patches under
cmake/patches.

## Agent-assisted contributions

Identify the agent/tool used in the pull-request description or commit
trailers. Include the model only when known. Human contributors remain
responsible for reviewing the diff and recording real validation.

## Licensing

By contributing, you agree that your contribution is provided under the
repository's GNU Affero General Public License v3.0. Do not submit code,
assets, presets, models, samples, or captures that you do not have permission
to redistribute.
