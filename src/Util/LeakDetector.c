#include <Util/LeakDetector.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

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
    node->allocatedWith = allocatedWith;

    pthread_mutex_lock(&mutex);
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
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);

    initialized = true;

    clearList(beginNode);
    beginNode = NULL;

    pthread_mutex_unlock(&mutex);
}

void exitLeakDetector() {
    pthread_mutex_lock(&mutex);

    clearList(beginNode);
    initialized = false;

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
        node                = (leak_list_node*)__real_malloc(sizeof(leak_list_node));
        node->next          = NULL;
        node->data          = realNode->data;
        node->dataSize      = realNode->dataSize;
        node->allocatedWith = realNode->allocatedWith;

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
