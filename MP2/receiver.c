#include "receiver.h"

int lastACK=0; // the most recent acked packet
int nextACK=0; // next expecting ack packet

vector<segment> receive_window;
vector<uint8_t> file_buffer;

int state=CLOSED;
FILE * fd;
int socket_fd;
int numbytes;
struct sockaddr_storage their_addr;
socklen_t addr_len;
uint8_t buf[sizeof(packet)];
    

int buildSocket(char* myUDPport){
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
        if (( socket_fd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("UDP server: socket");
            continue;
        }
        if(bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd);
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

    lastACK = 0;
    nextACK = 0;
    state = LISTEN;

    addr_len = sizeof their_addr;
	numbytes = recvfrom(socket_fd,buf,sizeof(packet),0,(struct sockaddr *) &their_addr, &addr_len);
    
    packet pkt; 
    memcpy(&pkt,buf,sizeof(packet));
    if(state == LISTEN && pkt.msg_type == SYN){ 
        while(1){
            pkt.msg_type = SYN_ACK;
            state = SYN_RCVD;
            memcpy(buf,&pkt,sizeof(packet));
            if((numbytes = sendto(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
                perror("can not send to sender");
                exit(2);
            }
            if((numbytes=recvfrom(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len))==-1){
                perror("can not receive from sender");
                exit(2);
            }
            memcpy(&pkt,buf,sizeof(packet));
            if(pkt.msg_type==ACK){
                state = ESTABLISHED;
                break;
            }
        }
    }

    return socket_fd;
}


void handleData(packet & pkt){
    // arrival of inorder segment with expected sequence number
    // send single cumulative ack if lower waiting for ack
    packet temp;
    if(pkt.seq_num == nextACK){
        //send ack
        temp.msg_type=ACK;
        temp.ack_num = nextACK;
        // ACK next missing packet
        temp.data_size = 0; // data size is 0 since we are sending ack
        memcpy(&temp,buf,HEADERSIZE);
        sendto(socket_fd, buf, HEADERSIZE, 0, (struct sockaddr *) &their_addr,addr_len);
        lastACK++;
        for(int i=0;i<pkt.data_size;i++){
            fputc(pkt.data[i],fd);
        }
        nextACK++;
    }
    //arrival out of order segment, send duplicate ack
    else if (pkt.seq_num > nextACK) {
        //buffer this packet

        // send nextack
    }
    //arrival of segment partially fills gap, send ack
    else if (pkt.seq_num < nextACK){
        //send seq_num
        temp.msg_type= ACK;
        temp.ack_num = nextACK;
        temp.data_size= 0;
        memcpy(&temp,buf,HEADERSIZE);
        sendto(socket_fd, buf, HEADERSIZE, 0, (struct sockaddr *) &their_addr,addr_len);
    }

}


void endConndection(){

}

void reliablyReceive(char* myUDPport, char* destinationFile)
{
	fd=fopen(destinationFile,"wb");
	
    socket_fd=buildSocket(myUDPport);

	cout<<"Connection established!"<<endl<<"Receiver ready to receive...."<<endl;

    while(1){
        if((numbytes = recvfrom(socket_fd,buf,sizeof(packet),0,NULL,NULL))==-1){
            perror("can not receive data");
            exit(2);
        }
        packet pkt;
        memcpy(&pkt,buf,sizeof(packet));
        if(pkt.data_size>0){
            switch (pkt.msg_type){
                case DATA:
                    handleData(pkt);
                    break;
                case FIN:
                    endConndection();
                    break;
                default:
                    cout<<"Should not reach here"<<endl;
                    break;
            }
        }
    }

}

