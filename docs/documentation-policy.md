# Documentation Policy

This repository separates documentation by audience and authority. A fact
should have one authoritative home; other documents link to it instead of
copying it.

## Documentation Areas

| Area | Audience | Owns |
| --- | --- | --- |
| `README.md` | New contributors and users | Project purpose, status, quick start, and component map |
| Component `README.md` files | Contributors entering one component | Scope, interfaces, layout, build targets, and links |
| `vc/docs/` | Verona programmers | Language syntax, semantics, user-facing tools, and examples |
| `docs/architecture/` | Compiler and runtime contributors | Stable decisions and evolving cross-component policies |
| `docs/formats/` | Producers and consumers of interchange formats | VIR and VBC contracts, versioning, and compatibility |
| `testsuite/` documentation | Test authors | Test pipelines, fixture coverage, and golden-file conventions |
| Source comments | Maintainers of an implementation | Local invariants that are clearer beside the code |

## Authority Rules

1. Language semantics belong in `vc/docs/`. Runtime or compiler documents may
   explain their implementation, but must link to the language definition.
2. Cross-component decisions belong in architecture decision records (ADRs).
   ADRs record context, a decision, and consequences; they are not edited into
   a running implementation guide after acceptance.
3. Operational architecture that is expected to evolve belongs in a named
   policy document under `docs/architecture/`, not in an ADR.
4. VIR and VBC are interchange contracts. Their definitions belong under
   `docs/formats/` and in neutral public headers, not in producer or consumer
   documentation.
5. A component README is a landing page. Detailed language, format, or test
   material should be linked rather than duplicated there.
6. Test documentation states only behavior that the fixture actually checks.
   Planned coverage is identified as planned, not implied by a fixture name.

## Status Language

Documentation describing incomplete work must use one of these labels:

- **Current:** behavior present in the repository.
- **Migration:** compatibility behavior retained while ownership or naming
  changes.
- **Target:** accepted direction that is not fully implemented.
- **Planned:** proposed work without an accepted compatibility commitment.

Documents may combine current and target descriptions, but each must be easy
to distinguish.

## Change Checklist

When a change affects:

- Verona syntax or semantics, update the relevant `vc/docs/` chapter.
- CLI options, update `vc/docs/21-toolchain-usage.md` and the owning component
  README.
- VIR or VBC structure, update the neutral header, the corresponding format
  document, producers, consumers, and format tests.
- Component boundaries or dependencies, add or supersede an ADR and update the
  architecture index.
- Backend support, update `docs/architecture/vc-backends.md` and the relevant
  test pipeline documentation.
- Runtime ownership, update `docs/architecture/vbci-vrt-migration.md`.

Before merging, verify relative links, commands, target names, and status claims
against the current source tree.