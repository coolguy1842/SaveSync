#ifndef __LEAK_DETECTOR_H__
#define __LEAK_DETECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MALLOC,
    CALLOC,
    STRDUP,
    STRNDUP,
    REALLOC,
    MEMALIGN,
    UNKNOWN
} leak_list_node_allocated_with;

typedef struct leak_list_node {
    struct leak_list_node* next;

    void* data;
    size_t dataSize;
    u8 allocatedWith;

} leak_list_node;

void initLeakDetector();
void exitLeakDetector();

bool isDetectingLeaks();
void clearLeaks();

// try not to use this
void setLeakList(leak_list_node* begin);

// begin to linked list, can be null
leak_list_node* leakListBegin();
size_t leakListCurrentLeaked();

// useful for snapshots
leak_list_node* cloneCurrentList();
void freeClonedList(leak_list_node* begin);

#ifdef __cplusplus
}
#endif

#endif