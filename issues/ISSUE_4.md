# Bug: Row Index Increment Instead of Decrement in editorDelRow()

## Severity
**Critical**

## Location
File: `/Users/davidklassen/work/davidklassen/slopix/cmd/ed/ed.c`
Lines: 738-740

## Description
In the `editorDelRow()` function, after deleting a row, the remaining rows' indices are incorrectly incremented (`E.row[j].idx++`) instead of being decremented (`E.row[j].idx--`). This violates the invariant that `row.idx` should correspond to the row's position in the `E.row` array.

When a row is deleted, all rows after it should shift upward in the array, meaning their indices should decrease by 1. The current code increments them instead, causing all subsequent row indices to be incorrect.

### Root Cause
Line 739 reads:
```c
E.row[j].idx++;
```

This should be:
```c
E.row[j].idx--;
```

The bug becomes apparent when comparing with `editorInsertRow()` (lines 703-705), which correctly increments indices when rows are inserted:
```c
for (int j = at + 1; j <= E.numrows; j++) {
    E.row[j].idx++;
}
```

When rows shift down due to insertion, indices increase. When rows shift up due to deletion, indices should decrease—not increase.

## How to Reproduce
1. Open a file with multiple lines in the editor
2. Delete a row from the middle of the file (e.g., using Ctrl-H to backspace at line start, merging with previous line)
3. Check the row indices of remaining rows
4. Observe that row indices are incorrect: they should decrease but instead they increase

The manifestation depends on code paths that rely on `row.idx`:
- Syntax highlighting that checks `row.idx > 0` for multi-line comment tracking (line 486)
- Any code that validates row indices against array positions

## Test Recommendations
Automated test:
1. Insert three rows with content "Line 1", "Line 2", "Line 3"
2. Delete the middle row
3. Assert that the remaining rows have indices 0 and 1 (not 0 and 2)
4. Assert that row.idx matches the row's actual position in E.row array for all rows
5. Test edge cases: delete first row, delete last row, delete from single-row file

## Fixing Recommendation
Change line 739 from:
```c
E.row[j].idx++;
```

To:
```c
E.row[j].idx--;
```

The complete corrected loop should be:
```c
for (int j = at; j < E.numrows - 1; j++) {
    E.row[j].idx--;
}
```

This ensures that when rows shift upward in the array after deletion, their indices are decremented to maintain the invariant that `row.idx` equals the row's position in the array.
