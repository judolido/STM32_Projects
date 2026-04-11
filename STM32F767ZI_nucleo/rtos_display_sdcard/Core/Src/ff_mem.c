/**
 * @file ff_mem.c
 * @brief FatFs memory management functions for FreeRTOS
 * 
 * This file provides malloc/free wrappers for FatFs when using
 * dynamic LFN allocation (_USE_LFN = 2).
 * 
 * Add this file to your project if you get linker errors for
 * ff_memalloc or ff_memfree.
 * 
 * IMPORTANT: Only needed if using _USE_LFN = 2 (dynamic allocation)
 */

#include "ff.h"
#include "cmsis_os.h"
#include <stdlib.h>

#if FF_USE_LFN == 2  /* Dynamic allocation */

/**
 * @brief Allocate memory for LFN working buffer
 * @param msize Memory size to allocate
 * @return Pointer to allocated memory, or NULL if failed
 */
void* ff_memalloc(UINT msize) {
    return pvPortMalloc(msize);
}

/**
 * @brief Free memory allocated by ff_memalloc
 * @param mblock Pointer to memory block to free
 */
void ff_memfree(void* mblock) {
    vPortFree(mblock);
}

#endif /* FF_USE_LFN == 2 */
