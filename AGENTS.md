# AI Reference

This file is the fast path for future agents working on `smartesp`. Keep it current. If you change behavior, structure, build steps, or workflow rules, update this file and `README.md` in the same turn.

## Non-Negotiable Directives

- Update `README.md` every time the architecture, workflow, build steps, or runtime behavior changes.
- Update this `AGENTS.md` every time the repository structure, coding rules, or contributor workflow changes.
- Treat [`external/fayasm`](/Users/richard/Sources/GitHub/smartesp/external/fayasm) as an upstream submodule repository: when you need to modify `fayasm`, commit inside that repo first, then update the submodule pointer here.
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
- After changing `external/fayasm`, run its native suite from the submodule with `./build.sh` (currently 113 tests); validate generated fixtures with both Emscripten and the Rust fallback when fixture/toolchain behavior changes.
- Compile both user apps with Emscripten: `./scripts/build_wasm_apps.sh` (`build_clock_demo_wasm.sh` is a compatibility alias).
- Upload `wasm_apps/build/watch.wasm` and `settings.wasm` with `push_wasm_serial.py --app watch/settings`, then reset for SD-loaded execution.
- Activate local ESP-IDF 6.1 with `source /Users/richard/.espressif/tools/activate_idf_v6.1.sh`.
- Run `node tests/test_wasm_apps.mjs` after app builds to validate compiled Watch encoding and Settings brightness bounds.
- After flash, SD app upload and reset to Watch, run `./tests/test_launcher_hardware.sh --port /dev/cu.usbmodem101` for real-device LVGL input and screenshot validation.
- Default local ESP-IDF path on this machine: `/Users/richard/esp-idf/esp-idf-v6.1/v6.1/esp-idf`
- Use `./tests/test_idf_build.sh --no-clean` for incremental reruns when the target is unchanged.
- `./tests/test_idf_build.sh --monitor` now resets the board and captures raw serial logs directly, so it works even when `idf_monitor` cannot attach to a TTY.

If you cannot run an ESP-IDF build in the current environment, say so clearly in the handoff.

## Project Map

Start from these files:

- [`assets/`](/Users/richard/Sources/GitHub/smartesp/assets): device-captured Watch, Apps, and Settings PNG screenshots embedded in `README.md`. Refresh these from the hardware smoke test captures when the visible UI changes.

- [`main/main.c`](/Users/richard/Sources/GitHub/smartesp/main/main.c): app entrypoint.
- [`components/watch_os/src/watch_os.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/watch_os.c): boot sequence, orchestration, periodic supervision.
- [`components/watch_os/src/board/watch_board.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/board/watch_board.c): display/LVGL/BSP bootstrap.
- [`components/watch_os/src/ui/app_menu.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/ui/app_menu.c): circular Watch/Settings launcher, brightness/error screens, serial pointer/snapshot diagnostics.
- [`components/watch_os/src/ui/native_ui.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/ui/native_ui.c): native boot and shell UI in C.
- [`components/watch_os/src/kernel/process_manager.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/kernel/process_manager.c): app slots and residency policy.
- [`components/watch_os/src/runtime/wasm_runtime.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/runtime/wasm_runtime.c): `fayasm` execution, host bindings, spill/load hooks.
- [`components/watch_os/src/storage/storage.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/storage/storage.c): runtime blob paths and file helpers.
- [`components/watch_os/src/io/serial_uploader.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/io/serial_uploader.c): USB/UART stdio protocol that installs external `app.wasm` payloads onto the SD card.
- [`components/watch_os/src/apps/app_catalog.c`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/apps/app_catalog.c): built-in app registry.
- [`components/watch_os/src/apps/clock_demo_wasm.h`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/apps/clock_demo_wasm.h): generated Emscripten-built clock demo bytes embedded into firmware.
- [`components/watch_os/src/apps/sample_add_wasm.h`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/src/apps/sample_add_wasm.h): embedded demo WASM bytes.
- [`components/fayasm_port/CMakeLists.txt`](/Users/richard/Sources/GitHub/smartesp/components/fayasm_port/CMakeLists.txt): ESP-IDF wrapper over upstream `fayasm`.
- [`wasm_apps/settings/settings.c`](/Users/richard/Sources/GitHub/smartesp/wasm_apps/settings/settings.c): SD-loaded Settings brightness policy, using `host_app_action` and `host_app_value`.
- [`scripts/build_wasm_apps.sh`](/Users/richard/Sources/GitHub/smartesp/scripts/build_wasm_apps.sh): Emscripten app build and Watch recovery header generation.
- [`scripts/watch_ui_serial.py`](/Users/richard/Sources/GitHub/smartesp/scripts/watch_ui_serial.py): real-device pointer/button smoke test and screenshots.
- [`wasm_apps/clock_demo/clock_demo.c`](/Users/richard/Sources/GitHub/smartesp/wasm_apps/clock_demo/clock_demo.c): C source for the built-in clock demo app compiled to WASM with Emscripten.
- [`scripts/build_clock_demo_wasm.sh`](/Users/richard/Sources/GitHub/smartesp/scripts/build_clock_demo_wasm.sh): regenerates the embedded clock demo header from the C source.
- [`scripts/push_wasm_serial.py`](/Users/richard/Sources/GitHub/smartesp/scripts/push_wasm_serial.py): host-side serial uploader for external WASM payloads.
- [`tests/test_idf_build.sh`](/Users/richard/Sources/GitHub/smartesp/tests/test_idf_build.sh): compile and optional flash/serial smoke validation.

