/*
Author: Nathan Tung
Notes: Using skeleton code from Beej's Guide to Network Programming
receiver.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include "packetTransfer.h"

#define MAXBUFLEN 1024
#define HEADERSIZE 16

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	char buf[MAXBUFLEN+HEADERSIZE];
	double pl, pc;

	// extract argument parameters
	if (argc != 6)
	{
		fprintf(stderr,"usage: receiver <hostname> <portnumber> <filename> <Pl> <Pc>\n");
		exit(1);
	}

	char *port = argv[2]; 
	pl = atof(argv[4]);
	pc = atof(argv[5]);

	if(pl < 0 || pc < 0 || pl > 1 || pc > 1)
	{
		fprintf(stderr,"Pl and Pc must be between 0 and 1!\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("receiver: socket");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "Error: receiver failed to bind socket\n");
		return 2;
	}

	if ((numbytes = sendto(sockfd, argv[3], strlen(argv[3]), 0, p->ai_addr, p->ai_addrlen)) == -1) 
	{
		perror("Error: failed to send filename\n");
		exit(1);
	}

	freeaddrinfo(servinfo);

	printf("Requested %s as new_%s from %s\n", argv[3], argv[3], argv[1]);

	// wait to receive requested file in packets from sender
	printf("Waiting for sender's file...\n");

	// initialize variables for sending/receiving acks
	int seq=0;
	int ack=0;
	size_t len = 0;
	short fin=0;
	short crc=0;
	int expectedSeq = 0;

	// initialize empty file to transmit data over
	//FILE *fp = fopen(argv[3], "w+");
	char newFilename[MAXBUFLEN];
	strcpy(newFilename, "new_");
	strcat(newFilename, argv[3]);

	FILE *fp = fopen(newFilename, "w+");

	if (fp==NULL)
	{
		printf("Error: file cannot be created!\n");
		exit(1);
	}

	while(fin!=1)
	{
		numbytes = receive2(sockfd, buf, &len, p->ai_addr, &(p->ai_addrlen), &seq, &ack, &fin, &crc);
		buf[numbytes] = '\0';

		//printf("%d/%d/%d/%d (s/a/f/c) with numbytes: %d\n", seq, ack, fin, crc, numbytes);
		printf("Received %d bytes with sequence #%d\n", numbytes, seq);

		if(numbytes == -1)
		{
			printf("Error: failed to receive file\n");
			exit(1);
		}
		else if(seq==0 && fin==1) // file does not exist!
		{
			printf("Error: file not found!\n");
			exit(1);
		}
		else if(crc==1) // if packet is corrupted, so we ignore it
		{
			printf("Packet is corrupted! Ignoring it.\n");
			fin=0; // make sure we don't end on a corrupt packet
		}
		else if(seq < expectedSeq-MAXBUFLEN || seq > expectedSeq) // if we don't receive the next packet in series
		{
			// or we can send back a packet with previous ACK
			printf("Packet is out of order! Ignoring it.\n");
			fin=0; // make sure we don't end on an out of sequence packet
		}
		else if(fin==1) // file is done transmitting!
		{
			// null-terminate the data if it ends early to prevent carrying over old data
			buf[len] = '\0';

			// print data to file
			fwrite(buf, sizeof(char), len, fp);


			numbytes = send2(sockfd, buf, len, p->ai_addr, p->ai_addrlen, seq, seq+len, 1, crc, 0, 0);
			printf("Sent ACK #%d\n", seq+len);

			// FIN is guaranteed to arrive if the last data packet arrived
			printf("FIN received!\n");

			// FINACK rides on the back of the last ACK packet
			printf("FINACK sent!\n");
		}
		else // packet received, send back ack normally
		{
			numbytes = send2(sockfd, buf, len, p->ai_addr, p->ai_addrlen, seq, seq+len, fin, crc, pl, pc);
			
			if (numbytes == -1)
			{
				printf("ACK #%d lost!\n", seq+len);
			}
			else if(numbytes == -2)
			{
				printf("ACK #%d corrupted!\n", seq+len);
			}
			else
			{
				// print data to file
				fwrite(buf, sizeof(char), len, fp);

				printf("Sent ACK #%d\n", seq+len);
				//increment expectedSeq
				expectedSeq += MAXBUFLEN;
			}

		}

	}
	
	printf("File finished transmitting! Receiver terminating...\n");

	// close file and socket
	fclose(fp);
	close(sockfd);

	return 0;
}


