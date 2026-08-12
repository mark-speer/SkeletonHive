# Architectural decision records

Use a decision record when a change establishes a durable constraint, accepts
a material tradeoff, or deviates from the normal JUCE/Tracktion Engine model.
Do not create a decision record for routine implementation detail.

## Naming

Use sequential names:

0001-short-decision-title.md

Accepted records are immutable except for spelling or link corrections.
Supersede a decision with a new record and link both directions.

## Template

# ADR NNNN — Title

- Status: proposed / accepted / superseded / rejected
- Date:
- Owners:
- Related work items:

## Context

What problem, evidence, constraints, and affected threads or workflows require
a decision?

## Decision

What is being chosen? State timing, ownership, failure, compatibility, and
migration behavior where relevant.

## Alternatives considered

What credible alternatives were evaluated and why were they not selected?

## Consequences

List benefits, costs, risks, compatibility boundaries, and required tests.

## Validation

Define automated evidence, human workflows, platforms, measurements, and
rollback criteria.

## Accepted decisions

| ID | Title |
|---|---|
| [0001](0001-juce-dependency-pinning.md) | JUCE dependency pinning |

## Initial decisions likely needed

- Engine/block scheduling model for Session launches.
- Non-blocking ownership publication for sandbox coordinators.
- Supported sandbox capabilities and default behavior.
- Testable engine/library boundary.
- Official platform, audio-backend, and release support matrix.
