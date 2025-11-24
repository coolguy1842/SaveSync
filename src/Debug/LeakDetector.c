#include <Debug/LeakDetector.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unwind.h>

#ifdef DEBUG
// if not defined then disable any leak checking
#define ENABLE_LEAK_CHECK
#endif

void* __real_malloc(size_t size);
void* __real_calloc(size_t num, size_t size);
void* __real_realloc(void* ptr, size_t size);
void* __real_memalign(size_t alignment, size_t size);
char* __real_strdup(const char* str);
char* __real_strndup(const char* str, size_t max);

void __real_free(void* ptr);
int __real_main();

#ifdef ENABLE_LEAK_CHECK

static bool initialized          = false;
static leak_list_node* beginNode = NULL;
static pthread_mutex_t mutex;

typedef struct {
    uintptr_t address;

    char* name;
} symbol_map_entry;

static size_t symbol_map_size       = 0;
static symbol_map_entry* symbol_map = NULL;

typedef struct {
    int capacity;
    int size;
    uintptr_t* addresses;

    _Unwind_Word cfa;
} trace_state;

trace_state trace_state_init(int maxTraces) {
    trace_state state;
    state.capacity = maxTraces;

    // go 3 traces back before recording
    state.size      = -3;
    state.addresses = __real_calloc((size_t)maxTraces, sizeof(uintptr_t));

    return state;
}

void trace_state_free(trace_state state) {
    if(state.addresses != NULL) {
        __real_free(state.addresses);
    }
}

// thanks to oreo639: https://gist.github.com/oreo639/61204aa6e2501650bc2e57bdc39b18fd#file-main-3ds-c-L60, also requires -fexceptions
static _Unwind_Reason_Code trace_func(_Unwind_Context* context, void* data) {
    if(context == NULL || data == NULL) return _URC_FAILURE;
    trace_state* state = (trace_state*)data;

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

static inline trace_state get_stack_trace(int max) {
    trace_state state = trace_state_init(max);
    if(state.addresses == NULL) {
        state.size = 0;
        return state;
    }

    _Unwind_Backtrace(trace_func, &state);
    if(state.size <= 0) {
        state.size = 0;
        return state;
    }

    return state;
}

void freeSymbolMap() {
    if(symbol_map == NULL) {
        return;
    }

    for(size_t i = 0; i < symbol_map_size; i++) {
        symbol_map_entry entry = symbol_map[i];
        if(entry.name != NULL) {
            __real_free(entry.name);
        }
    }

    __real_free(symbol_map);
    symbol_map_size = 0;
}

void initSymbolMap(FILE* fp) {
    (void)fp;

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
            symbol_map_size = strtoull(buf, &end, 10);
            symbol_map      = __real_calloc(symbol_map_size, sizeof(symbol_map_entry));
            memset(symbol_map, 0, symbol_map_size * sizeof(symbol_map_entry));

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
            return;
        }

        continue;
    addEntry:
        reading = ADDRESS;

        offset++;
        if(symbolSize > 0) {
            symbol_map[numSymbol].address = address;
            symbol_map[numSymbol].name    = __real_strndup(symbol, symbolSize);
            numSymbol++;
        }

        address    = 0;
        symbolSize = 0;
    }

    (void)symbol;
    (void)start;
    (void)symbolSize;
    (void)address;
    (void)reading;
    (void)offset;
    symbol_map_size = numSymbol;
}

void add_ptr(u8 allocatedWith, void* ptr, size_t size) {
    if(!initialized || ptr == NULL || size == 0) {
        return;
    }

    leak_list_node* node = (leak_list_node*)__real_malloc(sizeof(leak_list_node));
    if(node == NULL) {
        return;
    }

    node->data          = ptr;
    node->dataSize      = size;
    node->tracesSize    = 0;
    node->allocatedWith = allocatedWith;

    pthread_mutex_lock(&mutex);
    if(symbol_map_size > 0) {
        trace_state state = get_stack_trace(LEAK_LIST_NODE_MAX_TRACES);

        for(int i = 0; i < state.size; i++) {
            uintptr_t instructionAddr = state.addresses[i];

            size_t left  = 0;
            size_t right = symbol_map_size - 1;

            while(left <= right) {
                size_t mid = left + (right - left) / 2;

                if(symbol_map[mid].address <= instructionAddr) {
                    left = mid + 1;
                }
                else {
                    right = mid - 1;
                }
            }

            const symbol_map_entry* entry = &symbol_map[right];

            leak_list_trace trace;
            trace.symbol  = entry->name;
            trace.address = entry->address;
            trace.offset  = instructionAddr - entry->address;

            node->traces[node->tracesSize++] = trace;
        }

        trace_state_free(state);
    }

    node->next = beginNode;
    beginNode  = node;
    pthread_mutex_unlock(&mutex);
}

void remove_ptr(void* ptr) {
    if(!initialized || ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex);

    leak_list_node* prev = NULL;
    leak_list_node* node = beginNode;
    while(node != NULL) {
        if(node->data != ptr) {
            prev = node;
            node = node->next;

            continue;
        }

        leak_list_node* next = node->next;
        if(prev == NULL) {
            beginNode = next;
        }
        else {
            prev->next = next;
        }

        __real_free(node);
        pthread_mutex_unlock(&mutex);
        return;
    }

    pthread_mutex_unlock(&mutex);
}

