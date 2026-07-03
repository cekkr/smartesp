# Built-In WASM Sources

This directory holds source files for built-in WASM apps that are embedded into the firmware as generated headers.

Current workflow:

```bash
./scripts/build_clock_demo_wasm.sh
```

That command compiles [`clock_demo/clock_demo.c`](/Users/riccardo/Sources/GitHub/smartesp/wasm_apps/clock_demo/clock_demo.c) with Emscripten and refreshes [`components/watch_os/src/apps/clock_demo_wasm.h`](/Users/riccardo/Sources/GitHub/smartesp/components/watch_os/src/apps/clock_demo_wasm.h).
