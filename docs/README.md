# Slopix Documentation Library

Curated links to tutorials, projects, and communities.

## Official Documentation

### ARM
- [ARM Developer](https://developer.arm.com/) - Official specs and guides
- [ARM Architecture Documentation](https://developer.arm.com/architectures)
- [Learn the Architecture](https://developer.arm.com/architectures/learn-the-architecture) - Free guides

### QEMU
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [QEMU ARM Target](https://www.qemu.org/docs/master/system/target-arm.html)
- [QEMU Virt Board](https://qemu-project.gitlab.io/qemu/system/arm/virt.html)

## Tutorials

### AArch64 OS Development
- [OSDev Wiki - QEMU AArch64 Virt Bare Bones](https://wiki.osdev.org/QEMU_AArch64_Virt_Bare_Bones)
  - Step-by-step "Hello World" for AArch64 QEMU
- [raspberry-pi-os](https://github.com/s-matyukevich/raspberry-pi-os)
  - ARM64 OS tutorial (similar concepts apply)
- [Bare Metal ARM E-Book](https://umanovskis.se/files/arm-baremetal-ebook.pdf)
  - Comprehensive bare-metal guide

### General OS Development
- [OSDev Wiki](https://wiki.osdev.org/) - The OS development wiki
- [Writing an OS in Rust](https://os.phil-opp.com/) - Concepts transfer to C
- [OSTEP - Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)
  - Free textbook on OS concepts

### ARM Architecture
- [ARM Cortex-A Programmer's Guide](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf)
  - Excellent introduction to ARMv8-A
- [ARM Generic Timer](https://developer.arm.com/documentation/102379/latest/)
  - Timer programming guide

## Reference Implementations

### Educational Operating Systems
- [xv6-riscv](https://github.com/mit-pdos/xv6-riscv)
  - MIT's teaching OS (RISC-V, concepts transfer)
  - [xv6 Book](https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf)
- [xv6-public](https://github.com/mit-pdos/xv6-public)
  - Original x86 version

### ARM-Specific Projects
- [circle](https://github.com/rsta2/circle)
  - Bare metal environment for Raspberry Pi
- [Raspberry Pi OS](https://github.com/s-matyukevich/raspberry-pi-os)
  - Tutorial project with working code

### Production Kernels (for reference)
- [Linux kernel ARM64](https://github.com/torvalds/linux/tree/master/arch/arm64)
  - Real-world implementation
- [seL4](https://github.com/seL4/seL4)
  - Verified microkernel
- [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr)
  - Real-time OS with ARM support

## Communities

### Forums
- [OSDev Forums](https://forum.osdev.org/)
  - Active community for OS developers
- [ARM Community](https://community.arm.com/)
  - Official ARM developer forums

### Reddit
- [/r/osdev](https://reddit.com/r/osdev)
  - OS development subreddit
- [/r/lowlevel](https://reddit.com/r/lowlevel)
  - Low-level programming

### Discord
- OSDev Discord - See osdev.org for invite

## Tools

### Cross-Compilers
- [ARM GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/downloads)
  - Official ARM cross-compiler
- [Homebrew aarch64-elf-gcc](https://formulae.brew.sh/formula/aarch64-elf-gcc)
  - `brew install aarch64-elf-gcc`

### Debugging
- [GDB Multiarch](https://www.gnu.org/software/gdb/)
- [LLDB](https://lldb.llvm.org/) - LLVM debugger

### Build Systems
- [GNU Make](https://www.gnu.org/software/make/)
- [CMake](https://cmake.org/)
- [Meson](https://mesonbuild.com/)

## Books

### Operating Systems
- "Operating Systems: Three Easy Pieces" - Remzi Arpaci-Dusseau
- "Modern Operating Systems" - Andrew Tanenbaum
- "Operating System Concepts" - Silberschatz

### Computer Architecture
- "Computer Organization and Design ARM Edition" - Patterson & Hennessy
- "ARM System Developer's Guide" - Sloss, Symes, Wright

## Quick Reference

Local documentation converted to markdown for LLM-friendly access.

| Document | Description |
|----------|-------------|
| [ARMv8-A Programmer's Guide](ARMv8-A-Programmer-Guide/ARMv8-A-Programmer-Guide.md) | Architecture fundamentals |
| [ARM Exception Model](ARM-Exception-Model/ARM-Exception-Model.md) | EL0-EL3, vector layout |
| [GIC Architecture (IHI 0069)](IHI0069_gic_architecture/IHI0069_gic_architecture.md) | Interrupt controller |
| [PL011 UART (DDI 0183)](DDI0183_pl011_uart/DDI0183_pl011_uart.md) | Serial port |
| [Cortex-A57 TRM (DDI 0488)](DDI0488_cortex_a57/DDI0488_cortex_a57.md) | CPU implementation details |
| [PL031 RTC (DDI 0224)](DDI0224_pl031_rtc/DDI0224_pl031_rtc.md) | Real-time clock |
| [AAPCS64](aapcs64/aapcs64.md) | ARM calling convention |
| [ELF for AArch64](aaelf64/aaelf64.md) | Executable format |
| [System V ABI AArch64](sysvabi64/sysvabi64.md) | Syscall conventions |
| [PSCI (DEN 0022)](DEN0022_psci/DEN0022_psci.md) | Power State Coordination Interface |
| [Devicetree Spec](devicetree-spec/devicetree-spec.md) | Hardware description format |
| [Bare Metal Boot Code (DAI 0527)](DAI0527A_baremetal_boot_code/DAI0527A_baremetal_boot_code.md) | Boot sequence guide |
| [xv6 Book](xv6-book-riscv/xv6-book-riscv.md) | Teaching OS reference |

**Note:** ARM Architecture Reference Manual (DDI 0487) not included due to size.
