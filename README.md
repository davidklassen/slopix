# Slopix

Bare-metal AArch64 kernel for QEMU virt — 100% written by AI.

## What is this?

An experiment in vibe-coding an operating system from scratch. Every line of code was written by [Claude Code](https://claude.ai/claude-code) — no human-written code, just prompts and direction.

## Development Environment Setup

The build system bootstraps its own cross-toolchain from source — no cross compiler needed.

### macOS

```bash
xcode-select --install
brew install qemu
```

### Ubuntu

```bash
sudo apt update
sudo apt install build-essential qemu-system-arm
```

## Run

```
make run
```

## Test

```
make test
```
