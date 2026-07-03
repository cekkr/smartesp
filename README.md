# smartesp

`smartesp` is an ESP-IDF smartwatch OS scaffold for the Waveshare ESP32-S3 Touch AMOLED 2.06 board. It keeps the watch shell, boot flows, and sensitive shared UI native in C, while executing selected applications through [`fayasm`](https://github.com/Geckos-Ink/fayasm) as WebAssembly workloads.

The target shape is a watch-class system closer to an Apple Watch than a generic LVGL demo:

- native C owns boot, loading, shared surfaces, and latency-sensitive operations
- WASM apps run through `fayasm` when they benefit from portability or sandboxing
- process policy distinguishes what must stay resident, what may hibernate, and what can be discarded under memory pressure
- SD-backed spill hooks are the intended path for hibernating WASM memory/JIT state when storage is available

## Status

This repository now contains a buildable ESP-IDF project skeleton with:

- Waveshare BSP integration taken from the local `ESP32-S3-Touch-AMOLED-2.06` examples
- `fayasm` added as a Git submodule under `external/fayasm`
- an ESP-IDF component wrapper for `fayasm`
- a native watch shell in C for boot and launcher/status surfaces
- a WASM runtime layer with host bindings and SD-backed spill/load hooks
- a process manager that marks apps as `always resident`, `resident while active`, `hibernable`, or `disposable`
- an embedded demo WASM app proving end-to-end execution through `fayasm`

## Repository Layout

Use this map first when navigating the code:

- [`main/main.c`](/Users/riccardo/Sources/GitHub/smartesp/main/main.c) starts NVS and launches the OS.
- [`components/watch_os/src/watch_os.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/watch_os.c) is the top-level orchestrator.
- [`components/watch_os/src/board/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/board) contains board/display bootstrap helpers.
- [`components/watch_os/src/ui/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/ui) contains the native C watch shell and boot/loading UI.
- [`components/watch_os/src/kernel/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/kernel) contains the process catalog and residency policy logic.
- [`components/watch_os/src/runtime/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/runtime) contains the `fayasm` integration and host bindings.
- [`components/watch_os/src/storage/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/storage) contains SD/runtime blob helpers used for hibernation.
- [`components/watch_os/src/apps/`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/apps) defines built-in app descriptors and the embedded demo WASM.
- [`components/fayasm_port/`](/Users/riccardo/Sources/GitHub/smartesp/components/fayasm_port) wraps upstream `fayasm` sources as an ESP-IDF component.
- [`external/fayasm`](/Users/riccardo/Sources/GitHub/smartesp/external/fayasm) is the upstream runtime submodule. If you modify it, commit inside that repository.

## Runtime Model

The watch is intentionally split into two execution classes:

### Native C

Use native code for:

- boot and crash-safe loading sequences
- shared shell widgets and status chrome
- touch/display critical paths
- power, input, and notification daemons
- any operation where WASM overhead would harm latency or reliability

### WASM via `fayasm`

Use WASM for:

- app logic that benefits from isolation or portability
- disposable or restartable user apps
- workloads that can be paused, hibernated, or cold-loaded from storage

Current runtime policy categories:

- `WATCH_APP_RESIDENCY_ALWAYS_RESIDENT`: never swap out
- `WATCH_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE`: keep warm while in active use
- `WATCH_APP_RESIDENCY_HIBERNABLE`: spill JIT/memory state to SD when needed
- `WATCH_APP_RESIDENCY_DISPOSABLE`: unload and recreate on demand

## Storage and Hibernation

The scaffold assumes `/sdcard` is mounted by the platform integration. `smartesp` then uses:

- `/sdcard/smartesp/runtime/` for `fayasm` spill blobs
- `/sdcard/apps/<app>/app.wasm` as the expected convention for external WASM apps

If `/sdcard` is not mounted, the system still boots, but SD-backed hibernation stays unavailable and disposable apps behave as cold-start workloads.

## Built-In Demo

The built-in `wasm.demo.add` application embeds a tiny WASM module that imports `env.host_add` and exports `run`. This demonstrates:

- loading WASM from memory
- binding native host functions
- executing an exported function through `fayasm`
- capturing the last result for the native shell

## Build

1. Initialize the submodule:

```bash
git submodule update --init --recursive
```

2. Build with ESP-IDF 5.4+:

```bash
idf.py set-target esp32s3
idf.py build
```

3. Flash and monitor:

```bash
idf.py flash monitor
```

## Validation Script

Use the shell validation runner in [`tests/test_idf_build.sh`](/Users/riccardo/Sources/GitHub/smartesp/tests/test_idf_build.sh).

Compile-only validation:

```bash
./tests/test_idf_build.sh
```

Incremental rerun without `fullclean`:

```bash
./tests/test_idf_build.sh --no-clean
```

Compile + flash + serial smoke validation:

```bash
./tests/test_idf_build.sh --flash --monitor --port /dev/cu.usbmodemXXXX
```

Default ESP-IDF path:

- `/Users/riccardo/esp/esp-idf-v6.0.1`

The built-in `smoke-ui` scenario validates native UI/runtime milestones through serial logs:

- `boot-ui stage="display ready" progress=10`
- `boot-ui stage="entering shell" progress=100`
- `shell-ui rendered`
- `demo app wasm.demo.add result=12`
- `smartesp started`

This is log-driven UI smoke validation, not touch-injection automation. If you later add hardware touch automation, extend this script rather than replacing it.

## Notes for Future Work

- Add real SD-card mounting for this board rather than only consuming an already-mounted `/sdcard`.
- Add a manifest parser for external apps under `/sdcard/apps`.
- Move the process manager from synchronous demo orchestration to long-lived tasks/process supervision.
- Add native fallback UI paths for runtime faults and failed app resumes.
- If `fayasm` needs changes, make them in [`external/fayasm`](/Users/riccardo/Sources/GitHub/smartesp/external/fayasm) and commit there directly, then update the submodule pointer here.
