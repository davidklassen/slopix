# Cross-Compiler Roadmap (Phase 1 of CC.md)

A detailed implementation plan for porting chibicc to AArch64 as a Slopix cross-compiler.

## Overview

**Goal**: Build `tools/cc/chibicc` that replaces `aarch64-elf-gcc` for compiling cmd/ programs.

**Scope**: This roadmap covers Phase 1 only - a cross-compiler running on the host that:
- Takes C source files as input
- Emits AArch64 assembly
- Uses host `aarch64-elf-as` and `aarch64-elf-ld` for assembly/linking

**Key Reference**: Existing AArch64 ports provide implementation patterns:
- [xhackerustc/chibicc-aarch64](https://github.com/xhackerustc/chibicc-aarch64)
- [derbuihan/chibicc_arm64](https://github.com/derbuihan/chibicc_arm64)

---

## Test-Driven Approach

Tests run on Slopix via initramfs, same as `cmd/tests/`. This ensures we test the real target environment from the start.

The chibicc test suite is designed for incremental feature testing. Each test file exercises specific language features:

| Test File | Features | Validates Step |
|-----------|----------|----------------|
| `arith.c` | +, -, *, /, %, bitwise, comparisons | Step 5 |
| `variable.c` | Local variables, assignment | Step 6 |
| `control.c` | if, for, while, switch, goto | Step 6 |
| `function.c` | Function calls, parameters, recursion | Step 7 |
| `pointer.c` | Pointers, arrays, address-of | Step 8 |
| `cast.c`, `usualconv.c` | Type conversions | Step 9 |
| `float.c` | Floating-point operations | Step 10 |
| `struct.c`, `union.c`, `bitfield.c` | Structs, unions, bitfields | Step 11 |
| `sizeof.c`, `alignof.c`, `offsetof.c` | Size/alignment operators | Step 11 |
| `literal.c`, `string.c`, `unicode.c` | String/char literals | Step 8 |
| `enum.c`, `typedef.c`, `typeof.c` | Type declarations | Step 6-8 |
| `varargs.c` | Variadic functions | Step 7 |
| `initializer.c`, `constexpr.c` | Initializers | Step 8 |
| `vla.c` | Variable-length arrays | Step 11 |

**Strategy**: Set up Slopix integration and test infrastructure early (Steps 3-4), then use chibicc tests to validate each codegen step. All supported tests must pass - failures indicate bugs, not acceptable gaps.

**Libc Testing**: When adding functions to Slopix libc (e.g., `memcmp()`), also add corresponding tests to `cmd/tests/` to verify the implementation works correctly on Slopix.

**Unsupported Features** (tests skipped, not failed):
- `long double` (80-bit x87) - AArch64 uses 128-bit IEEE quad, different semantics
- TLS (`__thread`, tls.c) - requires OS threading support not in Slopix
- Atomics (atomic.c) - requires pthreads and kernel support
- Inline assembly (asm.c) - x86-64 specific, would need AArch64 rewrite
- alloca.c - requires `alloca()` builtin implementation

---

## Step 1: Project Setup

**Goal**: Copy chibicc source to `tools/cc/` and verify it builds on the host.

### Work

1. Copy chibicc source files to `tools/cc/`:
   ```
   tools/cc/
   ├── Makefile
   ├── chibicc.h
   ├── main.c
   ├── codegen.c      (will be rewritten)
   ├── parse.c
   ├── preprocess.c
   ├── tokenize.c
   ├── type.c
   ├── unicode.c
   ├── hashmap.c
   ├── strings.c
   └── include/       (chibicc's bundled headers)
   ```

2. Create `tools/cc/Makefile`:
   - Uses native `cc` (host compiler)
   - Builds `chibicc` executable
   - Integrates with root Makefile

3. Verify chibicc compiles on the host

### Deliverables

- `tools/cc/` directory with all chibicc source files
- `tools/cc/Makefile` that builds chibicc for host
- Working chibicc binary (compiler itself, not yet producing correct output)

### Testing Strategy

```bash
cd tools/cc
make
./chibicc --help  # Verify binary runs
```

Note: On ARM hosts (M1/M2 Mac), we cannot run chibicc's original x86-64 test suite since it produces Linux ELF binaries. Correctness will be validated through AArch64 tests in Steps 4+.

### Exit Criteria

1. `make -C tools/cc` produces `tools/cc/chibicc` binary
2. Binary executes without crashing (e.g., `--help` or version output)
3. Source files are cleanly organized in `tools/cc/`

---

## Step 2: Stub AArch64 Codegen

**Goal**: Replace x86-64 codegen with AArch64 stub that emits minimal valid assembly.

### Work

1. Rewrite `codegen.c` with AArch64 structure:
   - Change register arrays from x86-64 to AArch64 names
   - Update `push()`/`pop()` to use AArch64 stack operations
   - Create stub `gen_expr()` that handles only `ND_NUM` (integer literals)
   - Create stub `gen_stmt()` that handles only `ND_RETURN`
   - Implement minimal function prologue/epilogue

2. Key register definitions:
   ```c
   #define GP_MAX 8  // x0-x7 for arguments
   #define FP_MAX 8  // d0-d7 for float arguments

   static char *argreg64[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
   static char *argreg32[] = {"w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7"};
   ```

3. Minimal function structure:
   ```asm
   func:
       stp x29, x30, [sp, #-16]!
       mov x29, sp
       // ... body ...
       ldp x29, x30, [sp], #16
       ret
   ```

### Deliverables

- `codegen.c` with AArch64 register definitions
- Working function prologue/epilogue
- Ability to compile `int main() { return 42; }`

### Testing Strategy

```bash
# Compile minimal program
echo 'int main() { return 42; }' > /tmp/test.c
./chibicc -S -o /tmp/test.s /tmp/test.c

# Verify assembly syntax
cat /tmp/test.s  # Should show valid AArch64 syntax

# Assemble and run via QEMU user-mode
aarch64-elf-as /tmp/test.s -o /tmp/test.o
aarch64-elf-ld -o /tmp/test /tmp/test.o -e main
qemu-aarch64 /tmp/test; echo $?  # Should print 42
```

### Exit Criteria

1. `int main() { return 42; }` compiles to valid AArch64 assembly
2. Assembly can be assembled with `aarch64-elf-as`
3. Resulting binary returns correct value when run under QEMU user-mode

---

## Step 3: Slopix Integration

**Goal**: Configure chibicc to use Slopix libc headers and aarch64-elf toolchain early, enabling test infrastructure.

### Work

1. Modify `add_default_include_paths()` in `main.c`:
   ```c
   static void add_default_include_paths(char *argv0) {
       // Slopix libc headers (tools/cc/../../libc/include)
       char *dir = dirname(strdup(argv0));
       strarray_push(&include_paths, format("%s/../../libc/include", dir));

       // chibicc's bundled headers
       strarray_push(&include_paths, format("%s/include", dir));
   }
   ```

2. Update `assemble()` function:
   ```c
   static void assemble(char *input, char *output) {
       char *cmd[] = {"aarch64-elf-as", input, "-o", output, NULL};
       run_subprocess(cmd);
   }
   ```

3. Remove or stub `run_linker()` - Slopix build system handles linking

4. Add AArch64/Slopix-specific predefined macros:
   ```c
   define_macro("__aarch64__", "1");
   define_macro("__LP64__", "1");
   define_macro("__SLOPIX__", "1");
   ```

5. Create `tools/cc/include/` with AArch64-specific headers:
   - `stdarg.h` - AArch64 va_list implementation
   - `stddef.h` - size_t, ptrdiff_t, NULL
   - `stdint.h` - fixed-width integer types

### Deliverables

- chibicc configured for Slopix/AArch64
- Include paths point to `libc/include/`
- Uses `aarch64-elf-as` for assembly

### Testing Strategy

```bash
# Verify chibicc finds headers and produces assembly
echo '#include <stdio.h>
int main() { return 0; }' > /tmp/test.c

tools/cc/chibicc -S /tmp/test.c -o /tmp/test.s
cat /tmp/test.s  # Should show AArch64 assembly
```

### Exit Criteria

1. chibicc finds Slopix libc headers without `-I` flag
2. chibicc produces AArch64 assembly (even if incomplete)
3. No errors from missing standard headers

---

## Step 4: Test Infrastructure

**Goal**: Set up chibicc test infrastructure using Slopix initramfs, same as `cmd/tests/`.

### Work

1. Create `tools/cc/test/` directory structure:
   ```
   tools/cc/test/
   ├── Makefile
   ├── test.h          # ASSERT macro (from chibicc)
   ├── common.c        # assert() using Slopix libc
   ├── tests.c         # Test runner (like cmd/tests/tests.c)
   └── *.c             # Individual test files from chibicc
   ```

2. Create `test.h` (same as chibicc's):
   ```c
   #define ASSERT(x, y) assert(x, y, #y)
   void assert(int expected, int actual, char *code);
   ```

3. Create `common.c` using Slopix libc:
   ```c
   #include <stdio.h>
   #include <unistd.h>

   void assert(int expected, int actual, char *code) {
       if (expected == actual) {
           printf("%s => %d\n", code, actual);
       } else {
           printf("FAIL: %s => expected %d but got %d\n", code, expected, actual);
           exit(1);
       }
   }

   // Add helper functions from chibicc's test/common as needed:
   // - add_all() for varargs tests
   // - add_float/add_double() for float tests
   // - struct_test*() for struct passing tests
   ```

   Note: Requires `memcmp()` in Slopix libc (add to `libc/string.c` before running tests).

4. Create `tests.c` runner (modeled on `cmd/tests/tests.c`):
   ```c
   #include <stdio.h>

   // Declare test functions (each test file's main renamed)
   int test_arith(void);
   int test_variable(void);
   int test_control(void);
   // ... etc

   // From libc - clean shutdown
   void poweroff(void);

   int main(void) {
       printf("=== chibicc test suite ===\n");

       printf("arith... ");
       test_arith();
       printf("OK\n");

       printf("variable... ");
       test_variable();
       printf("OK\n");

       // ... more tests added as features implemented

       printf("All tests passed!\n");
       poweroff();
       return 0;
   }
   ```

5. Create `Makefile`:
   ```makefile
   CC = ../chibicc
   AS = aarch64-elf-as
   LD = aarch64-elf-ld
   CFLAGS = -I. -I../../../libc/include
   LIBC = ../../../libc/libc.a
   LINK_SCRIPT = ../../../cmd/link.ld

   # Start with just minimal test, add more as features work
   TESTS =

   OBJS = tests.o common.o $(TESTS:=.o)

   tests.elf: $(OBJS)
   	$(LD) -T $(LINK_SCRIPT) -o $@ $(OBJS) $(LIBC)

   %.o: %.c
   	$(CC) $(CFLAGS) -c $< -o $*.s
   	$(AS) $*.s -o $@

   clean:
   	rm -f *.o *.s *.elf
   ```

6. Copy chibicc test files and adapt:
   - chibicc has 41 test files; start with core tests, add more incrementally
   - Core tests: `arith.c`, `variable.c`, `control.c`, `function.c`, `pointer.c`, `cast.c`, `float.c`, `struct.c`, `union.c`
   - Additional tests as needed: `sizeof.c`, `literal.c`, `enum.c`, `typedef.c`, `initializer.c`, `bitfield.c`, `varargs.c`, `vla.c`, etc.
   - Rename `main()` to `test_<name>()` in each
   - Skip unsupported tests: tls.c, atomic.c, asm.c, alloca.c
   - Note: chibicc's `test/common` file provides helper functions (assert, struct test helpers, varargs helpers) - adapt for Slopix libc

7. Add to root Makefile:
   ```makefile
   test-chibicc: tools/cc/test/tests.elf
   	# Add to initramfs and run like cmd/tests
   ```

### Deliverables

- Test infrastructure in `tools/cc/test/`
- Tests run on Slopix via initramfs (same as `cmd/tests/`)
- Makefile integrates with existing build system

### Testing Strategy

Start with minimal runner (no tests yet):
```bash
make -C tools/cc/test
# Add tests.elf to initramfs, boot Slopix
# Should print "=== chibicc test suite ===" then "All tests passed!"
```

### Exit Criteria

1. Empty test runner compiles with chibicc
2. Test runner boots on Slopix and exits cleanly via poweroff()
3. Build integrates with root Makefile

---

## Step 5: Integer Arithmetic

**Goal**: Implement code generation for integer operations.

### Work

1. Implement stack operations:
   ```c
   static void push(void) {
       println("  str x0, [sp, #-16]!");
       depth++;
   }

   static void pop(char *reg) {
       println("  ldr %s, [sp], #16", reg);
       depth--;
   }
   ```

2. Implement arithmetic in `gen_expr()`:
   | Node Kind | x86-64 | AArch64 |
   |-----------|--------|---------|
   | ND_ADD | `add %rdi, %rax` | `add x0, x0, x1` |
   | ND_SUB | `sub %rdi, %rax` | `sub x0, x0, x1` |
   | ND_MUL | `imul %rdi, %rax` | `mul x0, x0, x1` |
   | ND_DIV | `cqo; idiv %rdi` | `sdiv x0, x0, x1` |
   | ND_MOD | `cqo; idiv; mov rdx,rax` | `sdiv x2, x0, x1; msub x0, x2, x1, x0` |
   | ND_NEG | `neg %rax` | `neg x0, x0` |

3. Implement bitwise operations:
   | Node Kind | AArch64 |
   |-----------|---------|
   | ND_BITAND | `and x0, x0, x1` |
   | ND_BITOR | `orr x0, x0, x1` |
   | ND_BITXOR | `eor x0, x0, x1` |
   | ND_BITNOT | `mvn x0, x0` |
   | ND_SHL | `lsl x0, x0, x1` |
   | ND_SHR | `lsr x0, x0, x1` (unsigned), `asr x0, x0, x1` (signed) |

4. Implement comparisons:
   ```c
   // Compare and set
   println("  cmp x0, x1");
   println("  cset x0, eq");  // for ND_EQ
   println("  cset x0, ne");  // for ND_NE
   println("  cset x0, lt");  // for ND_LT (signed)
   println("  cset x0, le");  // for ND_LE (signed)
   println("  cset x0, lo");  // for ND_LT (unsigned)
   println("  cset x0, ls");  // for ND_LE (unsigned)
   ```

### Deliverables

- Complete integer arithmetic code generation
- Comparison operators
- Bitwise operations

### Testing Strategy

1. Add `arith.c` to test suite (rename `main` to `test_arith`)
2. Add to `tests.c` runner and `TESTS` in Makefile
3. Run `make test-chibicc`

### Exit Criteria

1. `arith.c` tests pass on Slopix
2. All arithmetic operators (+, -, *, /, %) work correctly
3. All bitwise operators (&, |, ^, ~, <<, >>) work correctly
4. All comparison operators (==, !=, <, <=, >, >=) work correctly

---

## Step 6: Local Variables and Control Flow

**Goal**: Implement variable access and control flow statements.

### Work

1. Implement local variable access in `gen_addr()`:
   ```c
   // Local variable at offset from frame pointer
   println("  add x0, x29, #%d", var->offset);
   ```

2. Implement `load()` from address in x0:
   ```c
   if (ty->size == 1)
       println("  ldrsb x0, [x0]");  // or ldrb for unsigned
   else if (ty->size == 2)
       println("  ldrsh x0, [x0]");  // or ldrh for unsigned
   else if (ty->size == 4)
       println("  ldrsw x0, [x0]");  // or ldr w0 for unsigned
   else
       println("  ldr x0, [x0]");
   ```

3. Implement `store()` to address on stack:
   ```c
   pop("x1");  // address
   if (ty->size == 1)
       println("  strb w0, [x1]");
   else if (ty->size == 2)
       println("  strh w0, [x1]");
   else if (ty->size == 4)
       println("  str w0, [x1]");
   else
       println("  str x0, [x1]");
   ```

4. Implement control flow in `gen_stmt()`:
   ```c
   // IF statement
   case ND_IF:
       gen_expr(node->cond);
       println("  cbz x0, .L.else.%d", c);
       gen_stmt(node->then);
       println("  b .L.end.%d", c);
       println(".L.else.%d:", c);
       if (node->els) gen_stmt(node->els);
       println(".L.end.%d:", c);

   // FOR/WHILE loops
   case ND_FOR:
       println(".L.begin.%d:", c);
       if (node->cond) {
           gen_expr(node->cond);
           println("  cbz x0, %s", node->brk_label);
       }
       gen_stmt(node->then);
       println("%s:", node->cont_label);
       if (node->inc) gen_expr(node->inc);
       println("  b .L.begin.%d", c);
       println("%s:", node->brk_label);
   ```

5. Implement switch/case:
   ```c
   case ND_SWITCH:
       gen_expr(node->cond);
       for (Node *n = node->case_next; n; n = n->case_next) {
           println("  cmp x0, #%ld", n->begin);
           println("  b.eq %s", n->label);
       }
       if (node->default_case)
           println("  b %s", node->default_case->label);
       println("  b %s", node->brk_label);
   ```

### Deliverables

- Local variable load/store
- If/else statements
- For/while/do-while loops
- Switch/case statements
- Goto and labels

### Testing Strategy

1. Add `variable.c` and `control.c` to test suite
2. Add to `tests.c` runner and `TESTS` in Makefile
3. Run `make test-chibicc`

### Exit Criteria

1. `variable.c` tests pass on Slopix
2. `control.c` tests pass on Slopix
3. Local variables work correctly
4. All control flow constructs work
5. Break/continue in loops work

---

## Step 7: Function Calls (AAPCS64)

**Goal**: Implement the AArch64 Procedure Call Standard for function calls.

### Work

1. AAPCS64 register usage:
   | Role | Registers |
   |------|-----------|
   | Arguments (integer) | x0-x7 |
   | Arguments (float) | d0-d7 |
   | Return value | x0 (or x0+x1 for 128-bit) |
   | Caller-saved | x9-x15 |
   | Callee-saved | x19-x28 |
   | Frame pointer | x29 |
   | Link register | x30 |
   | Stack pointer | sp (16-byte aligned) |

2. Implement `push_args()` for parameter passing:
   - First 8 integer args in x0-x7
   - First 8 float args in d0-d7
   - Remaining args pushed to stack (right-to-left)
   - Stack must be 16-byte aligned before `bl`

3. Implement function call:
   ```c
   case ND_FUNCALL:
       int stack_args = push_args(node);
       gen_expr(node->lhs);  // Function address

       // Pop arguments into registers
       int gp = 0;
       for (Node *arg = node->args; arg; arg = arg->next) {
           if (!arg->pass_by_stack && gp < GP_MAX)
               pop(argreg64[gp++]);
       }

       // Call function
       println("  blr x0");

       // Clean up stack args
       if (stack_args > 0)
           println("  add sp, sp, #%d", align_to(stack_args * 8, 16));
   ```

4. Implement function prologue/epilogue:
   ```asm
   // Prologue
   stp x29, x30, [sp, #-FRAME_SIZE]!
   mov x29, sp
   // Store arguments to stack slots
   str x0, [x29, #-8]   // first arg
   str x1, [x29, #-16]  // second arg
   ...

   // Epilogue
   ldp x29, x30, [sp], #FRAME_SIZE
   ret
   ```

5. Save/restore callee-saved registers if used:
   ```asm
   stp x19, x20, [sp, #16]
   ...
   ldp x19, x20, [sp, #16]
   ```

### Deliverables

- Complete function call implementation
- Correct argument passing (up to 8 in registers, rest on stack)
- Proper stack alignment (16-byte)
- Callee-saved register preservation

### Testing Strategy

1. Add `function.c` to test suite
2. Run `make test-chibicc`

### Exit Criteria

1. `function.c` tests pass on Slopix
2. Functions with up to 8 integer args work
3. Functions with >8 args (stack passing) work
4. Recursive functions work
5. Nested function calls work

---

## Step 8: Global Variables and Pointers

**Goal**: Implement global variable access and pointer operations.

### Work

1. Implement global data emission in `emit_data()`:
   ```asm
   .data
   .globl var_name
   .align 3
   var_name:
       .xword 0    // 8-byte
       .word 0     // 4-byte
       .byte 0     // 1-byte
   ```

2. Implement PC-relative addressing for globals:
   ```c
   // Load address of global variable
   println("  adrp x0, %s", var->name);
   println("  add x0, x0, :lo12:%s", var->name);
   ```

3. Handle string literals:
   ```asm
   .section .rodata
   .L.str.0:
       .string "hello"
   ```

4. Implement pointer arithmetic:
   ```c
   // ptr + n  (where ptr points to type of size S)
   gen_expr(node->lhs);  // pointer
   push();
   gen_expr(node->rhs);  // integer
   println("  mov x1, #%d", node->lhs->ty->base->size);
   println("  mul x0, x0, x1");
   pop("x1");
   println("  add x0, x1, x0");
   ```

5. Implement address-of and dereference:
   ```c
   case ND_ADDR:
       gen_addr(node->lhs);  // Address already in x0
       return;

   case ND_DEREF:
       gen_expr(node->lhs);  // Pointer value in x0
       load(node->ty);       // Load from address
       return;
   ```

### Deliverables

- Global variable access
- String literal handling
- Pointer arithmetic
- Address-of operator
- Dereference operator

### Testing Strategy

1. Add `pointer.c` to test suite
2. Run `make test-chibicc`

### Exit Criteria

1. `pointer.c` tests pass on Slopix
2. Global variables accessible
3. String literals work
4. Pointer arithmetic correct
5. Arrays via pointer indexing work

---

## Step 9: Type Conversions

**Goal**: Implement the type cast table for all conversions.

### Work

1. Create AArch64 cast table:
   ```c
   enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64 };

   // Sign/zero extension
   static char i32i8[] = "sxtb w0, w0";
   static char i32u8[] = "uxtb w0, w0";
   static char i32i16[] = "sxth w0, w0";
   static char i32u16[] = "uxth w0, w0";
   static char i32i64[] = "sxtw x0, w0";
   static char u32i64[] = "mov w0, w0";  // Zero-extends automatically

   // Integer to float
   static char i32f32[] = "scvtf s0, w0";
   static char i32f64[] = "scvtf d0, w0";
   static char i64f32[] = "scvtf s0, x0";
   static char i64f64[] = "scvtf d0, x0";
   static char u32f32[] = "ucvtf s0, w0";
   static char u64f64[] = "ucvtf d0, x0";

   // Float to integer
   static char f32i32[] = "fcvtzs w0, s0";
   static char f64i32[] = "fcvtzs w0, d0";
   static char f32i64[] = "fcvtzs x0, s0";
   static char f64i64[] = "fcvtzs x0, d0";

   // Float conversions
   static char f32f64[] = "fcvt d0, s0";
   static char f64f32[] = "fcvt s0, d0";
   ```

2. Implement `cast()` function:
   ```c
   static void cast(Type *from, Type *to) {
       if (to->kind == TY_VOID) return;
       if (to->kind == TY_BOOL) {
           cmp_zero(from);
           println("  cset x0, ne");
           return;
       }
       int t1 = getTypeId(from);
       int t2 = getTypeId(to);
       if (cast_table[t1][t2])
           println("  %s", cast_table[t1][t2]);
   }
   ```

3. Implement `cmp_zero()` for different types:
   ```c
   static void cmp_zero(Type *ty) {
       if (ty->kind == TY_FLOAT) {
           println("  fcmp s0, #0.0");
       } else if (ty->kind == TY_DOUBLE) {
           println("  fcmp d0, #0.0");
       } else {
           println("  cmp x0, #0");
       }
   }
   ```

### Deliverables

- Complete type conversion table
- Integer sign/zero extension
- Float/integer conversions
- Float precision conversions

### Testing Strategy

1. Add `cast.c` and `usualconv.c` to test suite
2. Run `make test-chibicc`

### Exit Criteria

1. `cast.c` tests pass on Slopix
2. `usualconv.c` tests pass on Slopix
3. All integer conversions work (signed/unsigned, widening/narrowing)
4. Float/integer conversions work
5. Implicit type promotions correct

---

## Step 10: Floating-Point Operations

**Goal**: Implement floating-point arithmetic using SIMD/FP registers.

### Work

1. Implement float load/store:
   ```c
   // In load()
   case TY_FLOAT:
       println("  ldr s0, [x0]");
       return;
   case TY_DOUBLE:
       println("  ldr d0, [x0]");
       return;

   // In store()
   case TY_FLOAT:
       println("  str s0, [x1]");
       return;
   case TY_DOUBLE:
       println("  str d0, [x1]");
       return;
   ```

2. Implement float push/pop:
   ```c
   static void pushf(void) {
       println("  str d0, [sp, #-16]!");
       depth++;
   }
   static void popf(int reg) {
       println("  ldr d%d, [sp], #16", reg);
       depth--;
   }
   ```

3. Implement float arithmetic:
   | Operation | Float (s0,s1) | Double (d0,d1) |
   |-----------|---------------|----------------|
   | Add | `fadd s0, s0, s1` | `fadd d0, d0, d1` |
   | Sub | `fsub s0, s0, s1` | `fsub d0, d0, d1` |
   | Mul | `fmul s0, s0, s1` | `fmul d0, d0, d1` |
   | Div | `fdiv s0, s0, s1` | `fdiv d0, d0, d1` |
   | Neg | `fneg s0, s0` | `fneg d0, d0` |

4. Implement float comparisons:
   ```c
   println("  fcmp d0, d1");
   println("  cset x0, eq");  // ==
   println("  cset x0, ne");  // !=
   println("  cset x0, mi");  // < (less than)
   println("  cset x0, ls");  // <= (less or same)
   ```

5. Implement float argument passing (d0-d7):
   ```c
   // In push_args and function prologue
   if (is_flonum(ty)) {
       if (fp < FP_MAX)
           popf(fp++);
   }
   ```

6. Handle float literals:
   ```c
   case ND_NUM:
       if (node->ty->kind == TY_FLOAT) {
           union { float f; uint32_t u; } u = { node->fval };
           println("  mov w0, #%u", u.u);
           println("  fmov s0, w0");
       } else if (node->ty->kind == TY_DOUBLE) {
           union { double d; uint64_t u; } u = { node->fval };
           println("  mov x0, #%lu", u.u);
           println("  fmov d0, x0");
       }
   ```

### Deliverables

- Float/double arithmetic
- Float comparisons
- Float function arguments
- Float return values
- Float literals

### Testing Strategy

1. Add `float.c` to test suite
2. Run `make test-chibicc`

### Exit Criteria

1. `float.c` tests pass on Slopix
2. Float arithmetic works
3. Double arithmetic works
4. Float function calls work
5. Float/double comparisons work

---

## Step 11: Structs and Arrays

**Goal**: Implement struct/array handling including member access and passing.

### Work

1. Implement struct member access (ND_MEMBER):
   ```c
   case ND_MEMBER:
       gen_addr(node->lhs);  // Base address in x0
       println("  add x0, x0, #%d", node->member->offset);
   ```

2. Implement array subscript (handled as pointer arithmetic + deref)

3. Implement struct copy for assignment:
   ```c
   // Byte-by-byte copy for struct assignment
   case TY_STRUCT:
   case TY_UNION:
       for (int i = 0; i < ty->size; i++) {
           println("  ldrb w2, [x0, #%d]", i);
           println("  strb w2, [x1, #%d]", i);
       }
       return;
   ```

4. Implement struct passing in registers (AAPCS64):
   - Structs <= 16 bytes: passed in up to 2 registers
   - Larger structs: passed by pointer (x8 = indirect result location)

5. Implement struct return:
   ```c
   // Small structs returned in x0 (and x1 if needed)
   // Large structs: caller passes buffer address in x8
   if (ty->size <= 8) {
       println("  ldr x0, [x0]");
   } else if (ty->size <= 16) {
       println("  ldp x0, x1, [x0]");
   } else {
       // Copy to address in x8
       copy_struct_mem();
   }
   ```

6. Implement bitfield access:
   ```c
   // Load containing word, extract bits
   println("  lsl x0, x0, #%d", 64 - bit_width - bit_offset);
   if (is_unsigned)
       println("  lsr x0, x0, #%d", 64 - bit_width);
   else
       println("  asr x0, x0, #%d", 64 - bit_width);
   ```

### Deliverables

- Struct member access
- Array indexing
- Struct assignment (copy)
- Struct passing/returning
- Bitfield access

### Testing Strategy

1. Add `struct.c` and `union.c` to test suite
2. Run `make test-chibicc`

### Exit Criteria

1. `struct.c` tests pass on Slopix
2. `union.c` tests pass on Slopix
3. Struct member access works
4. Array indexing works
5. Struct function parameters work
6. Bitfields work

---

## Step 12: cmd/ Program Compilation

**Goal**: Successfully compile all cmd/ programs with chibicc.

### Work

1. Compile each cmd/ program:
   - Core: init, shell, tests
   - File utilities: cat, cp, mv, rm, mkdir, touch, ls
   - Text utilities: echo, head, wc, grep
   - Process utilities: ps, kill, sleep, shutdown
   - Misc: true, false, cursor_blink, ticker

2. Fix any code generation bugs discovered

3. Update Makefile:
   ```makefile
   cmd-chibicc:
   	$(MAKE) -C cmd CC=../tools/cc/chibicc
   ```

### Deliverables

- All cmd/ programs compile with chibicc
- No regressions in functionality

### Testing Strategy

```bash
make cmd-chibicc
make run  # Test interactively
```

### Exit Criteria

1. All cmd/ programs compile with chibicc
2. Programs run correctly on Slopix
3. `make run` with chibicc-compiled programs works

---

## Phase 1 Summary

### Test-Feature Mapping

| Step | Feature | Test File | Exit Criteria |
|------|---------|-----------|---------------|
| 1 | Project Setup | - | chibicc binary builds |
| 2 | Stub Codegen | manual | `return 42` works |
| 3 | Slopix Integration | - | Headers found, asm produced |
| 4 | Test Infrastructure | - | Empty runner boots on Slopix |
| 5 | Arithmetic | arith.c | All tests pass |
| 6 | Variables/Control | variable.c, control.c | All tests pass |
| 7 | Functions | function.c | All tests pass |
| 8 | Globals/Pointers | pointer.c | All tests pass |
| 9 | Type Casts | cast.c, usualconv.c | All tests pass |
| 10 | Floats | float.c | All tests pass |
| 11 | Structs | struct.c, union.c | All tests pass |
| 12 | cmd/ Programs | cmd/* | All compile and run |

### Final Directory Structure

```
tools/cc/
├── Makefile
├── chibicc.h
├── main.c          (modified: include paths, assemble())
├── codegen.c       (rewritten: AArch64)
├── parse.c         (unchanged)
├── preprocess.c    (unchanged)
├── tokenize.c      (unchanged)
├── type.c          (unchanged)
├── unicode.c       (unchanged)
├── hashmap.c       (unchanged)
├── strings.c       (unchanged)
├── include/        (AArch64-specific headers)
│   ├── stdarg.h
│   ├── stddef.h
│   └── stdint.h
└── test/
    ├── Makefile
    ├── test.h
    ├── common.c
    ├── tests.c      (test runner)
    └── *.c          (chibicc tests)
```

### Exit Criteria (Phase 1 Complete)

1. **Test Suite**: All supported chibicc tests pass on Slopix
2. **cmd/ Compilation**: All programs compile with chibicc
3. **Functionality**: All programs run correctly on Slopix
4. **Integration**: `make cmd CC=tools/cc/chibicc` works

---

## References

- [chibicc source](https://github.com/rui314/chibicc)
- [AAPCS64](docs/aapcs64/aapcs64.md) - AArch64 calling convention
- [ELF for AArch64](docs/aaelf64/aaelf64.md) - Executable format
- [ARM A64 Instruction Set](https://developer.arm.com/documentation/ddi0602/latest)
- [xhackerustc/chibicc-aarch64](https://github.com/xhackerustc/chibicc-aarch64) - Reference port
- [derbuihan/chibicc_arm64](https://github.com/derbuihan/chibicc_arm64) - M1 Mac port
