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
#include <iostream>
#ifndef RECEIVER_H_
#define RECEIVER_H_

#define MSS 4096
#define RWND 200
#define SYN 0
#define ACK 1
#define FIN 2
#define FINACK 3


using namespace std;

//packet structure used for transfering
typedef struct{
	int 	seq_num;
	int     ack_num;
	int     msg_type; //SYN 0 ACK 1 FIN 2 FINACK 3
	uint8_t data[MSS];
}packet;

//struct used in receive_window
typedef struct{
	packet pkt;
	bool   is_Acked;
}segment;


void reliablyReceive(char* myUDPport, char* destinationFile);
int buildSocket(char* myUDPport);

#endif