#include <Debug/ExceptionHandler.hpp>
#include <Debug/LeakDetector.h>
#include <Debug/Logger.hpp>
#include <Debug/SymbolUtils.h>
#include <malloc.h>

void DefaultExceptionHandler(ERRF_ExceptionInfo* info, CpuRegisters* regs) {
    exitLeakDetector(false);

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    for(int i = 0; i <= 12; ++i) {
        printf("r%d: %08lX\n", i, regs->r[i]);
    }

    printf("sp: %08lX\n", regs->sp);
    printf("lr: %08lX\n", regs->lr);
    printf("pc: %08lX\n\n", regs->pc);

    switch(info->type) {
    case ERRF_EXCEPTION_PREFETCH_ABORT: printf("Prefetch Abort (%s)\n", info->fsr & (1 << 11) ? "Write" : "Read"); break;
    case ERRF_EXCEPTION_DATA_ABORT:     printf("Data Abort (%s)\n", info->fsr & (1 << 11) ? "Write" : "Read"); break;
    case ERRF_EXCEPTION_UNDEFINED:      printf("Undefined Instruction\n"); break;
    case ERRF_EXCEPTION_VFP:            printf("Floating Point Exception\n"); break;
    default:                            printf("Unknown Exception Type\n"); break;
    }

    printf("Stack Trace\n");
    const symbol_map_entry* entry = getSymbol(regs->pc);
    if(entry == NULL) {
        printf("0: %08lX\n", regs->pc);
    }
    else {
        printf("0: %08X+%lX %s\n", entry->address, regs->pc - entry->address, entry->name);
    }

    const symbol_map_entry* entry2 = getSymbol(regs->lr);
    if(entry2 == NULL) {
        printf("1?: %08lX\n", regs->pc);
    }
    else {
        printf("1?: %08X+%lX %s\n", entry2->address, regs->lr - entry2->address, entry2->name);
    }

    printf("\nPress Select to exit...\n");
    while(aptMainLoop()) {
        gspWaitForVBlank();
        gfxFlushBuffers();
        gfxSwapBuffers();

        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if(!(kDown & KEY_L || kHeld & KEY_L) && kDown & KEY_SELECT) {
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