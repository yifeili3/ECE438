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

#ifndef SENDER_H_
#define SENDER_H_

using namespace std;

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer);
#endif
