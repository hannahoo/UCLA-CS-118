/*
Author: Nathan Tung
packetTransfer.h -- contains packet sending/receiving functions with specified header and packet loss/corruption simulation
*/

#ifndef packetTransfer
#define packetTransfer

#define MAXBUFLEN 1024
#define HEADERSIZE 16

int send2(int sockfd, char *buf, size_t len, struct sockaddr *dest_addr, socklen_t addrlen, int seq, int ack, short fin, short crc, double pl, double pc);

int receive2(int sockfd, char *buf, size_t *len, struct sockaddr *src_addr, socklen_t *addrlen, int *seq, int *ack, short *fin, short *crc);

#endif