## Execution Model

There are two deliberate paths:

- Native C path:
  boot, loading, shared shell, critical touch/display, power/input/notification services
- WASM path through `fayasm`:
  restartable apps, hibernable workloads, portable user-facing logic

Keep that separation visible in code and documentation.

## Display Rules

- Keep the SH8601 registered with `lvgl_port_add_disp()` as a QSPI/SPI display. Do not switch back to the Waveshare BSP's RGB registration path: it can release LVGL draw buffers before SPI color transfers complete and produce corrupted horizontal bands.
- Keep SH8601 invalidation coordinates rounded to even starts and even exclusive ends.
- Keep the AMOLED dark until the first full LVGL frame has been synchronously presented.
- Keep `CONFIG_ESP_MAIN_TASK_STACK_SIZE` at least 8192 bytes while display and touch initialization run synchronously from `app_main`; the ESP-IDF default overflows during this boot path.
- Place user-facing content inside the rounded panel's vertical safe region (currently y=58 through y=442) and dynamically inset any text bands that approach the curved corners.

## Launcher and App Rules

- Keep launcher, input, error presentation and sensitive Settings hardware/NVS operations native. WASM calculates app results; it never calls LVGL directly.
- Only Watch (`wasm.demo.clock`) and Settings (`wasm.settings`) appear in Apps. Preserve internal Demo Add and Workout stub catalog entries without advertising unimplemented apps.
- BOOT is GPIO0, active low, with 40 ms debounce. PWR uses TCA9554 EXIO6 (active high, I2C `0x20`) when present; older boards fall back to polling AXP2101 (`0x34`) short-press status at `0x49`, bit 3, enabled through `0x41`. Do not configure GPIO35: it is reserved for octal PSRAM. Leave PMIC long-press shutdown intact.
- UI callbacks enqueue actions only. The OS maintenance task owns navigation and all runtime calls; never execute WASM or SD/NVS operations inside LVGL event callbacks.
- Tick/render Watch only while visible; never let a periodic clock update switch away from another screen.
- Settings `run()` returns brightness from `host_app_action` (-1/0/1) and `host_app_value`; the OS validates 20–100%, applies it under the display lock, and persists NVS `watch_settings/brightness` (default 80%).
- SD installed modules take precedence. Watch alone has an embedded recovery copy used when its SD file is absent. Settings requires `/sdcard/apps/settings/app.wasm`. Existing installed-but-invalid files must show an error.
- Keep the serial/diagnostic task stack at least 12 KiB: synchronous LVGL snapshot rendering needs more stack than simple uploads. Yield periodically during full-frame USB streaming to service the idle watchdog.
- `WATCH-UI BUTTON/TAP/SNAPSHOT` are development serial diagnostics. TAP goes through LVGL pointer hit testing; snapshots use `CONFIG_LV_USE_SNAPSHOT`. They do not verify physical switches or sensor wiring.

## Residency Policy

Current policy classes live in [`components/watch_os/include/watch_os_app.h`](/Users/richard/Sources/GitHub/smartesp/components/watch_os/include/watch_os_app.h):

- `WATCH_APP_RESIDENCY_ALWAYS_RESIDENT`
- `WATCH_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE`
- `WATCH_APP_RESIDENCY_HIBERNABLE`
- `WATCH_APP_RESIDENCY_DISPOSABLE`

Foreground processes are excluded from memory-pressure victims. Background Watch is hibernable and pauses ticking; Settings is explicitly unloaded on exit as disposable.

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
- Keep native USB Serial/JTAG as the primary console (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`); the output-only secondary console cannot accept uploads. Install the buffered USB driver before starting serial I/O and assemble complete lines across USB packets.
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
