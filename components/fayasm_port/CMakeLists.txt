set(FAYASM_ROOT "${PROJECT_DIR}/external/fayasm")

idf_component_register(
    SRCS
        "src/fayasm_port.c"
        "${FAYASM_ROOT}/src/fa_jit.c"
        "${FAYASM_ROOT}/src/fa_job.c"
        "${FAYASM_ROOT}/src/fa_ops.c"
        "${FAYASM_ROOT}/src/fa_runtime.c"
        "${FAYASM_ROOT}/src/fa_wasm.c"
        "${FAYASM_ROOT}/src/fa_wasm_stream.c"
    INCLUDE_DIRS
        "include"
        "${FAYASM_ROOT}/src"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    FAYASM_TARGET_ESP32=1
    FAYASM_TARGET_RAM_BYTES=(520ULL*1024ULL)
    FAYASM_TARGET_CPU_COUNT=2U
)
