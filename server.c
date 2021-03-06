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
#define TX_UDP_PORT 8889
#define RX_UDP_PORT 8890

#define BUFLEN 512 /* Max length of buffer, for control data */

/* We want to send 100ms of audio data in each 
 * UDP transfer. For 8000 sampling rate, 100ms of data means
 * 0.100 * 8000 ~ 800 samples */
#define SAMPLES_PER_PERIOD 800 /* frames per perod */
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

int verify_udp_connection(struct connection_data *_conn, int role)
{
        /* send data over the UDP socket */
	struct connection_data conn = *_conn;
	struct sockaddr_in own = conn.own;	
	struct sockaddr_in other = conn.other;	
	char buffer[512];
	int udp_sock_rx = conn.udp_sock_rx; /* Descriptor for UDP socket */
	int udp_sock_tx = conn.udp_sock_tx;

	/* receive data from socket, add to buffer */
	int addrlen = sizeof other;
	int rc;

	if ( role == UDP_RX) {
        dbg("Verifying RX UDP connection");
		addrlen = sizeof other;
		other.sin_port = htons(8889);
	
		/* sending init message to other end */
		sprintf(buffer, "INIT");
		if ((rc = sendto(udp_sock_rx, buffer, sizeof buffer, 0,
                        (struct sockaddr *)&other, addrlen)) < 0) {
			fprintf(stderr,
				"Unable to send data to other side \n");
			return rc;
		}

		/* Wait for ACK from other side.
	 	 * This is to verify that UDP connection is successful */
		dbg("Waiting for ACK from other side");
		memset(buffer, 0, sizeof buffer);
		if ((rc = recvfrom(udp_sock_rx, buffer, sizeof buffer, 0, 
				(struct sockaddr *)&other, &addrlen)) < 0) {
			fprintf(stderr,
				"Unable to receive data \n");
			return rc;
		} 
		printf("Received from other side: %s \n", buffer);
		printf("Total bytes =  %d \n", strlen(buffer));
		
		if (!strncmp("ACK", buffer, strlen(buffer))) {
			printf("ACK received from other side \n");
		} else {
			fprintf(stderr, "Unknown request recevied \n");
			return -EINVAL;
		}
	} else if (role == UDP_TX) {
		dbg("Verifying TX UDP connection");
		memset(buffer, 0, sizeof buffer);
		if ((rc = recvfrom(udp_sock_tx, buffer, sizeof buffer, 0, 
				(struct sockaddr *)&other, &addrlen)) < 0) {
			fprintf(stderr,
					"Unable to receive data \n");
			return rc;
		} 
		printf("Received from other side: %s \n", buffer);
		printf("Total bytes =  %d \n", strlen(buffer));
		
		if (!strncmp("INIT", buffer, strlen(buffer))) {
			printf("ACK received from other side \n");
		} else {
			fprintf(stderr, "Unknown request recevied \n");
			return -EINVAL;
		}
		sprintf(buffer, "ACK");
		if ((rc = sendto(udp_sock_tx, buffer, sizeof buffer, 0,
                        (struct sockaddr *)&other, addrlen)) < 0) {
			fprintf(stderr,
					"Unable to send data to other side \n");
					return rc;
		}
	}

	return 0;
}

/* TODO: Move UDP socket creation to connection handler thread
 * Faced a bug when capture thread executed before send_audio
 * thread had established UDP socket and as a result, server
 * lost AUDIO_RQST from client 
 */
