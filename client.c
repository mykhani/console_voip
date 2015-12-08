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
#define BUFLEN 512 /* Max length of buffer, for control data */

#ifdef DEBUG
#define dbg(x) fprintf(stdout, x ":%s:%d \n", __FILE__, __LINE__)
#else
#define dbg(x) {} 
#endif

void *send_audio(void *arg)
{
        /* send data over the socket */
        pthread_exit(NULL);
}

void *receive_audio(void *data)
{
	struct sockaddr_in other = *((struct sockaddr_in *)data);
	char buffer[BUFLEN];
	int sockfd; /* Descriptor for UDP socket */
	/* receive data from socket, add to buffer */
	int addrlen = sizeof other;
	int rc;
	
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
		dbg("Audio reception started");
		break;
	}

rcv_sock_close:
	close(sockfd);	

rcv_audio_end:
        pthread_exit(NULL);
}

void *play_audio(void *arg)
{
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
	
	dbg("Creating control socket");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Unable to create socket\n");
		return EXIT_FAILURE;
	}
	
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
	
	/* Instantiate a connection_handler thread to
	 * handle call */
	pthread_create(&call, NULL, connection_handler, &sockfd);
	/* wait for call to finish before accepting another call */
	printf("Call in progress\n");
	pthread_join(call, NULL);

	/* TODO: add duration of call */
	printf("Call ended \n");

	return EXIT_SUCCESS;
}
