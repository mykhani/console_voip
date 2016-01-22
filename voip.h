#ifndef __voip_h__
#define __voip_h__

#include <alsa/asoundlib.h>
#include <arpa/inet.h>

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

enum {
	UDP_TX,
	UDP_RX
};

enum {
	PLAYBACK,
	RECORD
};

struct connection_data {
	struct sockaddr_in own;
	struct sockaddr_in other;
	int control_sock;
	int udp_sock_tx;
	int udp_sock_rx;
};

int voip_init_pcm(snd_pcm_t **handle, snd_pcm_hw_params_t **params,
		  int *buffer_size, unsigned int *rate, int mode);

char *voip_alloc_buf(snd_pcm_hw_params_t *params,
			snd_pcm_uframes_t *frames, int *size);

int voip_playback(snd_pcm_t *handle, snd_pcm_uframes_t frames,
		 short buffer[]);

int voip_capture(snd_pcm_t *handle, snd_pcm_uframes_t frames,
		 short buffer[]);

void voip_end_pcm(snd_pcm_t *handle);

#endif
