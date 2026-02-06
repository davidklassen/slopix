# Dead Code: Unused `nonprint` Variable in editorUpdateRow

## Severity
**Medium**

## File and Line Numbers
- **File:** `/Users/davidklassen/work/davidklassen/slopix/cmd/ed/ed.c`
- **Lines:** 656, 669

## Description

The `editorUpdateRow()` function contains an unused variable `nonprint` that is:

1. **Declared and initialized to 0** (line 656):
   ```c
   unsigned int tabs = 0, nonprint = 0;
   ```

2. **Used in allocation size calculation** (line 669):
   ```c
   unsigned long long allocsize = (unsigned long long)row->size + tabs * 8 + nonprint * 9 + 1;
   ```
   And again on line 675:
   ```c
   row->render = malloc(row->size + tabs * 8 + nonprint * 9 + 1);
   ```

3. **Never incremented anywhere** - The only loop that processes row characters (lines 662-666) only counts TAB characters:
   ```c
   for (j = 0; j < row->size; j++) {
       if (row->chars[j] == TAB) {
           tabs++;
       }
   }
   ```

4. **Feature is unimplemented** - The function comment (line 660) states the intended behavior:
   ```c
   // respecting tabs, substituting non printable characters with '?'.
   ```
   However, the rendering loop (lines 677-686) only handles TAB expansion and never substitutes non-printable characters or allocates space for them.

## Current Behavior

- The `nonprint` variable always remains 0 throughout execution
- The memory allocation always uses the minimum size needed without accounting for non-printable character expansion
- Non-printable characters are rendered in-place without any expansion or substitution in the render buffer
- Non-printable character display is handled only at rendering time (lines 1056-1064), not at buffer preparation time

## How to Verify

1. Open a file containing non-printable characters in ed
2. The non-printable characters will render correctly with special formatting (inverse video and symbolic representation)
3. Remove or reduce the `nonprint * 9` term from the allocation - behavior remains identical
4. The variable never appears anywhere else in the codebase

## Test Recommendations

1. Open a file with embedded control characters (e.g., `\x01`, `\x02`, etc.)
2. Verify display renders correctly
3. Verify no buffer overflows occur
4. Check memory usage is not wasteful

## Fixing Recommendations

Choose one approach:

### Option A: Remove Dead Code (Recommended)
The feature described in the comment is unimplemented. If non-printable substitution is not needed:
1. Remove the `nonprint` variable declaration
2. Simplify the allocation to: `(unsigned long long)row->size + tabs * 8 + 1`
3. This matches the actual rendering behavior which doesn't expand non-printables

### Option B: Implement the Feature
If non-printable character substitution is desired (expand `\x01` to `^A`, etc.):
1. Add a loop to count non-printable characters: increment `nonprint` when `!isprint(row->chars[j])`
2. Modify the rendering loop (lines 677-686) to substitute non-printable characters
3. Each non-printable could expand to up to 3 bytes (`^X` format), so adjust multiplier if needed
4. Update the comment to clarify the actual format

## Impact Assessment

- **Correctness:** No impact - allocation is conservative and still sufficient
- **Performance:** Minor - unnecessary multiplication by zero in allocation calculation
- **Maintainability:** Confusing due to code/comment mismatch
