#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "voip.h"
#include "ring.h"

/* Enable debugging */
#define DEBUG

#define PORT 8888
#define BUFLEN 512 /* Max length of buffer */

#define SAMPLES_PER_PERIOD 128

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

/* TODO: Move UDP socket creation to connection handler thread
 * Faced a bug when capture thread executed before send_audio
 * thread had established UDP socket and as a result, server
 * lost AUDIO_RQST from client 
 */
void *send_audio(void *data)
{
        /* send data over the UDP socket */
	struct connection_data conn = *((struct connection_data *)data);
	int sockfd;
	struct sockaddr_in other, own;
	char buffer[BUFLEN];
	int addrlen;
	int rc;
	
	dbg("Preparing to send audio over UDP socket");

	own = conn.own;
	other = conn.other;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Unable to create socket: %s \n", strerror(errno));
		goto audio_end;
	}

	if (bind(sockfd, (struct sockaddr *)&own, sizeof own) < 0) {
		fprintf(stderr, "Unable to bind socket: %s \n", strerror(errno));
                goto audio_end;	
	}
	
	addrlen = sizeof other;
	
	/* Wait for audio request from other side.
	 * This is to verify that UDP connection is successful */
	dbg("Waiting for audio request from other side");
	memset(buffer, 0, sizeof buffer);
	if ((rc = recvfrom(sockfd, buffer, sizeof buffer, 0, 
			(struct sockaddr *)&other, &addrlen)) < 0) {
		fprintf(stderr,
			"Unable to receive data \n");
		goto send_sock_close;
	} 
	printf("Received from other side: %s \n", buffer);
	printf("Total bytes =  %d \n", strlen(buffer));
	if (!strncmp("AUDIO_RQST", buffer, strlen(buffer))) {
		printf("other side requesting data \n");
		memset(buffer, 0, sizeof buffer);
		sprintf(buffer, "ACK");
		if (sendto(sockfd, buffer, sizeof buffer, 0,
                        (struct sockaddr *)&other, addrlen) < 0) {
			fprintf(stderr,
                                "Unable to send data to other side \n");
                        goto send_sock_close;
		}
		
	} else {
		fprintf(stderr, "Unknown request recevied \n");
		goto send_sock_close;
	}

	while(1) {
		/* send data here */
		/* TODO: How to end the send? */
		dbg("Sending audio data to other side \n");
		memset(buffer, 0, sizeof buffer);
		
		pthread_mutex_lock(&tx_lock);
		/* if read is unsuccessful, wait for capture thread
		 * to write audio data to sbuff */
		while (ring_read(sbuff, buffer, sizeof buffer) < 0) {
			pthread_cond_wait(&capture_done,&tx_lock); 
		}
		/* signal the read complete, capture thread maybe waiting
		 * on it */
		pthread_cond_signal(&transmit_done);
		pthread_mutex_unlock(&tx_lock);

		if (sendto(sockfd, buffer, sizeof buffer, 0, 
			(struct sockaddr *)&other, addrlen) < 0) {
			fprintf(stderr,
				"Unable to send data to other side \n");
			break;
		}
	}
	
	
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
}

void *capture_audio(void *data)
{
	/* This thread captures audio samples from audio device */
	/* A placeholder code, atm reading from file to simulate
	 * captured data */
	int fd;
	int rc;
	char buffer[512];
	
	fd = open("/home/ykhan/dev/c_learning/voip_console/audio_samples/test.wav", 'r');
	
	if (fd < 0) {
		fprintf(stderr, "Unable to open audio file \n");
		goto capture_end;
	}

	while (1) {
		rc = read(fd, buffer, 512);
		if (rc < 0) {
			fprintf(stderr, "Error reading file \n");
			close(fd);
			goto capture_end;
		} else if (rc == 0) {
                        printf("EOF reached \n");
                        close(fd);
                        break;
                } else if (rc !=512) {

			printf("Short read, read %d bytes \n", rc);
		}

		pthread_mutex_lock(&tx_lock);
		while(ring_write(sbuff, buffer, rc) < 0) {
			pthread_cond_wait(&transmit_done, &tx_lock);
		}
		pthread_cond_signal(&capture_done);
		pthread_mutex_unlock(&tx_lock);
			
	}

capture_end:
	pthread_exit(NULL);

}

