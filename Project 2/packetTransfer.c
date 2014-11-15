/*
Author: Nathan Tung
packetTransfer.c -- contains packet sending/receiving functions with specified header and packet loss/corruption simulation
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

int send2(int sockfd, char *buf, size_t len, struct sockaddr *dest_addr, socklen_t addrlen, int seq, int ack, short fin, short crc, double pl, double pc)
{
	// determine whether to simulate packet loss/corruption
	int plRate = (int)(pl*100);
	int pcRate = (int)(pc*100);
	int packetLoss = ((rand()%100+1)<=plRate);
	int packetCorruption = ((rand()%100+1)<=pcRate);

	// create temporary char buffer with header and data to represent buffer
	char* temp = malloc(HEADERSIZE + len);
	bzero(temp, HEADERSIZE + len);

	// header information
	memcpy(temp, &seq, sizeof(int));
	memcpy(temp+4, &ack, sizeof(int));
	memcpy(temp+8, &fin, sizeof(short));
	memcpy(temp+10, &crc, sizeof(short));
	memcpy(temp+12, &len, sizeof(int));

	// actual data
	memcpy(temp+16, buf, len);

	int numbytes = -1;

	// simulate packet corruption
	if(packetCorruption)
	{
		short newCrc=1;
		memcpy(temp+10, &newCrc, sizeof(short));
	}

	// simulate packet loss
	if(!packetLoss)
	{
		numbytes = sendto(sockfd, temp, HEADERSIZE + len, 0, dest_addr, addrlen);
	}

	free(temp);

	if(packetCorruption && !packetLoss)
		numbytes = -2;

	return numbytes; //let numbytes=-1 mean packet loss, numbytes=-2 mean packet corruption
}


int receive2(int sockfd, char *buf, size_t *len, struct sockaddr *src_addr, socklen_t *addrlen, int *seq, int *ack, short *fin, short *crc)
{
	// create temporary char buffer with header and data to represent buffer
	// char* temp = malloc(HEADERSIZE + len);
	char temp[MAXBUFLEN+HEADERSIZE];

	// zero out buf to prevent bad data
	bzero(buf, MAXBUFLEN+HEADERSIZE);

	// receive and populate temp buffer
	int numbytes = recvfrom(sockfd, temp, HEADERSIZE + MAXBUFLEN, 0, src_addr, addrlen);

	// extract header information
	memcpy(seq, temp, sizeof(int));
	memcpy(ack, temp+4, sizeof(int));
	memcpy(fin, temp+8, sizeof(short));
	memcpy(crc, temp+10, sizeof(short));
	memcpy(len, temp+12, sizeof(int));

	// extract actual data based on length header
	memcpy(buf, temp+16, MAXBUFLEN);

	return numbytes;
}
