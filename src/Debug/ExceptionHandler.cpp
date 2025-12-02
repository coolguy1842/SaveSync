#include <Debug/ExceptionHandler.hpp>
#include <Debug/Logger.hpp>
#include <Debug/SymbolUtils.h>
#include <functional>
#include <malloc.h>

struct UnwindState {
    stack_trace stack;
    CpuRegisters* regs;

    bool addAddress(uintptr_t ip) {
        if(stack.size >= stack.capacity) {
            return false;
        }

        if(ip == 0 || (stack.size > 0 && ip == stack.addresses[stack.size - 1])) {
            return true;
        }

        stack.addresses[stack.size++] = ip;
        return true;
    }
};

static _Unwind_Reason_Code unwind_func(_Unwind_Context* context, void* data) {
    if(context == NULL || data == NULL) return _URC_FAILURE;
    UnwindState* state = reinterpret_cast<UnwindState*>(data);

    if(state->stack.size == 0) {
        for(u8 i = 0; i <= 12; i++) {
            _Unwind_SetGR(context, i, state->regs->r[i]);
        }

        _Unwind_SetGR(context, 13, state->regs->sp);
        _Unwind_SetGR(context, 14, state->regs->lr);
        _Unwind_SetIP(context, state->regs->lr);

        state->addAddress(state->regs->pc);
    }

    return state->addAddress(_Unwind_GetIP(context)) ? _URC_NO_REASON : _URC_END_OF_STACK;
}

void _ExceptionHandlerInfoFormatter(ERRF_ExceptionInfo* info, CpuRegisters* regs, std::function<void(std::string)> callback) {
    for(int i = 0; i <= 12; ++i) {
        callback(std::format("r{}: {:08X}", i, regs->r[i]));
    }

    callback(std::format("sp: {:08X}", regs->sp));
    callback(std::format("lr: {:08X}", regs->lr));
    callback(std::format("pc: {:08X}\n", regs->pc));

    switch(info->type) {
    case ERRF_EXCEPTION_PREFETCH_ABORT: callback(std::format("Prefetch Abort ({})", info->fsr & (1 << 11) ? "Write" : "Read")); break;
    case ERRF_EXCEPTION_DATA_ABORT:     callback(std::format("Data Abort ({})", info->fsr & (1 << 11) ? "Write" : "Read")); break;
    case ERRF_EXCEPTION_UNDEFINED:      callback(std::format("Undefined Instruction")); break;
    case ERRF_EXCEPTION_VFP:            callback(std::format("Floating Point Exception")); break;
    default:                            callback(std::format("Unknown Exception Type")); break;
    }

    UnwindState state = {
        .stack = allocateStackTrace(10, 0),
        .regs  = regs
    };

    if(state.stack.addresses != NULL) {
        _Unwind_Backtrace(unwind_func, &state);
    }

    stack_trace stack = state.stack;
    callback(std::format("Stack Trace"));
    if(stack.size <= 0) {
        callback(std::format("Unwind failed, using only program counter"));
        stack.size = 0;

        const symbol_map_entry* entry = getSymbol(regs->pc);
        if(entry == NULL) {
            callback(std::format("0: {:X}", regs->pc));
        }
        else {
            callback(std::format("0: {:X}+{:X} {}", entry->address, regs->pc - entry->address, entry->name));
        }

        const symbol_map_entry* entry2 = getSymbol(regs->lr);
        if(entry2 == NULL) {
            callback(std::format("1?: {:X}", regs->pc));
        }
        else {
            callback(std::format("1?: {:X}+{:X} {}", entry2->address, regs->lr - entry2->address, entry2->name));
        }
    }

    for(int i = 0; i < stack.size; i++) {
        uintptr_t instructionAddr = stack.addresses[i];

        const symbol_map_entry* entry = getSymbol(instructionAddr);
        if(entry == NULL) {
            callback(std::format("{}: {:X}", i, instructionAddr));
            continue;
        }

        callback(std::format("{}: {:X}+{:X} {}", i, entry->address, instructionAddr - entry->address, entry->name));
    }

    freeStackTrace(stack);
}

void ExceptionHandlerLogInfo(ERRF_ExceptionInfo* info, CpuRegisters* regs) {
    _ExceptionHandlerInfoFormatter(info, regs, [](std::string msg) { Logger::log("{}", msg); });
}

std::string ExceptionHandlerFormatInfo(ERRF_ExceptionInfo* info, CpuRegisters* regs) {
    std::string out;
    _ExceptionHandlerInfoFormatter(info, regs, [&out](std::string msg) { out += msg + "\n"; });

    return out;
}

void DefaultExceptionHandler(ERRF_ExceptionInfo* info, CpuRegisters* regs) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    ExceptionHandlerLogInfo(info, regs);

    printf("\nPress Start to exit...\n");
    while(aptMainLoop()) {
        gspWaitForVBlank();
        gfxFlushBuffers();
        gfxSwapBuffers();

        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if(!(kDown & KEY_L || kHeld & KEY_L) && kDown & KEY_START) {
            break;
        }
    }

    gfxExit();
    exit(0);
}

extern const size_t __tdata_align;
extern const u8 __tdata_lma[];
extern const u8 __tdata_lma_end[];
extern u8 __tls_start[];
extern u8 __tls_end[];

static inline size_t alignTo(const size_t base, const size_t align) {
    return (base + (align - 1)) & ~(align - 1);
}

void* allocateHandlerStack(u64 stackSize) {
    size_t align = __tdata_align > 8 ? __tdata_align : 8;

    size_t stackoffset = alignTo(0, align);
    size_t allocsize   = alignTo(stackoffset + stackSize, align);

    size_t tlssize = static_cast<size_t>(__tls_end - __tls_start);
    size_t size    = alignTo(allocsize + tlssize, align);

    return memalign(align, size);
}