#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Enable debugging */
#define DEBUG

#define PORT 8888
#define BUFLEN 512 /* Max length of buffer */

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

void *receive_audio(void *arg)
{
	/* receive data from socket, add to buffer */
        pthread_exit(NULL);
}

void *play_audio(void *arg)
{
	/* Send the data from buffer to audio device */
        pthread_exit(NULL);
}

/* handler for maintaining call */
void *connection_handler(void *socket_fd)
{
	int sockfd = *((int *)socket_fd);
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
	
	if (rc = pthread_create(&receiver, NULL, receive_audio, NULL)) {
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

	struct sockaddr_in server, client;
	int addrlen;	
	int sockfd, newsockfd;

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
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	/* convert from host to network byte order */
	/* htons : host to network short */
	server.sin_port = htons(PORT);

	dbg("Binding the control socket");

	if (bind(sockfd, (struct sockaddr *)&server, sizeof(server))) {
		fprintf(stderr, "Unable to bind control socket \n");
		return EXIT_FAILURE;
	}

	dbg("Control socket binded");
	/* Start listening for incoming connections */
	dbg("Listening for incoming connections");
	listen(sockfd, 1); /* Allow only single connection */
	
	addrlen = sizeof(client);
	
	printf("Waiting for connection \n");
	
	while ((newsockfd = accept(sockfd, (struct sockaddr *)&client, &addrlen))) {
		char c;
		int ret;
		char ipstr[INET_ADDRSTRLEN]; /* holds the client ip address */	

		/* Convert binary address (in network byte order) to ascii string */
		inet_ntop(AF_INET, &client.sin_addr, ipstr, sizeof ipstr);

		printf("%s is calling \n", ipstr);
		printf("Accept call? y/n : default [y]");
		
		c = getc(stdin);
		if (c == 'n' || c == 'N') {
			/* reject the call */
			/* stop reception and transmission */
			printf("Rejecting call from %s \n",
				ipstr);

		} else {
			/* Instantiate a connection_handler thread to
			 * handle call */
			pthread_create(&call, NULL, connection_handler, &newsockfd);
			/* wait for call to finish before accepting another call */
			printf("Call in progress\n");
			pthread_join(call, NULL);

			/* TODO: add duration of call */
			printf("Call ended \n");

		}
		/* stop reception and transmission */
		if (ret = shutdown(newsockfd, 2)) {
			fprintf(stderr,
				"Unable to end connection: %s \n",
				strerror(errno));
		}

		/* close the socket so that it can be reused */
		close(newsockfd);

		printf("Waiting for call \n");	
	}

	if (newsockfd < 0) {
		fprintf(stderr, "Accept failed \n");
		exit(1);
	}

	return EXIT_SUCCESS;
}