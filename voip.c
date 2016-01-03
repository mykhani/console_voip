
#include "voip.h"

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
/*
 * buffer_size : size of data in bytes in a single period
 */
int voip_init_pcm(snd_pcm_t **handle, snd_pcm_hw_params_t **params, int *buffer_size, unsigned int *rate, int mode)
{
	int ret;
	unsigned int periods = 16;
	int frame_size = 2; /* 4 bytes for stereo, interleaved mode */
	snd_pcm_uframes_t frames;
	int _rate = *rate;;


	printf("Pointer address to handle=%p \n", handle);
	printf("Pointer to handle=%p \n", *handle);
	printf("Mode specified : %d \n", mode);
	/* open PCM device for playback/capture */
	switch (mode) {
	case PLAYBACK:
		ret = snd_pcm_open(handle, "pulse",
                        SND_PCM_STREAM_PLAYBACK, 0 );
		break;
	case RECORD: 
		ret = snd_pcm_open(handle, "pulse",
                        SND_PCM_STREAM_CAPTURE, 0 );
		break;
	default: 
		fprintf(stderr, "Invalid mode specified \n");
		return -EINVAL;
	}
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
	snd_pcm_hw_params_set_channels(*handle, *params, 1);

	/* setnumber of periods */
	ret = snd_pcm_hw_params_set_periods_near(*handle, *params, &periods, 0);

	if (ret < 0) {
		fprintf(stderr, "Error setting # periods to %d: %s\n",
			periods, strerror(ret));
		return ret;
	}

	frames = *buffer_size / frame_size * periods;

	ret = snd_pcm_hw_params_set_buffer_size_near(*handle, *params, &frames);

	if (ret < 0) {
                fprintf(stderr,
                        "Error setting buffer size %d frames: %s \n",
                        (int)frames, strerror(ret));
                return ret;
	}

	if (*buffer_size != frames * frame_size / periods) {
		fprintf(stderr, "Couldn't set requested buffer size, asked for %d, got %d \n",
			*buffer_size, (int) frames * frame_size / periods);
		*buffer_size = frames * frame_size / periods;
	}

	/* set desired sampling rate */
	ret = snd_pcm_hw_params_set_rate_near(*handle, *params,
					rate, NULL);
	if (ret < 0) {
                fprintf(stderr,
                        "Couldn't set sample rate : %s\n",
                        strerror(ret));
                return ret;
        }
	
	if (_rate != *rate) {
		fprintf(stderr, "Couldn't set requested sample rate, rqst %d, got %d \n",
			_rate, *rate);
	}

	
	/* set period size to FRAMES_PER_PERIOD frames */
	//frames = FRAMES_PER_PERIOD;
        //printf("requested frame size is =%d \n", (int)*frames);
	//snd_pcm_hw_params_set_period_size_near(*handle, *params,
	//				frames, NULL);

	snd_pcm_hw_params_get_period_size(*params, &frames,
                                        NULL);
        printf("Actual frame size is =%d \n", (int)frames);
	


	/* write the parameters to driver */
	ret = snd_pcm_hw_params(*handle, *params);
	if (ret < 0) {
		fprintf(stderr,
			"unable to set hardware parameters: %s \n",
			snd_strerror(ret));
		return ret;
	}

//	snd_pcm_hw_params_free(*params);

	ret = snd_pcm_prepare(*handle);
	if (ret < 0) {
		fprintf(stderr,
			"Cannot prepare audio interface for capture/playback \n");
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

int voip_playback(snd_pcm_t *handle, snd_pcm_uframes_t frames, short buffer[])
{
	int ret;
	/* write frames in one period to device */
	while ((ret = snd_pcm_writei(handle, buffer, frames)) < 0) {
		if (ret == -EAGAIN)
			continue;
		else if (ret == -EPIPE) {
	        	/* EPIPE means underrun */
	        	fprintf(stderr, "underrun occurred \n");
	        	snd_pcm_prepare(handle);
		}
	}

	if (ret != (int)frames) {
	       	fprintf(stderr,
	               	"short write: written %d frames \n",
	               	ret);
	}

	return ret;

}

int voip_capture(snd_pcm_t *handle, snd_pcm_uframes_t frames, short buffer[])
{
        int ret;
        /* read frames in one period to device */
        while ((ret = snd_pcm_readi(handle, buffer, frames)) < 0) {
		if (ret == -EAGAIN)
			continue;
	        else if (ret == -EPIPE) {
	                /* EPIPE means underrun */
	                fprintf(stderr, "overrun occurred \n");
			snd_pcm_drop(handle);
	                snd_pcm_prepare(handle);
	        }
	}
	if (ret != (int)frames) {
		fprintf(stderr,
                        "short read: read %d frames \n",
                        ret);
        }
        return ret;
}


void voip_end_pcm(snd_pcm_t *handle)
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

