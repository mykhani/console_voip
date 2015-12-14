/*
 * Read an audio file and play it back
 */


#include "voip.h"

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SAMPLES_PER_PERIOD 128


int main(int argc, char *argv[])
{
	int ret;
	unsigned int rate = 8000;
	int size;
	char *buffer;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames = SAMPLES_PER_PERIOD;
	
        printf("rate is =%d \n", rate);

	voip_init_pcm(&handle, &params, &frames, &rate, PLAYBACK);
	printf("In playback main \n");
	printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
	printf("Pointer to params=%p \n", params);
	buffer = voip_alloc_buf(params, &frames, &size);
	
	if (!buffer) {
		fprintf(stderr, "Unable to allocate buffer: %s \n", strerror(errno));
		ret = -ENOMEM;
		goto failure;
	}

	while(1) {
		ret = read(0, buffer, size); /* read from stdin */
		if (ret == 0) {
			fprintf(stderr, "end of file on input \n");
			break;
		} else if ( ret != size) {
			fprintf(stderr,
				"short read: read %d bytes \n", ret);
		}
		/* write frames in one period to device */
		ret = voip_playback(handle, &frames, buffer);
		
	}

	ret = 0;

failure:
	voip_end_pcm(handle);
	free(buffer);

	return ret;	
}

	
