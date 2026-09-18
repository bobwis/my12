Warning reduction plan for my12
================================

Checklist (will be followed automatically):

- [ ] Create a working git branch for changes (done: fix/warnings-lowrisk)
- [ ] Collect/confirm compiler warnings by attempting a build (optional; may require toolchain)
- [ ] Triage warnings into categories and pick low-risk, high-impact fixes
- [ ] Implement fixes in small commits, one logical change per commit
- [ ] Rebuild and re-evaluate warnings after each commit
- [ ] Iterate until an acceptable warning level is reached

Summary
-------
This document lists categories of common compiler warnings found in the project, low-risk fixes that can be applied with minimal functional impact, the files where the issues are concentrated (top candidates discovered via an initial search), and a prioritized action plan. The goal is to dramatically reduce warning count while minimizing behavioral risk.

High-level categories (with typical fixes)
-----------------------------------------
1) Format-string mismatches (printf/sprintf/sscanf etc.)
   - Symptoms: warnings about format '%u' expects 'unsigned int' but argument is 'unsigned long' or similar; '%0x' invalid; '%x' vs '%08x'.
   - Fix: correct format specifiers, cast explicitly to the expected integer type when necessary, or change variable type to match formatting use. Prefer explicit casts in printfs rather than changing variable types widely.
   - Risk: low if careful; test logging strings compiled and runtime formatting.

2) Unused variables/unused functions/static functions not used
   - Symptoms: 'defined but not used', 'unused variable'.
   - Fix: remove unused variables, annotate intentionally-unused parameters with (void)param; remove or document unused static functions; mark functions with static only if used.
   - Risk: very low.

3) Implicit function declarations / missing prototypes
   - Symptoms: warning that function is implicitly declared (older C dialects) or missing prototype.
   - Fix: add forward declarations to the appropriate header, or include the proper header file. Avoid changing function signatures.
   - Risk: low if prototypes match definitions.

4) Type conversion / cast warnings (pointer/integer casts, signed/unsigned mismatches)
   - Symptoms: conversion from 'int' to 'char' may change value; signed/unsigned comparison.
   - Fix: add explicit casts where safe, or adjust variable types for clarity. Prefer smallest-local-scope changes.
   - Risk: medium if changing variable types; low if explicit cast only.

5) Uninitialized variable usage
   - Symptoms: may lead to undefined behavior.
   - Fix: initialize variables at declaration or ensure they are set before use.
   - Risk: medium to high if overlooked; treat with care.

6) Format/printf with pointer types (size_t vs %u/%lu)
   - Fix: use correct format macros (e.g., %zu for size_t) or cast to unsigned long and use %lu consistently.

Priority for fixes (order of work)
---------------------------------
1) Remove or silence unused-variable and unused-function warnings (very low risk, high payoff). Files: many (httpclient.c, tftploader.c, etc.)
2) Fix obvious format-string typos (e.g., "%0x" -> "%x"), invalid format flags. Files: Core/Src/eeprom.c, splat1.c, lcd.c, others.
3) Initialize variables that are used before assigned (high importance). Files: as discovered by compiler.
4) Add missing prototypes or includes to silence implicit declaration warnings.
5) Address signed/unsigned or pointer-int casts via explicit casts.

Top candidate files (initial search)
-----------------------------------
- Core/Src/eeprom.c
- Core/Src/splat1.c
- Core/Src/lcd.c
- Core/Src/www.c
- Core/Src/main.c
- Core/Src/httpclient.c
- Core/Src/httploader.c
- Core/Src/neo7m.c
- LWIP/Target/ethernetif.c
- Middlewares/Third_Party/FreeRTOS/Source/tasks.c

Suggested patch workflow
------------------------
1) Create small commits grouped by type of fix and by file. Example commit names:
   - fix(warnings): remove unused variables in httpclient.c
   - fix(format): correct printf format in eeprom.c
   - fix(init): initialize variable in neo7m.c
   - style(annotate): mark intentionally-unused params with (void)foo

2) After each commit, attempt a build and record remaining warnings. If build environment is not available locally, run the build in CI or on the developer machine.

3) Prioritize issues that reduce many warnings with small edits (unused vars, format typos) before riskier type changes.

Concrete low-risk changes to start with
-------------------------------------
- Replace invalid printf specifiers like "%0x" with "%x".
- Add (void)param; for unused function parameters.
- Remove unused local variables or static functions not referenced.
- Initialize variables at declaration, e.g., int rc = 0;
- Use %zu for size_t or cast to (unsigned long) and use %lu where toolchain lacks %zu support.

Next steps I will take if you want me to proceed automatically
-------------------------------------------------------------
1) Run a project build (if toolchain available) and capture full warnings list.
2) Apply the easiest fixes (unused vars, printf typos) across the top candidate files in small commits.
3) Rebuild and iterate.

If you prefer I can begin by applying the low-risk fixes across the files above. Otherwise I will wait for your confirmation.

End of plan.