int send_audio(struct connection_data *_conn)
{
    /* send data over the UDP socket */
	struct connection_data conn = *_conn;
	int sockfd = conn.udp_sock_rx;
	struct sockaddr_in other, own;
	short audio_samples[SPEEX_FRAME_SIZE];
	char buffer[512];
	char compressed[SPEEX_FRAME_SIZE];
	int addrlen;
	int rc;
	int i;

	/* variables for speex */
        SpeexBits bits;
        void *state; /* For holding encoder state */
        int quality;
	int nbytes, count;

        dbg("Preparing speex for compression");

        state = speex_encoder_init(&speex_nb_mode);
        /* Set the quality to 8 (15 kbps) */
        quality = 8;
        speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality);

        speex_bits_init(&bits);
	
	dbg("Preparing to send audio over UDP socket");

	own = conn.own;
	other = conn.other;
	other.sin_port = htons(8889);

	addrlen = sizeof other;
		
		/* send data here */
		/* TODO: How to end the send? */
		//dbg("Sending audio data to other side \n");
		memset(compressed, 0, sizeof compressed);
		memset(buffer, 0, sizeof buffer);
                printf("Trying data send\n");
		/* Instead of multiple sends over socket, compress the complete audio buffer size
		 * of data, each encoding results in 38 bytes, so a 512 bytes transmit buffer
		 * can accomodate over 512 / 38 ~ 13 160 bytes speex frames. We are sending only
		 * 5 (i.e 800 / 160) speex frames for our audio buffer of 800 frames */
		for (i = 0, count = 0; i < 5; i++) {
                	/* if read is unsuccessful, wait for capture thread
                 	* to write audio data to sbuff */
                	rc = ring_read(sbuff, (char *)audio_samples, sizeof audio_samples);
			printf("Read %d bytes from tx circular buffer \n", rc);	
			speex_bits_reset(&bits);
			/* Encode here */
			speex_encode_int(state, audio_samples, &bits);
			/* Write the encoded bits to array of bytes so that they can be written */
			nbytes = speex_bits_write(&bits, compressed, sizeof compressed);

			memcpy(buffer + i * 38, compressed, nbytes);
			count += nbytes;
			//printf("Size of compressed data: %d \n", nbytes);
			//printf("Size of audio_samples: %d \n", sizeof audio_samples);
			//printf("Iteration no : %d \n", i + 1);
		}
		
		if ((rc =sendto(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&other, addrlen)) < 0) {
			fprintf(stderr,
		        	"Unable to send data to other side \n");
		} else 
                	printf("Data sent: %d bytes\n", rc);
		
	
	dbg("Freeing up speex resources");
	/* Destroy the encoder state */
        speex_encoder_destroy(state);
        /* Destroy the bits-packing */
        speex_bits_destroy(&bits);
	
#if 0	
send_sock_close:
	dbg("Closing UDP socket connection");
	/* stop reception and transmission */
        if (rc = shutdown(sockfd, 2)) {
                fprintf(stderr,
                        "Unable to end connection: %s \n",
                        strerror(errno));
        }
        /* close the socket so that it can be reused */
        close(sockfd);
	
audio_end:
	 
        pthread_exit(NULL);
#endif
}

void *capture_audio(void *data)
{
	/* This thread captures audio samples from audio device */
	/* TODO: add mechanism to end this thread when desired,
	 * perhaps by passing flags for indicating end of call? */
	struct connection_data conn = *((struct connection_data *)data);

	int ret;
	int rc;
	unsigned int rate = 8000;
        int size;
	short audio_samples[SAMPLES_PER_PERIOD];
        int fd;
	int frame_size = 2;

        snd_pcm_t *handle;
        snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
        int buffer_size = SAMPLES_PER_PERIOD * sizeof(short);

        printf("rate is =%d \n", rate);

        ret = voip_init_pcm(&handle, &params, &buffer_size, &rate, RECORD);
	if (ret) {
                fprintf(stderr, "Unable to initialize PCM \n");
                goto capture_failure;
        }
        printf("In record main \n");
        printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
        printf("Pointer to params=%p \n", params);

	printf("Size of buffer to accomodate period of data : %d \n", buffer_size);
	while(1) {
                rc = voip_capture(handle, buffer_size / frame_size, audio_samples);
#if 0	
		rc = read(fd, audio_samples, sizeof audio_samples);
	
		if (rc == 0) {
			printf("EOF reached \n");
			break;
		} else if (rc < 0) {
			fprintf(stderr, "Error reading file \n");
			break;
		}
#endif
	
		printf("Read %d frames from capture source \n", rc);
		ring_write(sbuff, (char *)audio_samples, sizeof audio_samples);
		rc = send_audio(&conn);

		if (rc < 0) {
			fprintf(stderr,
				"Failed to transmit audio: %s \n",
				strerror(rc));
			/* TODO: Gracefully handle error condition, close socket etc */
			break;		
		}
        }
	ret = rc;

capture_end:
        voip_end_pcm(handle);
capture_failure:
	pthread_exit(NULL);

}

