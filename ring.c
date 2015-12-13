#include "ring.h"
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#define dbg(x) fprintf(stdout, x ":%s:%d \n", __FILE__, __LINE__)
#else
#define dbg(x) {} 
#endif

/* Note: The locks has been integrated into the ring data
 * structure meaning the read and write must be performed
 * in a separate thread. 
 * TODO: See if a generic solution for single thread based
 * read/write is possible
 */


/* The struct ring is not public i.e. it's fields are not
 * visible in the header file. Therefore provide get/set 
 * methods to access it's members 
 */

/* Declaration of circular buffer
 * buff : pointer to memory allocated for buffer
 * size : size of buffer
 * head : read position
 * tail : write postion
 * count: number of bytes to read
*/
struct ring {
	int size;
	int head;
	int tail;
	int count;
	char buff[];
};

int ring_alloc(struct ring **_ring, int size)
{
	struct ring *tmp;
	tmp = malloc(sizeof(struct ring) + size * sizeof(char));
	if (!tmp) {
		return -ENOMEM;
	}
	tmp->size = size;
	*_ring = tmp;

	return 0; 
}
void ring_init(struct ring *_ring)
{
	_ring->head = 0;
	_ring->tail = 0;
	_ring->count = 0;
}
int ring_head(struct ring *_ring)
{
	return _ring->head;
}
int ring_tail(struct ring *_ring)
{
	return _ring->tail;
}

int ring_size(struct ring *_ring)
{
        return _ring->size;
}

int ring_count(struct ring *_ring)
{
	return _ring->count;
}
int ring_write(struct ring *_ring, char *buff, int len)
{
	int i;

	/* check if the data would fit in */
	if (_ring->count + len > _ring->size) {
		dbg("No space. Waiting for data to be consumed");
		return -ENOMEM;
	}

	for (i=0; i< len; i++) {
		_ring->buff[_ring->tail++] = buff[i];
		_ring->count++;
		/* wrap around tail position */
		_ring->tail %= _ring->size;
	}

	return len;	
	
}
int ring_read(struct ring *_ring, char *buff, int len)
{
	int i;
	int count = 0;
	

	/* check if ring contains data to read */
	/* TODO: Maybe we shoud have a certain minimum number of bytes
	 * available istead of just greater than 0 to proceed with read?
	 */
	if (!(_ring->count > 0)) {
		dbg("No data. Waiting for data to be produced");
		return -ENOMEM;
	}
	
        for (i=0; i< len && _ring->count > 0; i++) {
                buff[i] = _ring->buff[_ring->head++];
		_ring->count--; /* subtract bytes read */
                count++;
                /* wrap around tail position */
                _ring->head %= _ring->size;
        }

	/* return number of bytes read */
	return count;

}

