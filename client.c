#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "voip.h"
#include "ring.h"
#include <speex/speex.h>

/* Enable debugging */
#define DEBUG

#define PORT 8888
#define BUFLEN 512 /* Max length of buffer, for control data */

#define SAMPLES_PER_PERIOD 800
#define SPEEX_FRAME_SIZE 160

#ifdef DEBUG
#define dbg(x) fprintf(stdout, x ":%s:%d \n", __FILE__, __LINE__)
#else
#define dbg(x) {} 
#endif

/* declare a ring buffer for RX i.e. rbuff
 * receive_audio thread reads socket and write to rbuff
 * play_audio thread reads rbuff and passes data to audio device
 */
struct ring *rbuff;

/* declare a ring buffer for TX i.e. sbuff
 * capture_audio thread reads audio device and write to sbuff
 * send_audio thread reads sbuff and write data to UDP socket
 */
struct ring *sbuff;

/* pthread variable relevant for TX */
pthread_mutex_t tx_lock; // synchronize access to TX ring
pthread_cond_t capture_done, transmit_done;

/* pthread variable relevant for RX */
pthread_mutex_t rx_lock; // synchronize access to RX ring
pthread_cond_t receive_done, playback_done;


void *send_audio(void *arg)
{
        /* send data over the socket */
        pthread_exit(NULL);
}

void *receive_audio(void *data)
{
	struct sockaddr_in other = *((struct sockaddr_in *)data);
	char buffer[512];
	char compressed[SPEEX_FRAME_SIZE];
	short audio_samples[SPEEX_FRAME_SIZE];	
	int sockfd; /* Descriptor for UDP socket */
	/* receive data from socket, add to buffer */
	int addrlen = sizeof other;
	int rc;
	/* variables for speex */
        SpeexBits bits;
        void *state; /* For holding encoder state */
        int tmp;
	int nbytes;

        dbg("Preparing speex for de-compression");
        state = speex_decoder_init(&speex_nb_mode);
	
	tmp = 1;
        speex_decoder_ctl(state, SPEEX_SET_ENH, &tmp);

        speex_bits_init(&bits);
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Unable to create UDP socket \n");
		goto rcv_audio_end;
	} 

	/* Check UDP connection state by sending AUDIO_RQST to other side */
	dbg("Checking UDP connection status");
	memset(buffer, 0, sizeof buffer);
	sprintf(buffer, "AUDIO_RQST");
	if ((sendto(sockfd, buffer, sizeof buffer, 0,
		(struct sockaddr *)&other, addrlen)) < 0) {
		fprintf(stderr, "Unable to send AUDIO_RQST \n");
		goto rcv_sock_close;
	} else {
		dbg("AUDIO_RQST sent, waiting for response");
		memset(buffer, 0, sizeof buffer);
		if ( recvfrom(sockfd, buffer, sizeof buffer, 0,
				(struct sockaddr *)&other, &addrlen) < 0) {
			fprintf(stderr,
				"Unable to receive response: %s \n", 
				strerror(errno));
                	goto rcv_sock_close;
		} else {
			printf("Received %s from other side \n", buffer);
			if (!strncmp("ACK", buffer, strlen(buffer))) {
				dbg("ACK received from other side");
			} else {
				fprintf(stderr, "Unknown response \n");
				goto rcv_sock_close;
			}
		}
	}
	while(1) {
		memset(audio_samples, 0, sizeof audio_samples);
		memset(compressed, 0, sizeof compressed);
		if ((rc = recvfrom(sockfd, compressed, sizeof compressed, 0,
				(struct sockaddr *)&other, &addrlen)) < 0) {
			fprintf(stderr,
				"Unable to receive audio data: %s \n",
				strerror(errno));
			goto rcv_sock_close;
		}
		//printf("Received %d compressed bytes on UDP socket \n", rc);

		speex_bits_reset(&bits);

                speex_bits_read_from(&bits, compressed, rc);
		
		/* Decode here */
                speex_decode_int(state, &bits, audio_samples);	
		
		pthread_mutex_lock(&rx_lock);
		while (ring_write(rbuff, (char *)audio_samples, sizeof audio_samples) < 0 ) {
			pthread_cond_wait(&playback_done, &rx_lock);
		}

		/* signal the reception of data for playback thread */
		pthread_cond_signal(&receive_done);
		pthread_mutex_unlock(&rx_lock);
	}

	/* Destroy the encoder state */
        speex_decoder_destroy(state);
        /* Destroy the bits-packing */
        speex_bits_destroy(&bits);

rcv_sock_close:
	close(sockfd);	

rcv_audio_end:
        pthread_exit(NULL);
}

void *play_audio(void *arg)
{
	int ret;
        unsigned int rate = 8000;
        int size;
	short audio_samples[SAMPLES_PER_PERIOD];
	int fd;
	int frame_size = 2;

        snd_pcm_t *handle;
        snd_pcm_hw_params_t *params;
	/* write many data at a time to device */
	/* the value of 5512 comes from aplay, investigate
	 * why it is so */
	int buffer_size = SAMPLES_PER_PERIOD * sizeof(short);

        printf("rate is =%d \n", rate);

        voip_init_pcm(&handle, &params, &buffer_size, &rate, PLAYBACK);
        printf("In playback main \n");
        printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
        printf("Pointer to params=%p \n", params);

        while(1) {

		pthread_mutex_lock(&rx_lock);
		while ((ret = ring_read(rbuff, (char *)audio_samples, buffer_size)) < 0) {
			pthread_cond_wait(&receive_done, &rx_lock);

		}

		/* signal the read of audio sample by playback thread */
		pthread_cond_signal(&playback_done);
		pthread_mutex_unlock(&rx_lock);

                if ( ret != buffer_size) {
                        fprintf(stderr,
                                "short read: read %d bytes \n", ret);
                }
		printf("Playing audio \n");
                /* write frames in one period to device */
                ret = voip_playback(handle, buffer_size / frame_size, audio_samples);

        }

        ret = 0;

failure:
        voip_end_pcm(handle);

	/* Send the data from buffer to audio device */
	
        pthread_exit(NULL);
}

