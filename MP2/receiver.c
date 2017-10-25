#include "receiver.h"

int lastACK; // the most recent acked packet
int nextACK; // next expecting ack packet
vector<segment> receive_window;



int buildSocket(char* myUDPport){
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, myUDPport, &hints, &servinfo))!= 0) {
       return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if (( sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("UDP server: socket");
            continue;
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("UDP server:bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "UDP server: failed to bind socket");
        return 2;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}



void reliablyReceive(char* myUDPport, char* destinationFile)
{
	FILE * fd;
	fd=fopen(destinationFile,"wb");
	fclose(fd);
	int socketfd=buildSocket(myUDPport);

	cout<<"Receiver ready to receive...."<<endl;

	//TODO

}

