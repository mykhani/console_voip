#include "ring.h"
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
	struct ring *rbuff;
	int rc;
	char buff[512];
	ring_alloc(&rbuff, 512);
	ring_init(rbuff);
	printf("Ring buffer allocated with size %d \n", ring_size(rbuff));
	sprintf(buff, "Testing circular buffer 1st pass");
	
	printf("Ring tail is %d \n", ring_tail(rbuff));
	while (ring_write(rbuff,buff, (strlen(buff) + 1)) > 0) {
		printf("Bytes written to ring : %d \n", ring_count(rbuff));
		printf("Ring tail is %d \n", ring_tail(rbuff));
	}
	
	printf("Ring head is %d \n", ring_head(rbuff));
	
	memset(buff, 0, sizeof buff);
	while ((rc = ring_read(rbuff,buff, 33)) > 0) {
                printf("%d Bytes read from ring \n", rc);
		printf("Buffer content: %s \n", buff);
                printf("Ring head is %d \n", ring_head(rbuff));
                printf("Bytes remaining in ring: %d \n", ring_count(rbuff));
		
        }

	memset(buff, 0, sizeof buff);
	sprintf(buff, "Testing circular buffer 2nd pass");
	printf("Ring tail is %d \n", ring_tail(rbuff));
	while (ring_write(rbuff,buff, (strlen(buff) + 1)) > 0) {
                printf("Bytes written to ring : %d \n", ring_count(rbuff));
                printf("Ring tail is %d \n", ring_tail(rbuff));
        }

	printf("Ring head is %d \n", ring_head(rbuff));

        memset(buff, 0, sizeof buff);
        while ((rc = ring_read(rbuff,buff, 33)) > 0) {
                printf("%d Bytes read from ring \n", rc);
                printf("Buffer content: %s \n", buff);
                printf("Ring head is %d \n", ring_head(rbuff));
                printf("Bytes remaining in ring: %d \n", ring_count(rbuff));

        }


	
}



