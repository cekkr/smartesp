# WASM Apps

C sources compile with Emscripten into SD-installable apps:

- `clock_demo/clock_demo.c` → `build/watch.wasm` (also `build/clock_demo.wasm`): Watch time logic, `wasm.demo.clock`.
- `settings/settings.c` → `build/settings.wasm`: Settings brightness policy, `wasm.settings`.

```bash
./scripts/build_wasm_apps.sh
./scripts/push_wasm_serial.py --app watch wasm_apps/build/watch.wasm
./scripts/push_wasm_serial.py --app settings wasm_apps/build/settings.wasm
```

Reset the watch after installing Watch so its cached runtime loads `/sdcard/apps/watch/app.wasm`. Settings cold-loads `/sdcard/apps/settings/app.wasm` when opened and unloads on exit. Watch's generated `components/watch_os/src/apps/clock_demo_wasm.h` is a recovery fallback for a missing SD payload. The older `build_clock_demo_wasm.sh` delegates to the complete app build.

Both apps export `run() -> i32`. Watch imports `env.host_time_ms() -> i64` and `env.host_log_i32(i32)`. Settings imports `env.host_app_action() -> i32` (-1 dim, 0 open, 1 brighten) and `env.host_app_value() -> i32` (current brightness), returning a value clamped to 20–100 in 20% steps. The native OS owns UI, display hardware and persistent NVS settings.
