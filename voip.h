#ifndef __voip_h__
#define __voip_h__

#include <alsa/asoundlib.h>

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

int voip_init_pcm(snd_pcm_t **handle, snd_pcm_hw_params_t **params,
		  snd_pcm_uframes_t *frames, unsigned int *rate);

char *voip_alloc_buf(snd_pcm_hw_params_t *params,
			snd_pcm_uframes_t *frames, int *size);

int voip_playback(snd_pcm_t *handle, snd_pcm_uframes_t *frames,
		 char *buffer);

void voip_end_pcm(snd_pcm_t *handle);

#endif