void *receive_audio(void *data)
{
	struct connection_data conn = *((struct connection_data *)data);	
	struct sockaddr_in other = conn.other; 
	char buffer[512];
	char compressed[SPEEX_FRAME_SIZE];
	short audio_samples[SPEEX_FRAME_SIZE];	
	int sockfd = conn.udp_sock_tx; /* Descriptor for UDP socket */
	/* receive data from socket, add to buffer */
	int addrlen = sizeof other;
	int rc;
	/* variables for speex */
        SpeexBits bits;
        void *state; /* For holding encoder state */
        int tmp;
	int nbytes;
	int i;

	int ret;
        unsigned int rate = 8000;
        int size;
        short playback_samples[SAMPLES_PER_PERIOD];
        int fd;
        int frame_size = 2;

	snd_pcm_t *handle;
        snd_pcm_hw_params_t *params;
        /* write many data at a time to device */
        /* the value of 5512 comes from aplay, investigate
         * why it is so */
	int buffer_size = SAMPLES_PER_PERIOD * sizeof(short);

        printf("rate is =%d \n", rate);

	dbg("Preparing audio playback");
        ret = voip_init_pcm(&handle, &params, &buffer_size, &rate, PLAYBACK);
        
	if (ret < 0) {
		fprintf(stderr,
			"Failed to prepare audio system\n");
		goto rcv_audio_end;
	}

	printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
        printf("Pointer to params=%p \n", params);
	
	dbg("Preparing speex for de-compression");
        state = speex_decoder_init(&speex_nb_mode);
	tmp = 1;
        speex_decoder_ctl(state, SPEEX_SET_ENH, &tmp);
        speex_bits_init(&bits);

	while(1) {
		memset(audio_samples, 0, sizeof audio_samples);
		memset(compressed, 0, sizeof compressed);
		memset(buffer, 0, sizeof buffer);
		printf("Waiting for data\n");
		if ((rc = recvfrom(sockfd, buffer, sizeof buffer, 0,
				(struct sockaddr *)&other, &addrlen)) < 0) {
			fprintf(stderr,
				"Unable to receive audio data: %s \n",
				strerror(errno));
			goto rcv_sock_close;
		}
		printf("Received %d compressed bytes on UDP socket \n", rc);

		for (i = 0; i < 5; i++) {
			speex_bits_reset(&bits);
			/* each encoded speex frame takes 38 bytes */
			memcpy(compressed, buffer + i * 38, 38);

                	speex_bits_read_from(&bits, compressed, 38);
		
			/* Decode here */
                	speex_decode_int(state, &bits, audio_samples);	
		
			ring_write(rbuff, (char *)audio_samples, sizeof audio_samples);

		}
		ret = ring_read(rbuff, (char *)audio_samples, buffer_size);
		if ( ret != buffer_size) {
                        fprintf(stderr,
                                "short read: read %d bytes \n", ret);
                }
                printf("Playing audio \n");
                /* write frames in one period to device */
                ret = voip_playback(handle, buffer_size / frame_size, audio_samples);
	}

        voip_end_pcm(handle);
	/* Destroy the encoder state */
        speex_decoder_destroy(state);
        /* Destroy the bits-packing */
        speex_bits_destroy(&bits);

rcv_sock_close:
	close(sockfd);	

rcv_audio_end:
        pthread_exit(NULL);
}

