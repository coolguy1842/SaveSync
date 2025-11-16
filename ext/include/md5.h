#ifndef MD5_H
#define MD5_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t size;      // Size of input in bytes
    uint32_t buffer[4]; // Current accumulation of hash
    uint8_t input[64];  // Input to be used in the next step
    uint8_t digest[16]; // Result of algorithm
} MD5Context;

void md5Init(MD5Context* ctx);
void md5Update(MD5Context* ctx, uint8_t* input, size_t input_len);
void md5Finalize(MD5Context* ctx);
void md5Step(uint32_t* buffer, uint32_t* input);

void md5String(char* input, size_t inputLen, uint8_t* result);
void md5File(FILE* file, uint8_t* result);

#ifdef __cplusplus
}
#endif

#endif