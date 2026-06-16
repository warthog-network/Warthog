# AGENTS.md - Warthog Reference Implementation (defi branch)

## Overview

This is the C++ reference implementation of the Warthog cryptocurrency, **defi branch**. The defi branch adds native DeFi features (tokens, DEX, liquidity pools, Fair Batch Matching) on top of the mainnet `master` branch. The project is built with Meson/Ninja, requires C++23, and targets Linux/Windows/macOS.

For cross-repo context (other sub-repos, sub-repo → git remote mapping), see `/AGENTS.md` and `/HUB.md` at the project hub root.

## Important Constants

- **Block time**: 20 seconds
- **Initial block reward**: 3 WART
- **Halving**: every 3,153,600 blocks (~2 years)
- **Supply hard cap**: ~18,921,599.68464 WART (~19 million)
- **Coin unit**: 1 WART = 100,000,000 E8 (8 decimal places)
- **WART decimals**: 8
- **Token decimals (assets)**: 0-18 (per-asset)
- **Block version**: 4

## Architecture

The codebase is organized as:

- `src/shared/src/communication/` — Transaction creation and message types
- `src/shared/src/defi/` — DeFi logic (CPMM pools, Fair Batch Matching, matcher)
- `src/shared/src/block/` — Block, header, and chain types
- `src/node/` — Node implementation (network, API server, P2P)
- `src/tui_wallet/` — TUI wallet (TUI on defi branch; CLI was on master)
- `src/test/` — Meson tests (currently: `custom_float`)
- `thirdparty/` — Vendored dependencies (json, spdlog, libuv, libwebsockets, etc.)

The defi branch is C++23. The codebase uses templated CRTP for transactions, signed/unsigned structure, and signed/unsized templates.

## Build Commands

### `just` recipes (recommended)

The project includes a `justfile` for development tasks. If you have `just` installed:

```bash
just build-linux      # Build Linux executables via Docker, outputs to build/
just build-windows    # Cross-compile for Windows via Docker, outputs to build/windows/
just run_debug        # Run debug Docker image (for valgrind)
just valgrind -- [args]   # Run valgrind; args after -- go to warthog
just bump             # Bump patch version in meson.build
```

### Direct docker builds

```bash
DOCKER_BUILDKIT=1 docker build . -f dockerfiles/build_linux --output build
DOCKER_BUILDKIT=1 docker build . -f dockerfiles/build_windows --output ./build/windows
```

Available dockerfiles in `dockerfiles/`:
- `build_linux` — ✅ Works
- `build_windows` — ✅ Works
- `run_debug` — ✅ Works
- `run_tests` — ✅ Works
- `build_macos` — ❌ Broken (fmt dependency issue during cross-compilation)
- `build_emscripten` — ❌ Broken (Emscripten libc++ missing C++23 std::move_only_function)
- `build_linux_arm64` — 🔒 Disabled (all steps commented out)

### Native build (Linux)

```bash
sudo apt install meson ninja-build build-essential
meson setup build --buildtype=release
cd build && ninja
```

## CI/CD

The defi branch has the following GitHub Actions workflows (in `.github/workflows/`):

- **`release-defi.yml`** — Runs on push to `defi` branch and `workflow_dispatch`. Builds Docker (Linux + Windows), runs `run_tests`, and creates a GitHub release with `wart-node-defi-beta-linux` and `wart-node-defi-beta-windows.exe` artifacts. Also triggers `dockerhub_push.yml` via `repository_dispatch`.
- **`pre_release.yml`** — Runs on push to `dev` branch. Builds pre-release artifacts (Linux, Windows, macOS aarch64). macOS build is currently broken (fmt dep).
- **`cloudflare.yml`** — Runs on push to `network_refactor` branch. Builds browser node (emscripten) and deploys to Cloudflare Pages. **Currently broken** because the emscripten build is broken (see dockerfiles/README.md).
- **`dockerhub_push.yml`** — Triggered by `repository_dispatch` (event type: `docker`) from `release-defi.yml`. Builds and pushes Docker image to `zzjulien/warthog_node` with tags `latest` and `$VERSION` (from `meson.build`).

## Versioning

- Current version: **v0.10.16** (read from `meson.build` line `version : '...'`)
- Versions follow semantic versioning: `MAJOR.MINOR.PATCH`
- `just bump` automatically increments the patch version in `meson.build`
- Docker images are tagged with the version from `meson.build`
- GitHub releases use the version as the tag name

## Important Files

- `meson.build` — Project configuration, version declaration
- `justfile` — Development task recipes
- `core/defi/dockerfiles/` — Docker build targets (Linux, Windows, emscripten, etc.)
- `src/shared/src/communication/create_transaction.hpp` — Transaction type definitions (7 signed + 2 implicit)
- `src/shared/src/communication/create_transaction.cpp` — Transaction creation logic
- `src/shared/src/defi/uint64/matcher.hpp` — Fair Batch Matching engine
- `src/shared/src/block/body/elements.hpp` — Block body transaction types
- `src/node/api/server.hpp` — Main API server
- `src/node/api/hook_endpoints.hxx` — All REST/WebSocket endpoint definitions (look here for `GET_PRIV`/`GET_PUB`)
- `src/node/api/http/endpoint.cpp` — HTTP endpoint including public/private logic
- `src/tui_wallet/` — TUI wallet source

## Constraints

- **C++23 required** — `meson.build` declares `cpp_std=c++23`
- **Sub-repo rules** — this is part of the Warthog project hub; see `/AGENTS.md` for hub-level rules
- **Git operations** — the project hub root is NOT a git repository; this subdirectory IS a git repository. Use `git -C core/defi <command>` or `cd core/defi` for git operations.
- **Public RPC mode** — for internet exposure, use `--enable-public` (shorthand for `--publicrpc=0.0.0.0:3001`). This exposes a filtered subset of the API on port 3001; critical admin endpoints are hidden. Default port 3000 is full-access and should never be exposed to the internet.
- **End-to-end FBM** — all DeFi matching uses Fair Batch Matching. There is no traditional sequential matching.

## Transaction Types

7 signed (user-created) transaction types:
- `wartTransfer`, `tokenTransfer`, `assetCreation`, `cancelation`, `liquidityDeposit`, `liquidityWithdrawal`, `limitSwap`

2 implicit (node-generated) transaction types:
- `reward` (block reward paid to miner)
- `match` (Fair Batch Matching result; computed at block processing time, surfaced via API, not stored in block body)

## Testing

```bash
# Via docker (in release-defi workflow)
DOCKER_BUILDKIT=1 docker build . --file dockerfiles/run_tests --progress plain

# Native (after meson setup)
cd build && meson test -v
```

Currently only one test: `custom_float` (C++ floating-point conversion test).

## Cross-references

- **HUB.md** (hub root) — Cross-repo reference, public RPC info, research papers, depository structure
- **AGENTS.md** (hub root) — Hub-level overview, sub-repos table
- **`whitepaper/`** sub-repo — Project whitepaper (typst → PDF)
- **Research papers**:
  - [PoBW](https://warthog.network/PoBW.pdf) — Proof of Balanced Work paper
  - [Fair Batch Matching](https://warthog.network/FairBatchMatching.pdf) — FBM paper
- **Docs**: https://docs.warthog.network/
- **Website**: https://warthog.network/
- **DeFi Demo**: https://warthog.network/defi-demo
