# Compiler fixture feasibility is layered from broad defaults to stable path
# conventions and, when needed, exact source overrides.

verona_fixture_defaults(
  VBC_STAGE run)

verona_fixture_rule(
  MATCH "^(v|vir)/compile_only/"
  VBC_STAGE compile)