/* handler for maintaining call */
void *connection_handler(void *data)
{
	pthread_t sender, receiver, playback, capture;
	struct connection_data conn = *((struct connection_data *)data);
	struct sockaddr_in own = conn.own;
	struct sockaddr_in other = conn.other;
	int sockfd = conn.control_sock;
	int udp_sock_rx;
	int udp_sock_tx;

	char ipstr[INET_ADDRSTRLEN]; /* holds the client ip address */
	char buffer[BUFLEN];
	char c;
	int rc;
	/* instantiate sender thread */
        dbg("Creating a sender thread");

	/* Convert binary address (in network byte order) to ascii string */
	inet_ntop(AF_INET, &other.sin_addr, ipstr, sizeof ipstr);
	
	printf("%s is calling \n", ipstr);
	printf("Accept call? y/n : default [y]");
	
	c = getc(stdin);
	if (c == 'n' || c == 'N') {
        	/* reject the call */
        	/* stop reception and transmission */
        	printf("Rejecting call from %s \n",
                	ipstr);
		memset(buffer, 0, sizeof(buffer));
                sprintf(buffer, "NO");
                rc = write(sockfd, buffer, sizeof(buffer));
                if (rc < 0) {
                        fprintf(stderr, "Write to socket failed \n");
                }

		goto end;
	}

	memset(buffer, 0, sizeof(buffer));
	dbg("Waiting to receive handshake msg from client");
	rc = read(sockfd, buffer, sizeof(buffer));
	if (rc < 0) {
		fprintf(stderr, "Failed to read socket \n");
		goto end;
	} else if (rc > 0) {
		printf("Msg from client : %s \n", buffer);
		/* add a check on msg and send response */
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "OK");
		rc = write(sockfd, buffer, sizeof(buffer));
		if (rc < 0) {
			fprintf(stderr, "Write to socket failed \n");
			goto end;
		}
	
	} else {
		/* read returns 0 meaning end of connection */
		fprintf(stderr, "Connection terminated with client \n");
		goto end;
	}

	if ((udp_sock_tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                fprintf(stderr, "Unable to create TX UDP socket: %s \n", strerror(errno));
                goto end;
        }

        if (bind(udp_sock_tx, (struct sockaddr *)&own, sizeof own) < 0) {
                fprintf(stderr, "Unable to bind TX UDP socket: %s \n", strerror(errno));
                goto end;
        }

	if ((udp_sock_rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                fprintf(stderr, "Unable to create RX UDP socket: %s \n", strerror(errno));
                goto end;
        }

	conn.udp_sock_tx = udp_sock_tx;
	conn.udp_sock_rx = udp_sock_rx;

	rc = verify_udp_connection(&conn, UDP_TX);

        if (rc < 0) {
                fprintf(stderr,
                        "Failed to verify Tx UDP connection\n");
                goto end;
        }

	rc = verify_udp_connection(&conn, UDP_RX);

        if (rc < 0) {
                fprintf(stderr,
                        "Failed to verify Rx UDP connection\n");
                goto end;
        }

	/* instantiate audio capture thread */
        dbg("Creating a capture thread");

	if (rc = pthread_create(&capture, NULL, capture_audio, &conn)) {
                fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
                goto end;
        }
        /* instantiate receiver thread */
	dbg("Creating a receiver thread");
	
	if (rc = pthread_create(&receiver, NULL, receive_audio, &conn)) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto end;
	}
	dbg("Connection_handler waiting");
        /* wait for threads to finish their jobs */
	pthread_join(capture, NULL);
        pthread_join(receiver, NULL);
	
	dbg("Exiting connection handler");
	
end:
	/* stop reception and transmission */
	if (rc = shutdown(sockfd, 2)) {
		fprintf(stderr,
			"Unable to end connection: %s \n",
			strerror(errno));
	}
        /* close the socket so that it can be reused */
        close(sockfd);
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

	struct sockaddr_in own, other;
	int addrlen;	
	int sockfd, newsockfd;
	struct connection_data conn;
	int rc;
	/* Threads for sending, receiving and playback */
	pthread_t call;

	dbg("Creating RX ring buffer");
	/* allocate enough space to store 512 periods or 2.56s of audio data
	 * as each period is of 5ms size */
	if (ring_alloc(&rbuff, SAMPLES_PER_PERIOD *4 * 512)) {
		fprintf(stderr, "Unable to allocate RX ring: %s \n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	ring_init(rbuff);

	printf("Created RX ring of size %d \n", ring_size(rbuff));

	dbg("Creating TX ring buffer");
        if (ring_alloc(&sbuff, SAMPLES_PER_PERIOD *4 * 512)) {
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

	/* populate sockaddr_in structure for server */
	own.sin_family = AF_INET;
	own.sin_addr.s_addr = INADDR_ANY;
	/* convert from host to network byte order */
	/* htons : host to network short */
	own.sin_port = htons(PORT);

	dbg("Binding the control socket");

	if (bind(sockfd, (struct sockaddr *)&own, sizeof(own))) {
		fprintf(stderr, "Unable to bind control socket \n");
		return EXIT_FAILURE;
	}

	dbg("Control socket binded");
	/* Start listening for incoming connections */
	dbg("Listening for incoming connections");
	listen(sockfd, 1); /* Allow only single connection */
	
	addrlen = sizeof(other);
	
	printf("Waiting for connection \n");
	
	while ((newsockfd = accept(sockfd, (struct sockaddr *)&other, &addrlen))) {
		/* Instantiate a connection_handler thread to
		 * handle call */
		conn.own = own;
		conn.other = other;
		conn.control_sock = newsockfd;
		
		pthread_create(&call, NULL, connection_handler, &conn);
		/* wait for call to finish before accepting another call */
		printf("Call in progress\n");
		pthread_join(call, NULL);

		/* TODO: add duration of call */
		printf("Call ended \n");
		printf("Waiting for connection \n");
	}

	if (newsockfd < 0) {
		fprintf(stderr, "Accept failed \n");
		exit(1);
	}

	return EXIT_SUCCESS;
}
