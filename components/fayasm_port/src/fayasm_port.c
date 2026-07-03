#include "fayasm_port.h"

#include "fa_runtime.h"

const char *fayasm_port_status_string(int status)
{
    switch (status) {
    case FA_RUNTIME_OK:
        return "ok";
    case FA_RUNTIME_ERR_INVALID_ARGUMENT:
        return "invalid-argument";
    case FA_RUNTIME_ERR_OUT_OF_MEMORY:
        return "out-of-memory";
    case FA_RUNTIME_ERR_NO_MODULE:
        return "no-module";
    case FA_RUNTIME_ERR_STREAM:
        return "stream-error";
    case FA_RUNTIME_ERR_UNSUPPORTED:
        return "unsupported";
    case FA_RUNTIME_ERR_UNIMPLEMENTED_OPCODE:
        return "unimplemented-opcode";
    case FA_RUNTIME_ERR_CALL_DEPTH_EXCEEDED:
        return "call-depth-exceeded";
    case FA_RUNTIME_ERR_TRAP:
        return "trap";
    default:
        return "unknown";
    }
}