/* handler for maintaining call */
void *connection_handler(void *data)
{
	struct connection_data conn = *((struct connection_data *)data);
	struct sockaddr_in other = conn.other;
	int sockfd = conn.sockfd;
	int rc;
	pthread_t sender, receiver, playback;

	/* instantiate sender thread */
        dbg("Creating a sender thread");

        if (rc = pthread_create(&sender, NULL, send_audio, NULL)) {
                fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
                goto failure;
        }

        /* instantiate receiver thread */
	dbg("Creating a receiver thread");
	
	if (rc = pthread_create(&receiver, NULL, receive_audio, &other)) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto failure;
	}
	
	/* instantiate playback thread */
	dbg("Creating a playback thread");
	
	if (rc = pthread_create(&playback, NULL, play_audio, NULL)) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto failure;
	}
	
	dbg("Connection_handler waiting");
        /* wait for threads to finish their jobs */
        pthread_join(sender, NULL);
        pthread_join(receiver, NULL);
        pthread_join(playback, NULL);
	
	dbg("Exiting connection handler");
	
	pthread_exit(NULL);
	
failure:
	/* TODO: handle the failure condition */
	/* Make sure end all the threads */
	pthread_exit(NULL);
	
}

int main (int argc, char *argv[])
{
	/* 
	 * The protocol is double connection:
	 * Control connection: TCP based connection for establishing calls
	 * Data connection: UDP based connection for audio data transfer
	 * One UDP socket to send audio data
	 * Other UDP socket to receive audio data
	 */
	
	/* TODO: Add a menu in the main function
	 * Based on the number input by user, jump to different state
	 * The code for establishing control socket must be in a separate
	 * function which is called based on the user input */

	struct sockaddr_in server;
	int sockfd;
	char *server_ip;
	char buffer[BUFLEN];
	struct connection_data conn;
	/* Threads for sending, receiving and playback */
	pthread_t call;

	int rc;
	
	if (argc < 2) {
		fprintf(stderr, "Invalid arguments \n");
		printf("Usage: control server_ip\n");
	}
	
	server_ip = argv[1];
	
	dbg("Creating RX ring buffer");
	if (ring_alloc(&rbuff, SAMPLES_PER_PERIOD * 4 * 512)) {
		fprintf(stderr, "Unable to allocate RX ring: %s \n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	ring_init(rbuff);

	printf("Created RX ring of size %d \n", ring_size(rbuff));

	dbg("Creating TX ring buffer");
        if (ring_alloc(&sbuff, SAMPLES_PER_PERIOD * 4 * 512)) {
                fprintf(stderr, "Unable to allocated TX ring: %s \n",
                        strerror(errno));
                return EXIT_FAILURE;
        }
        ring_init(sbuff);
	printf("Created TX ring of size %d \n", ring_size(sbuff));

	dbg("Creating control socket");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Unable to create socket\n");
		return EXIT_FAILURE;
	}

	pthread_mutex_init(&rx_lock, NULL);
	pthread_mutex_init(&tx_lock, NULL);

	dbg("Control socket created");
	
	/* zeroize sockaddr_in structure for server*/
	memset((char *)&server, 0, sizeof server);
	/* populate sockaddr_in structure for server */
	server.sin_family = AF_INET;
	/* convert from host to network byte order */
	/* htons : host to network short */
	server.sin_port = htons(PORT);
	/* set server ip address */	
	if (inet_aton(server_ip, &server.sin_addr) == 0) {
		fprintf(stderr, "inet_aton() failed \n");
		close(sockfd);
		return EXIT_FAILURE;
	}
	
	conn.other = server;
	
	dbg("Trying to connect to server");
	printf("Trying to connect %s \n", server_ip);
	if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		fprintf(stderr, "Unable to connect to server: %s \n", strerror(errno));
		close(sockfd);
		return EXIT_FAILURE;
	}
	
	printf("Connection established with server %s\n", server_ip);
	conn.sockfd = sockfd;
	dbg("Trying to make call");

	memset(buffer, 0, sizeof buffer);
	sprintf(buffer, "CALL");
	rc = write(sockfd, buffer, strlen(buffer));
	if (rc < 0) {
		fprintf(stderr, "Unable to write to socket \n");
		close(sockfd);
		return EXIT_FAILURE;
	}
	memset(buffer, 0, sizeof buffer);
	rc = read(sockfd, buffer, sizeof buffer);
	if (rc < 0) {
		fprintf(stderr, "Error reading from socket \n");
		close(sockfd);
		return EXIT_FAILURE;
	}

	printf("The return code: %s \n", buffer);

	if (!strncmp("OK", buffer, strlen(buffer))) {
		/* Other side returned OK */
		dbg("Call established");
		/* Instantiate a connection_handler thread to
         	 * handle call */
        	pthread_create(&call, NULL, connection_handler, &conn);
	}
	
	printf("Call in progress\n");
	pthread_join(call, NULL);

	/* TODO: add duration of call */
	printf("Call ended \n");

	return EXIT_SUCCESS;
}
