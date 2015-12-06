
#include "voip.h"

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

int voip_init_pcm(snd_pcm_t **handle, snd_pcm_hw_params_t **params, snd_pcm_uframes_t *frames, unsigned int *rate)
{
	int ret;

	printf("Pointer address to handle=%p \n", handle);
	printf("Pointer to handle=%p \n", *handle);

	/* open PCM device for playback */
	ret = snd_pcm_open(handle, "plughw",
	                SND_PCM_STREAM_PLAYBACK, 0 );
	printf("Pointer address to handle=%p \n", handle);
	printf("Pointer to handle=%p \n", *handle);
	if (ret < 0) {
	        fprintf(stderr,
	                "unable to open pcm device: %s\n",
	                snd_strerror(ret));
	        return ret;
	}
        printf("Pointer address to params=%p \n", params);
        printf("Pointer to params=%p \n", *params);
	
	/* allocate hardware parameters object */
	snd_pcm_hw_params_alloca(params);

	printf("%s After allocation \n", __func__);
        printf("Pointer address to params=%p \n", params);
        printf("Pointer to params=%p \n", *params);
        
        printf("requested rate is =%d \n", *rate);
	
	/* fill it in with default values */
	snd_pcm_hw_params_any(*handle, *params);

	/* set the desired hardware parameters */
	
	/* interleaved mode */
	snd_pcm_hw_params_set_access(*handle, *params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
		
	/* signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(*handle, *params,
			SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(*handle, *params, 2);

	/* 44100 bits/second sampling rate (cd quality) */
	snd_pcm_hw_params_set_rate_near(*handle, *params,
					rate, NULL);

	/* set period size to FRAMES_PER_PERIOD frames */
	//frames = FRAMES_PER_PERIOD;
        printf("requested frame size is =%d \n", (int)*frames);
	snd_pcm_hw_params_set_period_size_near(*handle, *params,
					frames, NULL);

	snd_pcm_hw_params_get_period_size(*params, frames,
                                        NULL);
        printf("Actual frame size is =%d \n", (int)*frames);


	/* write the parameters to driver */
	ret = snd_pcm_hw_params(*handle, *params);
	if (ret < 0) {
		fprintf(stderr,
			"unable to set hardware parameters: %s \n",
			snd_strerror(ret));
		return ret;
	}

	return 0;
}

char *voip_alloc_buf(snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *size)
{
	char *buf = NULL;

	printf("Pointer address to params=%p \n", &params);
        printf("Pointer to params=%p \n", params);

        printf("Actual frame size is =%d \n", (int)*frames);
	
	*size = *frames * 4; /* 2 bytes/sample, 2 channels */
	printf("Size of buffer to be allocated = %u \n", *size);
	buf = malloc(*size * sizeof(char));

	if (!buf) {
		*size = 0;
		return NULL;
	}
	return buf;
}

int voip_playback(snd_pcm_t *handle, snd_pcm_uframes_t *frames, char *buffer)
{
	int ret;
	/* write frames in one period to device */
	ret = snd_pcm_writei(handle, buffer, *frames);
	
	if (ret == -EPIPE) {
	        /* EPIPE means underrun */
	        fprintf(stderr, "underrun occurred \n");
	        snd_pcm_prepare(handle);
	} else if (ret < 0) {
	        fprintf(stderr,
	                "error from writei: %s \n",
	                snd_strerror(ret));
	} else if (ret != (int)*frames) {
	        fprintf(stderr,
	                "short write: written %d frames \n",
	                ret);
	}
	return ret;

}

void voip_end_pcm(snd_pcm_t *handle)
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

