/*
 * Read an audio file and play it back
 */


#include "voip.h"
#include "ring.h"
/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SAMPLES_PER_PERIOD 800

int main(int argc, char *argv[])
{
	int ret;
	unsigned int rate = 8000;
	short audio_samples[SAMPLES_PER_PERIOD];

	int buffer_size = sizeof audio_samples;
	int frame_size = 2;
	struct ring *rbuff;
	if (ring_alloc(&rbuff, sizeof audio_samples * 1024)) {
                fprintf(stderr, "Unable to allocate RX ring: %s \n",
                        strerror(errno));
                return EXIT_FAILURE;
	}
	ring_init(rbuff);	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	
        printf("rate is =%d \n", rate);

	voip_init_pcm(&handle, &params, &buffer_size, &rate, PLAYBACK);
	printf("In playback main \n");
	printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
	printf("Pointer to params=%p \n", params);

	while(1) {
		ret = read(0, audio_samples, buffer_size); /* read from stdin */
		if (ret == 0) {
			fprintf(stderr, "end of file on input \n");
			break;
		} else if ( ret != buffer_size) {
			fprintf(stderr,
				"short read: read %d bytes \n", ret);
		}
		printf("Read %d bytes \n", ret);
		ring_write(rbuff, (char *)audio_samples, ret);
		
	}
	printf("Starting playback \n");
	while(ring_read(rbuff, (char *)audio_samples, buffer_size) > 0) {
		/* write frames in one period to device */
		ret = voip_playback(handle, buffer_size / frame_size, audio_samples);
	}
		
	ret = 0;

failure:
	voip_end_pcm(handle);

	return ret;	
}

	
