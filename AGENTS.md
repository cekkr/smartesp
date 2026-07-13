# AI Reference

This file is the fast path for future agents working on `smartesp`. Keep it current. If you change behavior, structure, build steps, or workflow rules, update this file and `README.md` in the same turn.

## Non-Negotiable Directives

- Update `README.md` every time the architecture, workflow, build steps, or runtime behavior changes.
- Update this `AGENTS.md` every time the repository structure, coding rules, or contributor workflow changes.
- Treat [`external/fayasm`](/Users/riccardo/Sources/GitHub/smartesp/external/fayasm) as an upstream submodule repository: when you need to modify `fayasm`, commit inside that repo first, then update the submodule pointer here.
- Keep boot/loading/sensitive/shared UI paths native in C unless there is a strong and documented reason not to.
- Keep the process residency model explicit. Do not hide swapping/hibernation decisions inside unrelated code.
- When you add or remove apps, update the catalog documentation in `README.md` and the code map below.

## Build Checklist

- Initialize submodules: `git submodule update --init --recursive`
- ESP-IDF target: `esp32s3`
- Expected reference board: Waveshare ESP32-S3 Touch AMOLED 2.06
- Main build command: `idf.py build`
- Flash/monitor: `idf.py flash monitor`
- Primary validation script: `./tests/test_idf_build.sh`
- After changing `external/fayasm`, run its native suite from the submodule with `./build.sh`; validate generated fixtures with both Emscripten and the Rust fallback when fixture/toolchain behavior changes.
- Regenerate the built-in clock WASM after editing it: `./scripts/build_clock_demo_wasm.sh`
- Default local ESP-IDF path on this machine: `/Users/riccardo/esp/esp-idf-v6.0.1`
- Use `./tests/test_idf_build.sh --no-clean` for incremental reruns when the target is unchanged.
- `./tests/test_idf_build.sh --monitor` now resets the board and captures raw serial logs directly, so it works even when `idf_monitor` cannot attach to a TTY.

If you cannot run an ESP-IDF build in the current environment, say so clearly in the handoff.

## Project Map

Start from these files:

- [`main/main.c`](/Users/riccardo/Sources/GitHub/smartesp/main/main.c): app entrypoint.
- [`components/watch_os/src/watch_os.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/watch_os.c): boot sequence, orchestration, periodic supervision.
- [`components/watch_os/src/board/watch_board.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/board/watch_board.c): display/LVGL/BSP bootstrap.
- [`components/watch_os/src/ui/native_ui.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/ui/native_ui.c): native boot and shell UI in C.
- [`components/watch_os/src/kernel/process_manager.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/kernel/process_manager.c): app slots and residency policy.
- [`components/watch_os/src/runtime/wasm_runtime.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/runtime/wasm_runtime.c): `fayasm` execution, host bindings, spill/load hooks.
- [`components/watch_os/src/storage/storage.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/storage/storage.c): runtime blob paths and file helpers.
- [`components/watch_os/src/io/serial_uploader.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/io/serial_uploader.c): USB/UART stdio protocol that installs external `app.wasm` payloads onto the SD card.
- [`components/watch_os/src/apps/app_catalog.c`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/apps/app_catalog.c): built-in app registry.
- [`components/watch_os/src/apps/clock_demo_wasm.h`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/apps/clock_demo_wasm.h): generated Emscripten-built clock demo bytes embedded into firmware.
- [`components/watch_os/src/apps/sample_add_wasm.h`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/apps/sample_add_wasm.h): embedded demo WASM bytes.
- [`components/fayasm_port/CMakeLists.txt`](/Users/riccardo/Sources/GitHub/smartesp/components/fayasm_port/CMakeLists.txt): ESP-IDF wrapper over upstream `fayasm`.
- [`wasm_apps/clock_demo/clock_demo.c`](/Users/riccardo/Sources/GitHub/smartesp/wasm_apps/clock_demo/clock_demo.c): C source for the built-in clock demo app compiled to WASM with Emscripten.
- [`scripts/build_clock_demo_wasm.sh`](/Users/riccardo/Sources/GitHub/smartesp/scripts/build_clock_demo_wasm.sh): regenerates the embedded clock demo header from the C source.
- [`scripts/push_wasm_serial.py`](/Users/riccardo/Sources/GitHub/smartesp/scripts/push_wasm_serial.py): host-side serial uploader for external WASM payloads.
- [`tests/test_idf_build.sh`](/Users/riccardo/Sources/GitHub/smartesp/tests/test_idf_build.sh): compile and optional flash/serial smoke validation.

## Execution Model

There are two deliberate paths:

- Native C path:
  boot, loading, shared shell, critical touch/display, power/input/notification services
- WASM path through `fayasm`:
  restartable apps, hibernable workloads, portable user-facing logic

Keep that separation visible in code and documentation.

## Residency Policy

Current policy classes live in [`components/watch_os/include/watch_os_app.h`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/include/watch_os_app.h):

- `WATCH_APP_RESIDENCY_ALWAYS_RESIDENT`
- `WATCH_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE`
- `WATCH_APP_RESIDENCY_HIBERNABLE`
- `WATCH_APP_RESIDENCY_DISPOSABLE`

When changing policy behavior:

- update the enum docs
- update the process manager
- update `README.md`
- update this file

## Storage Rules

- The watch OS mounts `/sdcard` directly in `storage.c` (not via `bsp_sdcard_mount()`) when SD-backed runtime is enabled, because the 2.06" board needs `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` and a probing-clock retry that the BSP helper does not enable. It still reuses the BSP pin macros (`BSP_SD_CLK/CMD/D0`) and the global `bsp_sdcard` handle.
- Runtime spill blobs go under `/sdcard/smartesp/runtime/`.
- External WASM apps should converge on `/sdcard/apps/<app>/app.wasm`.
- FAT-formatted cards are the safe default. When a card initializes but FatFs reports no usable filesystem or an inconsistent filesystem, boot-time mounting automatically repartitions and formats it as FAT; this is destructive to the card's existing contents. Transport/enumeration failures never trigger formatting.
- During early development, external WASM payloads are transferred over USB serial with `./scripts/push_wasm_serial.py`, which speaks the device-side `WASM-UPLOAD` stdio protocol.
- Built-in Emscripten-generated WASM headers should be regenerated from `wasm_apps/` sources before firmware builds when those sources change.
- If you add manifests, document the schema in `README.md` and link it here.

## `fayasm` Rules

- Prefer integration changes in `smartesp` before changing upstream `fayasm`.
- If upstream changes are necessary, make them inside the submodule and commit there directly.
- After any upstream runtime behavior change, sync:
  - `external/fayasm/README.md`
  - `external/fayasm/AGENTS.md`
  - the submodule pointer in this repository

## When Starting New Work

- Read this file first.
- Read `README.md` second.
- Check whether the requested change belongs in native UI, process policy, storage, or `fayasm` runtime glue before editing.
- Preserve the distinction between board bootstrap and OS logic.
- Preserve the distinction between runtime policy and UI presentation.