void *receive_audio(void *data)
{
	/* receive data from socket, add to buffer */
	struct connection_data conn = *((struct connection_data *)data);
        int sockfd;
        struct sockaddr_in other, own;
        char buffer[BUFLEN];
        int addrlen;
        int rc;

        dbg("Preparing to send audio over UDP socket");

        own = conn.own;
        other = conn.other;

        if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                fprintf(stderr, "Unable to create socket: %s \n", strerror(errno));
                goto rcv_audio_end;
        }

        if (bind(sockfd, (struct sockaddr *)&own, sizeof own) < 0) {
                fprintf(stderr, "Unable to bind socket: %s \n", strerror(errno));
                goto rcv_audio_end;
        }

        addrlen = sizeof other;

        /* Wait for audio request from other side.
         * This is to verify that UDP connection is successful */

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
                dbg("Audio reception started");
		memset(buffer, 0, sizeof buffer);
		if ( recvfrom(sockfd, buffer, sizeof buffer, 0,
				(struct sockaddr *)&other, &addrlen) < 0) {
			fprintf(stderr,
				"Unable to receive audio data: %s \n",
				strerror(errno));
			goto rcv_sock_close;
		}
		pthread_mutex_lock(&rx_lock);
		while (ring_write(rbuff, buffer, sizeof buffer) < 0 ) {
			pthread_cond_wait(&playback_done, &rx_lock);
		}
		/* signal the reception of data for playback thread */
		pthread_cond_signal(&receive_done);
		pthread_mutex_unlock(&rx_lock);
		
	}

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
        char *buffer;
	int fd;

        snd_pcm_t *handle;
        snd_pcm_hw_params_t *params;
        snd_pcm_uframes_t frames = SAMPLES_PER_PERIOD;

        printf("rate is =%d \n", rate);

        voip_init_pcm(&handle, &params, &frames, &rate);
        printf("In playback main \n");
        printf("Pointer address to handle=%p \n", &handle);
        printf("Pointer to handle=%p \n", handle);
        printf("Pointer to params=%p \n", params);
        buffer = voip_alloc_buf(params, &frames, &size);

	if (!buffer) {
                fprintf(stderr, "Unable to allocate buffer: %s \n", strerror(errno));
                ret = -ENOMEM;
                goto failure;
        }

        while(1) {

		pthread_mutex_lock(&rx_lock);
		while ((ret = ring_read(rbuff, buffer, size)) < 0) {
			pthread_cond_wait(&receive_done, &rx_lock);

		}

		/* signal the read of audio sample by playback thread */
		pthread_cond_signal(&playback_done);
		pthread_mutex_unlock(&rx_lock);

                if ( ret != size) {
                        fprintf(stderr,
                                "short read: read %d bytes \n", ret);
                }
                /* write frames in one period to device */
                ret = voip_playback(handle, &frames, buffer);

        }

        ret = 0;

failure:
        voip_end_pcm(handle);
        free(buffer);

	/* Send the data from buffer to audio device */
	
        pthread_exit(NULL);
}

/* handler for maintaining call */
void *connection_handler(void *data)
{
	pthread_t sender, receiver, playback, capture;
	struct connection_data conn = *((struct connection_data *)data);
	struct sockaddr_in other = conn.other;
	int sockfd = conn.sockfd;

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

        if (rc = pthread_create(&sender, NULL, send_audio, &conn)) {
                fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
                goto end;
        }

	/* instantiate audio capture thread */
        dbg("Creating a capture thread");

	if (rc = pthread_create(&capture, NULL, capture_audio, NULL)) {
                fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
                goto end;
        }
#if 0
        /* instantiate receiver thread */
	dbg("Creating a receiver thread");
	
	if (rc = pthread_create(&receiver, NULL, receive_audio, &conn)) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto end;
	}
	
	/* instantiate playback thread */
	dbg("Creating a playback thread");
	
	if (rc = pthread_create(&playback, NULL, play_audio, NULL )) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto end;
	}
#endif
	dbg("Connection_handler waiting");
        /* wait for threads to finish their jobs */
        pthread_join(sender, NULL);
        pthread_join(capture, NULL);
        pthread_join(receiver, NULL);
        pthread_join(playback, NULL);
	
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
	if (ring_alloc(&rbuff, 512 * 512)) {
		fprintf(stderr, "Unable to allocate RX ring: %s \n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	ring_init(rbuff);

	printf("Created RX ring of size %d \n", ring_size(rbuff));

	dbg("Creating TX ring buffer");
        if (ring_alloc(&sbuff, 512 * 512)) {
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
		conn.sockfd = newsockfd;
		
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
