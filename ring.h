/* Circular buffer for audio samples */
#ifndef __ring_h__
#define __ring_h__

struct ring;

int ring_alloc(struct ring **_ring, int size);
void ring_init(struct ring *_ring);
int ring_head(struct ring *_ring);
int ring_tail(struct ring *_ring);
int ring_size(struct ring *_ring);
int ring_count(struct ring *_ring);
int ring_write(struct ring *_ring, char *buff, int len);
int ring_read(struct ring *_ring, char *buff, int len);

#endif
