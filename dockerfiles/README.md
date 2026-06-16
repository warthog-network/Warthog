# Dockerfiles

This directory contains Dockerfiles for building warthog for different platforms.

## Status Overview

| Dockerfile | Status | Description |
|------------|--------|-------------|
| `build_linux` | ✅ Works | Production Linux build |
| `build_windows` | ✅ Works | Windows cross-compilation |
| `run_debug` | ✅ Works | Debug build with valgrind |
| `run_tests` | ✅ Works | Test runner |
| `build_macos` | ❌ Broken | macOS cross-compilation (fmt dependency issue) |
| `build_emscripten` | ❌ Broken | WebAssembly build (C++23 std::move_only_function not implemented) |
| `build_linux_arm64` | 🔒 Disabled | ARM64 build (all steps commented out) |

## Individual Dockerfiles

### build_linux
Production Linux build using Alpine Linux.

**Usage:**
```bash
docker build . -f dockerfiles/build_linux --output build
# or
just build linux
```

**Output:** Linux executables in `build/` directory

---

### build_windows
Cross-compilation for Windows using MinGW on Alpine.

**Usage:**
```bash
docker build . -f dockerfiles/build_windows --output build
# or
just build windows
```

**Output:** Windows `.exe` files in `build/` directory

**Note:** Requires `-lstdc++exp` linker flag for C++23 std::print support. This flag can be removed when MinGW GCC version includes std::print terminal support by default.

---

### run_debug
Debug Docker image with valgrind for memory analysis.

**Usage:**
```bash
just run_debug
just valgrind -- [args]  # Run with valgrind, args after -- go to warthog
```

---

### build_macos
Cross-compilation for macOS (aarch64) using osxcross.

**Status:** ❌ Broken

**Issue:** spdlog requires external `fmt >= 11.0.0` for the macOS target, but Alpine's pkg-config cannot provide macOS-target libraries during cross-compilation.

**To fix:** Would need to build fmt for the macOS target within the osxcross environment.

---

### build_emscripten
WebAssembly build using Emscripten for browser-based nodes.

**Status:** ❌ Broken

**Issue:** Emscripten's libc++ does not implement C++23 `std::move_only_function`. The `move_only_function.hpp` header checks `__cplusplus >= 202302L` but the fallback doesn't trigger because the implementation is missing.

**Blocked by:** Upstream Emscripten libc++ support for C++23 features.

---

### build_linux_arm64
ARM64 (aarch64) Linux build.

**Status:** 🔒 Disabled

All build steps are commented out intentionally. This build is not yet implemented.

---

### run_tests
Test runner Docker container.

**Status:** ⚠️ Untested

Has not been run or verified to work.
