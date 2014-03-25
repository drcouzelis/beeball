#ifndef MEMORY_H
#define MEMORY_H


#include <malloc.h>


void show_memory_label();

/**
 * Call "malloc" and increase the memory allocation counter.
 */
void *alloc_memory(const char *label, size_t size);

/**
 * Call "calloc" and increase the memory allocation counter.
 */
void *calloc_memory(const char *label, size_t nmemb, size_t size);

/**
 * Call "free" and increase the memory deallocation counter.
 */
void *free_memory(const char *label, void *ptr);

/**
 * Check to see if the number of allocations
 * matches the number of frees.
 */
void check_memory();


#endif
