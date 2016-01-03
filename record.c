/*
 * Read an audio file and play it back
 */

#include "voip.h"

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SAMPLES_PER_PERIOD 800

int main(int argc, char *argv[])
{
	int ret;
	unsigned int rate = 8000;
	short audio_samples[SAMPLES_PER_PERIOD];
	int buffer_size = sizeof audio_samples;
	int fd;
	int i;
	int frame_size = 2;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	
        printf("rate is =%d \n", rate);

	ret = voip_init_pcm(&handle, &params, &buffer_size, &rate, RECORD);
	
	if (ret) {
		fprintf(stderr, "Unable to initialize PCM \n");
		return EXIT_FAILURE;
	}
	printf("In record main \n");
	printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
	printf("Pointer to params=%p \n", params);
	
	fd = open("/home/ykhan/khan/voipp2/recording", O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
	
	if (fd < 0) {
		fprintf(stderr, "Unable to open output file \n");
		return EXIT_FAILURE;
	}
	/* record 50 periods, each of 100 ms, for 5 sec of recording */
	for (i = 0; i < 50; i++) {
		voip_capture(handle, buffer_size / frame_size, audio_samples);
			
		/* write frames in one period to file */
		ret = write(fd, audio_samples, buffer_size);
		if (ret > 0 ) {
			printf("Written %d bytes to file \n", ret);
		} else {
			fprintf(stderr, "Unable to write to file: %s \n", strerror(errno));
		} 
	}
	printf("Closing file \n");
	close(fd);

	ret = 0;

failure:
	voip_end_pcm(handle);

	return ret;	
}

	
