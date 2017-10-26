#include "receiver.h"

int nextACK=0; // next expecting ack packet
int buf_idx=0;
int  receive_window[RWND];
packet window_buffer[RWND];
uint8_t file_buffer[MAXBUFSIZE];

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

    nextACK = 0;
    state = LISTEN;

    addr_len = sizeof their_addr;
	numbytes = recvfrom(socket_fd,buf,sizeof(packet),0,(struct sockaddr *) &their_addr, &addr_len);
    
    packet pkt; 
    memcpy(&pkt,buf,sizeof(packet));
    if(state == LISTEN && pkt.msg_type == SYN){ 
        while(1){
            pkt.msg_type = SYN_ACK;
            pkt.data_size=0;
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


void handleData(packet pkt){
    // arrival of inorder segment with expected sequence number
    // send single cumulative ack if lower waiting for ack
    if(pkt.seq_num == nextACK){
        //send current packet and potential to receive_buffer
        receive_window[nextACK]=1;
        while(receive_window[nextACK]){
            receive_window[nextACK]=0;
            for(int i=0;i<window_buffer[nextACK].data_size;i++){
                if(buf_idx<MAXBUFSIZE){
                    file_buffer[buf_idx++] = pkt.data[i];
                }
                else{
                    // write to file
                    fwrite(file_buffer,sizeof(uint8_t),MAXBUFSIZE,fd);
                    buf_idx=0;
                }
            }
        nextACK=(nextACK+1) % RWND;     
        }
    }
    //arrival out of order segment, send duplicate ack
    else if (pkt.seq_num > nextACK) {
        //buffer this packet
        if (receive_window[pkt.seq_num]==0){
            receive_window[pkt.seq_num]=1;
            memcpy(&window_buffer[pkt.seq_num], &pkt, sizeof(packet));
        }
    }
    //arrival of segment partially fills gap, send ack
    else {
        // sequence number < nextAck
        // simply ignore this packet
    }

    // always send nextACK
    packet ack;
    ack.msg_type=ACK;
    ack.ack_num = nextACK;
    ack.data_size = 0; // data size is 0 since we are sending ack
    memcpy(&ack,buf,sizeof(packet));
    sendto(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len);
}


void endConndection(){
    //receive FIN, send FINACK
    while(1){
        packet pkt;
        pkt.msg_type = FIN_ACK;
        pkt.data_size=0;
        memcpy(buf,&pkt,sizeof(packet));
        if((numbytes = sendto(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
            perror("can not send to sender");
            exit(2);
        }
        state = CLOSE_WAIT;

        // send FIN
        packet fin;
        fin.msg_type=FIN;
        fin.data_size=0;
        memcpy(buf,&fin,sizeof(packet));
        if((numbytes = sendto(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
            perror("can not send to sender");
            exit(2);
        }
        state = LAST_ACK;

        // send final ack
        packet ack;
        if((numbytes=recvfrom(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len))==-1){
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&ack,buf,sizeof(packet));
        if(ack.msg_type==FIN_ACK){
            state = CLOSED;
            break;
        }
    }
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
        if(pkt.msg_type == DATA){
            handleData(pkt);
            continue;
        }
        else if(pkt.msg_type == FIN){
            // send data remain in buffer
            fwrite(file_buffer,sizeof(uint8_t),buf_idx+1,fd);
            endConndection();
            break;
        }
    }
    fclose(fd);
}

