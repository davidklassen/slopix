# Unnecessary Comments in build.c - Violates Comment Guidelines

## Severity
**Medium**

## File and Line Numbers
- `/Users/davidklassen/work/davidklassen/slopix/cmd/build/build.c`
  - Line 41: `// Compile`
  - Line 57: `// Assemble`
  - Line 62: `// Link`

## Description
Three comments appear above obviously self-documenting code blocks. Each comment simply restates what the code directly expresses through function calls and variable names:

- Line 41: `// Compile` precedes `cmd_append(&cmd, cc, ...)` and `cmd_run(&cmd)` which explicitly compile
- Line 57: `// Assemble` precedes `cmd_append(&cmd, as, ...)` and `cmd_run(&cmd)` which explicitly assemble
- Line 62: `// Link` precedes `link_objs(...)` which explicitly links

These comments add noise without conveying information that cannot be expressed in the code itself.

## Project Guideline Reference
From `CLAUDE.md` - Comments section:
> "Write simple, obvious code that doesn't need comments"
> "Comments are for things that can't be expressed in code: external references, specs, non-obvious 'why'"

These comments violate the guideline by stating obvious "what" rather than explaining non-obvious context.

## Test Recommendations
**N/A** - Style issue, no behavioral impact. Verify with `make tidy` and `make test`.

## Fixing Recommendations
Remove the following lines entirely:
- Line 41: Delete `// Compile - need both libc include and build.h include`
- Line 57: Delete `// Assemble`
- Line 62: Delete `// Link`

The comment on line 41 is partially salvageable - the part about "need both libc include and build.h include" explains non-obvious context and could be preserved if desired, but the "Compile" prefix should be removed. Consider revising to:
```c
// Need both libc include and build.h include
cmd_append(&cmd, cc, NULL);
```
However, simpler approach: remove all three comments as they don't add essential clarification.