void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(MALLOC, ptr, size);
    return ptr;
}

void* __wrap_calloc(size_t num, size_t size) {
    void* ptr = __real_calloc(num, size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(CALLOC, ptr, num * size);
    return ptr;
}

void* __wrap_realloc(void* ptr, size_t size) {
    remove_ptr(ptr);

    void* newPtr = __real_realloc(ptr, size);
    if(newPtr == NULL) {
        return NULL;
    }

    add_ptr(REALLOC, newPtr, size);
    return newPtr;
}

void* __wrap_memalign(size_t alignment, size_t size) {
    void* ptr = __real_memalign(alignment, size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(MEMALIGN, ptr, size);
    return ptr;
}

char* __wrap_strdup(const char* str) {
    char* ptr = __real_strdup(str);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(STRDUP, ptr, strlen(str) + 1);
    return ptr;
}

char* __wrap_strndup(const char* str, size_t max) {
    char* ptr = __real_strndup(str, max);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(STRNDUP, ptr, strlen(ptr) + 1);
    return ptr;
}

void __wrap_free(void* ptr) {
    if(ptr == NULL) {
        return;
    }

    remove_ptr(ptr);
    __real_free(ptr);
}

int __wrap_main() {
    initLeakDetector();
    int code = __real_main();
    exitLeakDetector();

    return code;
}

void clearList(leak_list_node* begin) {
    if(begin == NULL) {
        return;
    }

    leak_list_node* node = begin;
    while(node != NULL) {
        leak_list_node* next = node->next;
        __real_free(node);

        node = next;
    }
}

void initLeakDetector() {
    if(initialized) {
        return;
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);

    romfsInit();

    FILE* fp = fopen("romfs:/SaveSync.lst", "r");
    if(fp != NULL) {
        initSymbolMap(fp);
        fclose(fp);
    }

    romfsExit();

    initialized = true;
    pthread_mutex_unlock(&mutex);
}

void exitLeakDetector() {
    if(!initialized) {
        return;
    }

    initialized = false;
    pthread_mutex_lock(&mutex);

    clearList(beginNode);
    beginNode = NULL;

    freeSymbolMap();
    pthread_mutex_destroy(&mutex);
}

void setLeakList(leak_list_node* begin) {
    if(!initialized) {
        return;
    }

    pthread_mutex_lock(&mutex);
    beginNode = begin;
    pthread_mutex_unlock(&mutex);
}

bool isDetectingLeaks() { return true; }
leak_list_node* leakListBegin() { return beginNode; }

void clearLeaks() {
    if(!initialized || beginNode == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex);

    clearList(beginNode);
    beginNode = NULL;

    pthread_mutex_unlock(&mutex);
}

void freeClonedList(leak_list_node* begin) {
    clearList(begin);
}

leak_list_node* cloneCurrentList() {
    if(!initialized || beginNode == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&mutex);

    leak_list_node* realNode = beginNode;
    leak_list_node *begin = NULL, *prevNode = NULL, *node = NULL;
    while(realNode != NULL) {
        node = (leak_list_node*)__real_malloc(sizeof(leak_list_node));
        memcpy(node, realNode, sizeof(leak_list_node));
        node->next = NULL;

        if(prevNode == NULL) {
            begin = node;
        }
        else {
            prevNode->next = node;
        }

        prevNode = node;
        realNode = realNode->next;
    }

    pthread_mutex_unlock(&mutex);

    return begin;
}

size_t leakListCurrentLeaked() {
    if(!initialized || beginNode == NULL) {
        return 0;
    }

    pthread_mutex_lock(&mutex);

    size_t leaked        = 0;
    leak_list_node* node = beginNode;
    while(node != NULL) {
        leaked += node->dataSize;
        node = node->next;
    }

    pthread_mutex_unlock(&mutex);

    return leaked;
}

#else

void* __wrap_malloc(size_t size) { return __real_malloc(size); }
void* __wrap_calloc(size_t num, size_t size) { return __real_calloc(num, size); }
void* __wrap_realloc(void* ptr, size_t size) { return __real_realloc(ptr, size); }
void* __wrap_memalign(size_t alignment, size_t size) { return __real_memalign(alignment, size); }
char* __wrap_strdup(const char* str) { return __real_strdup(str); }
char* __wrap_strndup(const char* str, size_t n) { return __real_strndup(str, n); }
void __wrap_free(void* ptr) { __real_free(ptr); }
int __wrap_main() { return __real_main(); }

void initLeakDetector() {}
void exitLeakDetector() {}

bool isDetectingLeaks() { return false; }

void setLeakList(leak_list_node* begin) { (void)begin; }
leak_list_node* leakListBegin() { return NULL; }
void clearLeaks() {}

leak_list_node* cloneCurrentList() { return NULL; }
void freeClonedList(leak_list_node* begin) { (void)begin; }

#endif
