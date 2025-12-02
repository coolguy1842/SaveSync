#ifndef __SYMBOL_UTILS_H__
#define __SYMBOL_UTILS_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unwind.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uintptr_t address;
    char* name;
} symbol_map_entry;

// addresses must be free'd later
typedef struct {
    int capacity;
    int size;

    uintptr_t* addresses;

    // for internal use
    _Unwind_Word cfa;
} stack_trace;

void initSymbolMap();
void freeSymbolMap();

size_t symbolMapSize();
// if nullptr, then symbols are not loaded/disabled
const symbol_map_entry* symbolMap();

// gets closest symbol to the address
const symbol_map_entry* getSymbol(uintptr_t addr);

extern _Unwind_Reason_Code StackTraceUnwindFunc(_Unwind_Context* context, void* data);
// offset is how far down in the stack to start, e.g 1 will do a function below the calling one

stack_trace allocateStackTrace(int maxDepth, int offset);
stack_trace getStackTrace(int maxDepth, int offset);
void freeStackTrace(stack_trace trace);

#ifdef __cplusplus
}
#endif

#endif