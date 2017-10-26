#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <list>	// list
#include <utility>	// pair
#include <iostream>
using namespace std;


#ifndef SENDER_H_
#define SENDER_H_

#define MSS 1000
#define SWND 150
#define DATA 0
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define FIN 4
#define FIN_ACK 5

#define MAX_SEQ 150

#define HEADERSIZE 16
//packet structure used for transfering
typedef struct{
    int 	data_size;
    int 	seq_num;
    int     ack_num;
    int     msg_type; //SYN 0 ACK 1 FIN 2 FINACK 3
    uint8_t data[MSS];
} packet;;
enum socket_state {CLOSED, SLOW_START, CONGESTION_AVOID, FAST_RECOVERY, FIN_WAIT};
int buildSocket(char* hostname, char* hostUDPport);
void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer);
#endif
