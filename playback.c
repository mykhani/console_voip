/*
 * Read an audio file and play it back
 */

#include <alsa/asoundlib.h>

#define FRAMES_PER_PERIOD 128

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

int main(int argc, char *argv[])
{
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;

	int ret;
	unsigned int rate;
	int size;
	int dir;
	char *buffer;

	/* open PCM device for playback */
	ret = snd_pcm_open(&handle, "plughw",
			SND_PCM_STREAM_PLAYBACK, 0 );
	if (ret < 0) {
		fprintf(stderr,
			"unable to open pcm device: %s\n",
			snd_strerror(ret));
		exit(1);
	}
	
	/* allocate hardware parameters object */
	snd_pcm_hw_params_alloca(&params);

	/* fill it in with default values */
	snd_pcm_hw_params_any(handle, params);

	/* set the desired hardware parameters */
	
	/* interleaved mode */
	snd_pcm_hw_params_set_access(handle, params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
		
	/* signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params,
			SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(handle, params, 2);

	/* 44100 bits/second sampling rate (cd quality) */
	rate = 8000;
	snd_pcm_hw_params_set_rate_near(handle, params,
					&rate, &dir);

	/* set period size to FRAMES_PER_PERIOD frames */
	frames = FRAMES_PER_PERIOD;
	snd_pcm_hw_params_set_period_size_near(handle, params,
					&frames, &dir);

	/* write the parameters to driver */
	ret = snd_pcm_hw_params(handle, params);
	if (ret < 0) {
		fprintf(stderr,
			"unable to set hardware parameters: %s \n",
			snd_strerror(ret));
		exit(1);
	}

	/* use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(params, &frames,
					&dir);
	
	size = frames * 4; /* 2 bytes/sample, 2 channels */
	buffer = malloc(size * sizeof(char));

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
		ret = snd_pcm_writei(handle, buffer, frames);
		
		if (ret == -EPIPE) {
			/* EPIPE means underrun */
			fprintf(stderr, "underrun occurred \n");
			snd_pcm_prepare(handle);
		} else if (ret < 0) {
			fprintf(stderr,
				"error from writei: %s \n",
				snd_strerror(ret));
		} else if (ret != (int)frames) {
			fprintf(stderr,
				"short write: written %d frames \n",
				ret);
		}
	}
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	return 0;			
}

	
