#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "voip.h"

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
		dbg("Sending audio data to other side \n");
		memset(buffer, 0, sizeof buffer);
		if (sendto(sockfd, buffer, sizeof buffer, 0, 
			(struct sockaddr *)&other, addrlen) < 0) {
			fprintf(stderr,
				"Unable to send data to other side \n");
			break;
		}
		break;
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

void *receive_audio(void *arg)
{
	/* receive data from socket, add to buffer */
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

	fd = open("/home/ykhan/dev/c_learning/voip_console/audio_samples/test.wav", 'r');
	if (!fd) {
		fprintf(stderr, "Unable to open audio file \n");
		goto failure;
	}

        while(1) {
                ret = read(fd, buffer, size); /* read from stdin */
                if (ret == 0) {
                        fprintf(stderr, "end of file on input \n");
                        break;
                } else if ( ret != size) {
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
	pthread_t sender, receiver, playback;
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

        /* instantiate receiver thread */
	dbg("Creating a receiver thread");
	
	if (rc = pthread_create(&receiver, NULL, receive_audio, NULL)) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto end;
	}
	
	/* instantiate playback thread */
	dbg("Creating a playback thread");
	
	if (rc = pthread_create(&playback, NULL, play_audio, NULL )) {
		fprintf(stderr, "error: pthread create failed, rc = %d \n", rc );
		goto end;
	}
	
	dbg("Connection_handler waiting");
        /* wait for threads to finish their jobs */
        pthread_join(sender, NULL);
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
	/* Threads for sending, receiving and playback */
	pthread_t call;

	int rc;

	dbg("Creating control socket");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Unable to create socket\n");
		return EXIT_FAILURE;
	}
	
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
