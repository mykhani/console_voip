/*
 * Read an audio file and play it back
 */


#include "voip.h"

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SAMPLES_PER_PERIOD 1000


int main(int argc, char *argv[])
{
	int ret;
	unsigned int rate = 8000;
	int size;
	char *buffer;
	int fd;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames = SAMPLES_PER_PERIOD;
	
        printf("rate is =%d \n", rate);

	ret = voip_init_pcm(&handle, &params, &frames, &rate, RECORD);
	
	if (ret) {
		fprintf(stderr, "Unable to initialize PCM \n");
		return EXIT_FAILURE;
	}
	printf("In record main \n");
	printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
	printf("Pointer to params=%p \n", params);
	buffer = voip_alloc_buf(params, &frames, &size);
	
	if (!buffer) {
		fprintf(stderr, "Unable to allocate buffer: %s \n", strerror(errno));
		ret = -ENOMEM;
		goto failure;
	}
	
	fd = open("/home/ykhan/dev/c_learning/voip_console/recording", O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
	
	if (fd < 0) {
		fprintf(stderr, "Unable to open output file \n");
		return EXIT_FAILURE;
	}

	while(1) {
		voip_record(handle, &frames, buffer);
			
		/* write frames in one period to file */
		ret = write(fd, buffer, size);
		if (ret > 0 ) {
			printf("Written %d bytes to file \n", ret);
		} else {
			fprintf(stderr, "Unable to write to file: %s \n", strerror(errno));
		} 
	}

	ret = 0;

failure:
	voip_end_pcm(handle);
	free(buffer);

	return ret;	
}

	
