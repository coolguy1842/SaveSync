#include <3ds.h>
#include <Debug/SymbolUtils.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#if defined(DEBUG) && !defined(DISABLE_SYMBOLS)

void* __real_malloc(size_t size);
void* __real_calloc(size_t num, size_t size);
void* __real_realloc(void* ptr, size_t size);
void* __real_memalign(size_t alignment, size_t size);
char* __real_strdup(const char* str);
char* __real_strndup(const char* str, size_t max);

void __real_free(void* ptr);

static size_t s_symbolMapSize        = 0;
static symbol_map_entry* s_symbolMap = NULL;

void initSymbolMap() {
    romfsInit();

    FILE* fp = fopen("romfs:/" EXE_NAME ".lst", "r");
    if(fp == NULL) {
        goto exit;
    }

    char buf[0x1000];
    size_t read = 0, offset = 0;
    size_t numSymbol = 0;

    bool start = true;

    uintptr_t address = 0;
    size_t symbolSize = 0;

#define MAX_SYMBOL 0x800
    char symbol[MAX_SYMBOL];

    enum {
        ADDRESS,
        SYMBOL
    } reading = ADDRESS;

    while(true) {
        if(offset >= read) {
            offset = 0;
            if((read = fread(buf, sizeof(buf[0]), sizeof(buf), fp)) <= 0) {
                break;
            }
        }

        if(start) {
            char* end;
            if((s_symbolMapSize = strtoull(buf, &end, 10)) == 0) {
                goto exit;
            }

            s_symbolMap = __real_calloc(s_symbolMapSize, sizeof(symbol_map_entry));
            memset(s_symbolMap, 0, s_symbolMapSize * sizeof(symbol_map_entry));

            offset = (size_t)(end - buf + 1);
            start  = false;
        }

        switch(reading) {
        case ADDRESS:
            for(; offset < read; offset++) {
                if(!isdigit((int)buf[offset])) {
                    reading = SYMBOL;
                    goto symbol;
                }

                address *= 10;
                address += buf[offset] - '0';
            }

            break;
        case SYMBOL:
        symbol:
            for(; offset < read; offset++) {
                char c = buf[offset];
                if(c == '\n') {
                    goto addEntry;
                }

                if(symbolSize < MAX_SYMBOL) {
                    symbol[symbolSize++] = c;
                }
            }

            break;
        default:
            freeSymbolMap();
            goto exit;
        }

        continue;
    addEntry:
        reading = ADDRESS;

        offset++;
        (void)symbol;
        if(symbolSize > 0) {
            s_symbolMap[numSymbol].address = address;
            s_symbolMap[numSymbol].name    = __real_strndup(symbol, symbolSize >= MAX_SYMBOL ? MAX_SYMBOL : symbolSize);
            numSymbol++;
        }

        address    = 0;
        symbolSize = 0;
    }

    s_symbolMapSize = numSymbol;

exit:
    fclose(fp);
    romfsExit();
}

void freeSymbolMap() {
    if(s_symbolMap == NULL) {
        return;
    }

    for(size_t i = 0; i < s_symbolMapSize; i++) {
        symbol_map_entry entry = s_symbolMap[i];
        if(entry.name != NULL) {
            __real_free(entry.name);
        }
    }

    __real_free(s_symbolMap);
    s_symbolMapSize = 0;
}

size_t symbolMapSize() { return s_symbolMapSize; }
const symbol_map_entry* symbolMap() { return s_symbolMap; }

const symbol_map_entry* getSymbol(uintptr_t addr) {
    if(s_symbolMapSize == 0 || s_symbolMap == NULL) {
        return NULL;
    }

    size_t left  = 0;
    size_t right = s_symbolMapSize - 1;

    while(left <= right) {
        size_t mid = left + (right - left) / 2;

        if(s_symbolMap[mid].address <= addr) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    return s_symbolMap + right;
}

// thanks to oreo639: https://gist.github.com/oreo639/61204aa6e2501650bc2e57bdc39b18fd#file-main-3ds-c-L60, also requires -fasynchronous-unwind-tables or -funwind-tables
_Unwind_Reason_Code StackTraceUnwindFunc(_Unwind_Context* context, void* data) {
    if(context == NULL || data == NULL) return _URC_FAILURE;
    stack_trace* state = (stack_trace*)data;

    if(state->size >= 0) {
        state->addresses[state->size] = _Unwind_GetIP(context);
        _Unwind_Word cfa              = _Unwind_GetCFA(context);

        if(state->size > 0 && state->addresses[state->size - 1] == state->addresses[state->size] && cfa == state->cfa) {
            return _URC_END_OF_STACK;
        }

        state->cfa = cfa;
    }

    if(++state->size == state->capacity) {
        return _URC_END_OF_STACK;
    }

    return _URC_NO_REASON;
}

stack_trace allocateStackTrace(int maxDepth, int offset) {
    stack_trace state;
    state.capacity  = maxDepth;
    state.size      = -(1 + offset);
    state.addresses = __real_calloc((size_t)maxDepth, sizeof(uintptr_t));

    return state;
}

stack_trace getStackTrace(int maxDepth, int offset) {
    stack_trace state = allocateStackTrace(maxDepth, offset + 1);
    if(state.addresses != NULL) {
        _Unwind_Backtrace(StackTraceUnwindFunc, &state);
    }

    if(state.size <= 0) {
        state.size = 0;
    }

    return state;
}

void freeStackTrace(stack_trace trace) {
    if(trace.addresses != NULL) {
        __real_free(trace.addresses);
    }
}

#else

void initSymbolMap() {}
void freeSymbolMap() {}

size_t symbolMapSize() { return 0; }
const symbol_map_entry* symbolMap() { return NULL; }
const symbol_map_entry* getSymbol(uintptr_t addr) {
    (void)addr;
    return NULL;
}

stack_trace allocateStackTrace(int maxDepth, int offset) {
    (void)maxDepth;
    (void)offset;

    stack_trace trace;
    trace.capacity = 0;
    trace.size     = 0;

    return trace;
}

void freeStackTrace(stack_trace trace) {
    (void)trace;
}

#endif