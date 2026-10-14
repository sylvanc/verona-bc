---
name: docs-maintenance
description: "Maintain Verona repository documentation ownership and links. Use when reorganizing docs, changing component boundaries, adding ADRs, changing VIR or VBC formats, updating backend policy, or moving implementation detail out of the language manual."
user-invocable: false
---

# Documentation Maintenance

Use this workflow for repository architecture, component, format, and test
documentation. Use the `docs-update` skill for Verona language features.

## 1. Find the Authoritative Owner

Read `docs/documentation-policy.md` before editing. Place material according to
its audience:

- language syntax and semantics: `vc/docs/`;
- stable cross-component decisions: an ADR in `docs/architecture/`;
- evolving backend or migration constraints: an architecture policy;
- VIR or VBC contracts: `docs/formats/` and neutral headers;
- component orientation: the component's `README.md`;
- fixture behavior and test mechanics: `testsuite/` documentation.

Do not duplicate an authoritative explanation. Add a short context sentence
and link to it.

## 2. Verify the Implementation

Read the owning source, CMake targets, CLI option definitions, and test
registration before making factual claims. Distinguish `Current`, `Migration`,
`Target`, and `Planned` behavior as defined by the policy.

For an ADR, record context, decision, compatibility, consequences, and rejected
alternatives. Do not turn an ADR into a frequently edited implementation guide.

## 3. Update Related Entry Points

Check whether the change requires updates to:

- `README.md` or a component README;
- `docs/architecture/README.md`;
- `docs/formats/README.md`;
- `vc/docs/20-compiler-pipeline.md` or
  `vc/docs/21-toolchain-usage.md`;
- testsuite pipeline and fixture coverage documentation;
- `AGENTS.md` only for concise durable workflow facts.

## 4. Validate

Verify every relative Markdown link resolves. Confirm command names, CMake
targets, paths, defaults, and status labels against the current tree. Run the
narrowest executable build or test when documentation accompanies code.

## 5. Review

Check that user documentation does not absorb implementation architecture,
component READMEs remain concise, and future plans are not described as current
behavior.