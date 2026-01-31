# Slopix

Bare-metal AArch64 kernel for QEMU virt — 100% written by AI.

## What is this?

An experiment in vibe-coding an operating system from scratch. Every line of code was written by [Claude Code](https://claude.ai/claude-code) — no human-written code, just prompts and direction.

## Development Environment Setup

### macOS

```bash
brew install aarch64-elf-gcc qemu
```

### Ubuntu

```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm
```

## Run

```
make run
```

## Test

```
make test
```
