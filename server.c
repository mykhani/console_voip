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

struct connection_data {
	struct sockaddr_in client;
	int sockfd;
};

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
void *connection_handler(void *data)
{
	pthread_t sender, receiver, playback;
	struct connection_data conn = *((struct connection_data *)data);
	struct sockaddr_in client = conn.client;
	int sockfd = conn.sockfd;

	char ipstr[INET_ADDRSTRLEN]; /* holds the client ip address */
	char buffer[BUFLEN];
	char c;
	int rc;
	/* instantiate sender thread */
        dbg("Creating a sender thread");

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

        if (rc = pthread_create(&sender, NULL, send_audio, NULL)) {
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
	
	if (rc = pthread_create(&playback, NULL, play_audio, NULL)) {
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

	struct sockaddr_in server, client;
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
		/* Instantiate a connection_handler thread to
		 * handle call */
		conn.client = client;
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
