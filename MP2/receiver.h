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
#include <utility>
#include <vector>
#include <map>
#include <iostream>
#ifndef RECEIVER_H_
#define RECEIVER_H_

#define MSS 1000
#define RWND 20000
#define DATA 0
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define FIN 4
#define FIN_ACK 5
#define MAXSEQUENCE 150000

#define CLOSED 0
#define LISTEN 1
#define SYN_RCVD 2
#define ESTABLISHED 3
#define CLOSE_WAIT 4
#define LAST_ACK 5
#define MAXBUFSIZE 10000
#define HEADERSIZE 16
using namespace std;

//packet structure used for transfering
typedef struct{
	int 	data_size;
	int 	seq_num;
	int     ack_num;
	int     msg_type; //SYN 0 ACK 1 FIN 2 FINACK 3
	char data[MSS];
}packet;

void reliablyReceive(char* myUDPport, char* destinationFile);
int  buildSocket(char* myUDPport);
void handleData(packet pkt);
void endConndection();
#endif