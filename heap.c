#include "heap.h"


uint8_t HeapRemove( uint8_t * heapdata, uint8_t * heaplen, uint16_t * heapvals )
{
	uint8_t ret = heapdata[0];

	(*heaplen)--;

	if( *heaplen == 0 ) ret;

	heapdata[0] = heapdata[*heaplen];

	uint8_t i = 0;
	while(  !( (i >= *heaplen/2) && (i < *heaplen) ) ) //Not a leaf...
	{
		int j = i*2+1; //left child.
		if( j >= *heaplen ) break;

		if( j < *heaplen -1 && heapvals[heapdata[j]] < heapvals[heapdata[j+1]] ) //?? is the > here right?
			j++;  //Get index of lowest? value.

		if( heapvals[heapdata[j]] < heapvals[heapdata[i]] ) break;

		uint8_t v = heapdata[j];
		heapdata[j] = heapdata[i];
		heapdata[i] = v;

		i = j;
	}

	return ret;
}


void HeapAdd( uint8_t * heapdata, uint8_t * heaplen, uint8_t data, uint16_t * heapvals )
{
	heapdata[*heaplen] = data;

	uint8_t curr = *heaplen;
	uint8_t parent = (curr-1)/2;

	while( curr != 0 && heapvals[heapdata[curr]] > heapvals[heapdata[parent]] )
	{
		uint8_t d = heapdata[curr];
		heapdata[curr] = heapdata[parent];
		heapdata[parent] = d;

		curr = parent;
		parent = (curr-1)/2;
	}

	(*heaplen)++;
}


