#ifndef _HEAP_H
#define _HEAP_H

#include <stdint.h>

uint8_t HeapRemove( uint8_t * heapdata, uint8_t * heaplen, uint16_t * heapvals );

void HeapAdd( uint8_t * heapdata, uint8_t * heaplen, uint8_t data, uint16_t * heapvals );

#endif


