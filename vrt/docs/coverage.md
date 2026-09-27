# VRT Coverage

**Current:** `vrt.cmake` registers focused ABI and runtime fixtures. LLVM-native
source and VIR collections provide integration coverage only for explicitly
allowlisted behaviors.

VRT fixtures state native runtime coverage and non-goals in a source header or
adjacent README. A migrated subsystem requires direct VRT coverage and
equivalent native/interpreted behavior where VBC-visible semantics apply.

The `vrt/api/reference` fixture covers reference layout, construction,
ownership, frame epochs, escape, tailcall validation, load, and exchange. The
internal write-barrier fixture covers managed exchange across region-parent
boundaries. The `reference` VIR fixture supplies equivalent VBCI and
LLVM-native integration coverage.

**Migration:** VRT is not yet a complete VBCI replacement. Coverage expansion
follows the [runtime migration policy](../../docs/architecture/vbci-vrt-migration.md).