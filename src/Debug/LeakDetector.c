#include <Debug/LeakDetector.h>
#include <Debug/SymbolUtils.h>
#include <citro2d.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unwind.h>

void* __real_malloc(size_t size);
void* __real_calloc(size_t num, size_t size);
void* __real_realloc(void* ptr, size_t size);
void* __real_memalign(size_t alignment, size_t size);
char* __real_strdup(const char* str);
char* __real_strndup(const char* str, size_t max);

void __real_free(void* ptr);
int __real_main();

#if defined(DEBUG) && !defined(DISABLE_LEAK_CHECK)

static bool initialized          = false;
static leak_list_node* beginNode = NULL;
static LightLock mutex;

void add_ptr(u8 allocatedWith, void* ptr, size_t size, void* pc) {
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

    stack_trace traces = getStackTrace(LEAK_LIST_NODE_MAX_TRACES, 1);
    for(int i = 0; i < traces.size; i++) {
        uintptr_t addr                = traces.addresses[i];
        const symbol_map_entry* entry = getSymbol(addr);

        leak_list_trace* trace = &node->traces[node->tracesSize++];
        if(entry == NULL) {
            trace->symbol  = "unknown";
            trace->address = addr;
            trace->offset  = 0;
        }
        else {
            trace->symbol  = entry->name;
            trace->address = entry->address;
            trace->offset  = addr - entry->address;
        }
    }

    if(traces.size <= 0) {
        const char* ignored[] = {
            "MemPool::Deallocate",
            "setvbuf",
            NULL
        };

        uintptr_t addr                = (uintptr_t)pc;
        const symbol_map_entry* entry = getSymbol(addr);

        // ignore these static buffers
        if(entry != NULL) {
            for(const char** str = ignored; *str; str++) {
                if(strncmp(*str, entry->name, strlen(*str)) == 0) {
                    goto skipTrace;
                }
            }
        }

        leak_list_trace* trace = &node->traces[node->tracesSize++];

        if(entry == NULL) {
            trace->symbol  = "unknown";
            trace->address = addr;
            trace->offset  = 0;
        }
        else {

            trace->symbol  = entry->name;
            trace->address = entry->address;
            trace->offset  = addr - entry->address;
        }
    }

skipTrace:
    freeStackTrace(traces);

    LightLock_Lock(&mutex);
    node->next = beginNode;
    beginNode  = node;
    LightLock_Unlock(&mutex);
}

void remove_ptr(void* ptr) {
    if(!initialized || ptr == NULL) {
        return;
    }

    LightLock_Lock(&mutex);

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
        break;
    }

    LightLock_Unlock(&mutex);
}

void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(MALLOC, ptr, size, __builtin_return_address(0));
    return ptr;
}

void* __wrap_calloc(size_t num, size_t size) {
    void* ptr = __real_calloc(num, size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(CALLOC, ptr, num * size, __builtin_return_address(0));
    return ptr;
}

void* __wrap_realloc(void* ptr, size_t size) {
    remove_ptr(ptr);

    void* newPtr = __real_realloc(ptr, size);
    if(newPtr == NULL) {
        return NULL;
    }

    add_ptr(REALLOC, newPtr, size, __builtin_return_address(0));
    return newPtr;
}

void* __wrap_memalign(size_t alignment, size_t size) {
    void* ptr = __real_memalign(alignment, size);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(MEMALIGN, ptr, size, __builtin_return_address(0));
    return ptr;
}

char* __wrap_strdup(const char* str) {
    char* ptr = __real_strdup(str);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(STRDUP, ptr, strlen(str) + 1, __builtin_return_address(0));
    return ptr;
}

char* __wrap_strndup(const char* str, size_t max) {
    char* ptr = __real_strndup(str, max);
    if(ptr == NULL) {
        return NULL;
    }

    add_ptr(STRNDUP, ptr, strlen(ptr) + 1, __builtin_return_address(0));
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
    exitLeakDetector(true);

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

    // hide from leak list
    C2D_TextBufDelete(C2D_TextBufNew(0));

    LightLock_Init(&mutex);
    LightLock_Lock(&mutex);

    initSymbolMap();

    initialized = true;
    LightLock_Unlock(&mutex);
}

void exitLeakDetector(bool useMutex) {
    if(!initialized) {
        return;
    }

    initialized = false;
    if(useMutex) {
        LightLock_Lock(&mutex);
    }

    clearList(beginNode);
    beginNode = NULL;

    freeSymbolMap();
    if(useMutex) {
        LightLock_Unlock(&mutex);
    }
}

void setLeakList(leak_list_node* begin) {
    if(!initialized) {
        return;
    }

    LightLock_Lock(&mutex);
    beginNode = begin;
    LightLock_Unlock(&mutex);
}

bool isDetectingLeaks() { return true; }
leak_list_node* leakListBegin() { return beginNode; }

void clearLeaks() {
    if(!initialized || beginNode == NULL) {
        return;
    }

    LightLock_Lock(&mutex);

    clearList(beginNode);
    beginNode = NULL;

    LightLock_Unlock(&mutex);
}

void freeClonedList(leak_list_node* begin) {
    clearList(begin);
}

leak_list_node* cloneCurrentList() {
    if(!initialized || beginNode == NULL) {
        return NULL;
    }

    LightLock_Lock(&mutex);

    leak_list_node *realNode = beginNode, *begin = NULL;
    while(realNode != NULL) {
        leak_list_node* node = (leak_list_node*)__real_malloc(sizeof(leak_list_node));
        memcpy(node, realNode, sizeof(leak_list_node));

        node->next = begin;
        begin      = node;

        realNode = realNode->next;
    }

    LightLock_Unlock(&mutex);
    return begin;
}

size_t leakListCurrentLeaked() {
    if(!initialized || beginNode == NULL) {
        return 0;
    }

    LightLock_Lock(&mutex);

    size_t leaked        = 0;
    leak_list_node* node = beginNode;
    while(node != NULL) {
        leaked += node->dataSize;
        node = node->next;
    }

    LightLock_Unlock(&mutex);
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
void exitLeakDetector(bool) {}

bool isDetectingLeaks() { return false; }

void setLeakList(leak_list_node* begin) { (void)begin; }
leak_list_node* leakListBegin() { return NULL; }
void clearLeaks() {}

leak_list_node* cloneCurrentList() { return NULL; }
void freeClonedList(leak_list_node* begin) { (void)begin; }
size_t leakListCurrentLeaked() { return 0; }

#endif
