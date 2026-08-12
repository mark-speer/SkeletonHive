# SkeletonHive documentation index

This page routes contributors and agents to the smallest authoritative document
set needed for a task. Do not treat one large document as product vision,
roadmap, architecture, validation record, and handoff at the same time.

## Sources of truth

| Question | Authoritative source |
|---|---|
| What is the product trying to become? | VISION.md |
| What must never be compromised? | NEVER.md |
| What work is prioritized now? | ROADMAP.md |
| What has actually been demonstrated? | validation/VALIDATED_BASELINE.md |
| Which environments and workflows were tested? | validation/COMPATIBILITY_MATRIX.md |
| How should a human validate a build? | validation/MANUAL_TESTS.md |
| How is the system currently organized? | ARCHITECTURE.md |
| What happened in the latest work session? | ../AGENT_HANDOFF.md |
| Which architectural choices are binding? | decisions/ |
| How should a contribution be prepared? | ../CONTRIBUTING.md |
| How must coding agents operate? | ../AGENTS.md |

When documents disagree, validation evidence wins for claims about working
behavior, accepted decisions win for architectural constraints, and the
current roadmap wins for priority. Raise unresolved conflicts instead of
silently choosing whichever document supports the desired change.

## Required reading by task

Every task begins with:

- this index;
- validation/VALIDATED_BASELINE.md;
- ROADMAP.md;
- ../AGENT_HANDOFF.md.

Then add only the relevant material:

| Task | Additional required reading |
|---|---|
| Audio engine, Session timing, routing, recording, rendering | NEVER.md, ARCHITECTURE.md, relevant decisions |
| Plugin scanning, loading, sandboxing, editors | NEVER.md, ARCHITECTURE.md sections on plugin hosting, compatibility matrix |
| UI or workflow behavior | VISION.md, NEVER.md, relevant manual tests |
| Persistence, autosave, recovery, project switching | NEVER.md, ARCHITECTURE.md ownership rules, project-lifecycle manual tests |
| Build, CI, dependencies, packaging | CONTRIBUTING.md, compatibility matrix, dependency sections of ARCHITECTURE.md |
| Roadmap or feature proposal | VISION.md, ROADMAP.md, validated baseline |
| Release preparation | CONTRIBUTING.md, validated baseline, compatibility matrix, full manual test plan |

Read a decision record only when its scope intersects the task. Do not ingest
the entire decisions directory by default.

## Document responsibilities

- README.md is a concise public overview and build entry point.
- VISION.md describes product intent, not implementation status.
- ARCHITECTURE.md describes structure and historical implementation decisions,
  not proof that a workflow works.
- ROADMAP.md prioritizes work and defines acceptance evidence.
- VALIDATED_BASELINE.md records only observed evidence.
- AGENT_HANDOFF.md is a replaceable short-term operational summary.

Each pull request should update only the documents whose truth changed.
