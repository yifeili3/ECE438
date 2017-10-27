#include <MacTypes.h>
#include "sender.h"
#include "receiver.h"

//struct socketaddr_in bind_addr, receiver_addr;
struct sockaddr_storage their_addr; // connector's address information
socklen_t addr_len = sizeof their_addr;

unsigned long long int num_pkt_sent = 0, num_pkt_received = 0, num_pkt_total = 0;
int64_t timeout, estimatedRTT = 1000, deviation = 1;

FILE *fp;
double cwnd = 1;
int ssthread = 8, dupACK = 0;
int send_base = 0, next_send = 0, file_pt = 0;
int global_seq_num = 0;
ssize_t numbytes;
int file_read_finished = 0;
int window_ack[SWND] = {0};
packet window_buffer[SWND];
int64_t sent_time[SWND];
uint8_t buf[sizeof(packet)];
int soc_state = CLOSED;


uint64_t time_now() {
    struct timeval current;
    gettimeofday(&current, 0);
    return (uint64_t)(current.tv_sec * 1000000 + current.tv_usec);
}

int setTimeout(int sockfd, int usec)
{
    if (usec < 0)
        return -1;
    struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = usec;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("sender: setsockopt");
    }
    return 0;
}

void updateTimeout(int64_t sentTime) {
    int64_t sampleRTT = time_now() - sentTime;
    estimatedRTT = (int64_t)(0.875 * estimatedRTT + 0.125 * sampleRTT); // alpha = 0.875
    deviation += (int64_t)(0.25 * ( abs(sampleRTT - estimatedRTT) - deviation)); //delta = 0.25
    timeout = (estimatedRTT + 4 * deviation); // mu = 1, phi = 4
    timeout = timeout/5;
}

int handshake(int sockfd) {
    packet pkt;
    pkt.msg_type = SYN;
    pkt.data_size=0;
    while(1) {
        memcpy(buf, &pkt, sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
            perror("can not send SYN to sender");
            exit(2);
        }
        if((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len))==-1){
            perror("can not receive SYN_ACK from sender");
            exit(2);
        }
        memcpy(&pkt, buf, sizeof(packet));
        if(pkt.msg_type == SYN_ACK){
            soc_state = SLOW_START;
            break;
        }
    }
    pkt.msg_type = ACK;
    memcpy(&pkt, buf, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
        perror("can not receive SYN_ACK from sender");
        exit(2);
    }

    return 1;
}

int buildSenderSocket(char* hostname, char* hostUDPport) {
    int rv, sockfd, optval=1;  // listen on sock_fd,
//    int recver_port = atoi(hostUDPport);
//    int sender_port = 8000;
    struct addrinfo hints, *recvinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &recvinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = recvinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: error opening socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
        perror("can not send to sender");
        exit(2);
    }

    if (handshake(sockfd) != -1) {
    	return sockfd;
    } else {
    	perror("Fail handshaking");
    	exit(2);
    }
    
}

int fillingWindow(int num_pkt) {
    int counter = 0;
    char file_buffer[MSS];
    packet pkt;
    while (!file_read_finished && counter < num_pkt) {
        if( window_ack[file_pt] == 0 ) { break; }
        if( fgets (file_buffer, MSS, fp)== NULL ) {
            file_read_finished = 1;
            //TODO: tell the receiver it is the last?
            break;
        } else {
            pkt.data_size = strlen(file_buffer);
            pkt.seq_num = global_seq_num;
            pkt.msg_type = DATA;
            memcpy(pkt.data, &file_buffer, sizeof(file_buffer));
            memcpy(&window_buffer[file_pt], &pkt, sizeof(pkt));
            window_ack[file_pt] = 0;
            file_pt = (file_pt + 1) % SWND;
            global_seq_num = (global_seq_num + 1) % MAX_SEQ;
        }
        ++counter;
    }

    return counter;
}

int sendMultiPackets (int sockfd, int start_pt, int last_pt) {
    // Inclusive for start and last
    for (int i = start_pt; i < last_pt; ++i) {
        i %= SWND;
        memcpy(buf, &window_buffer[i], sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt", i);
            exit(2);
        }
    }
    num_pkt_sent += (last_pt + SWND - start_pt) % SWND;
    fillingWindow( (last_pt + SWND - start_pt) % SWND );
    return 0;
}

int sendSinglePacket (int sockfd, int pt) {
    memcpy(buf, &window_buffer[pt], sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
        perror("Error: data sending");
        printf("Fail to send %d pkt", pt);
        exit(2);
    }
    sent_time[pt] = time_now();
    return 0;
}


int sendAllowedPackets (int sockfd) {
    if (next_send < send_base) {
        next_send += SWND;
    }
    int last_pk = min(send_base + SWND, next_send + (int)cwnd);
    sendMultiPackets(sockfd, next_send, last_pk);
    next_send = (last_pk + 1) % SWND;

    return 0;
}

int handleACK (packet pkt, int sockfd) {
    if (pkt.ack_num > next_send ||
        (send_base < next_send && pkt.ack_num < send_base)) {
        perror("Out of order dupACK");
        return -1;
    }

    if (window_ack[pkt.ack_num] == 0) {
        // new ACK
        if (soc_state == FAST_RECOVERY) {
            cwnd = ssthread * 1.0;
            dupACK = 0;
            soc_state = CONGESTION_AVOID;
        }

        // Mark all ACKed, move sendbase
        int last_send_base = send_base, tmp;
        send_base = pkt.ack_num;    // Cumulative ACK
        tmp = send_base > last_send_base ? send_base : send_base + SWND;
        for (int i = last_send_base; i < tmp; ++i) {
            window_ack[i % 5] = 1;
        }
        num_pkt_received += tmp - last_send_base;

        switch (soc_state) {
            case SLOW_START:
                cwnd += 1;
                dupACK = 0;
                sendAllowedPackets(sockfd);
                break;
            case CONGESTION_AVOID:
                cwnd += 1.0 / cwnd;
                dupACK = 0;
                sendAllowedPackets(sockfd);
                break;
            case FAST_RECOVERY:
                cwnd = ssthread;
                dupACK = 0;
                soc_state = CONGESTION_AVOID;
                break;
            default:
                perror("Wrong socket status");
                return -1;
        }
    } else if (pkt.ack_num == send_base) { // dup ACK
        if (soc_state == SLOW_START || soc_state == CONGESTION_AVOID) {
            ++dupACK;
        } else { cwnd += 1; }
    } else {
        perror("Invalid ACK packet");
    }

    // cwnd > ssthread
    if (soc_state == SLOW_START && cwnd > ssthread) {
        soc_state = CONGESTION_AVOID;
    }

    // dupACK to 3
    if (dupACK >= 3) {
        ssthread = (int) cwnd / 2;
        cwnd = ssthread + 3.0;
        sendSinglePacket(sockfd, send_base);
        soc_state = FAST_RECOVERY;
    }
    updateTimeout(sent_time[pkt.ack_num]);
    return 0;
}

void endConnection(int sockfd){
    packet pkt;
    pkt.msg_type = FIN;
    pkt.data_size=0;
    memcpy(buf, &pkt, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
        perror("can not send FIN to sender");
        exit(2);
    }
    // Wait for two ACK
    while (1) {
        packet ack;
        if ((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&ack, buf, sizeof(packet));
        if (ack.msg_type == FIN_ACK) {
            soc_state = FIN_WAIT;
            break;
        }
    }
    // Wait for the FIN
    setTimeout(sockfd, (int) timeout * 10);
    while (1) {
        packet ack;
        if ((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&ack, buf, sizeof(packet));
        if (ack.msg_type == FIN) {
            soc_state = FIN_WAIT;
            break;
        }

        pkt.msg_type = FIN_ACK;
        pkt.data_size=0;
        memcpy(buf, &pkt, sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
            perror("can not send FIN to sender");
            exit(2);
        }
    }


}

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    int sockfd = buildSenderSocket(hostname, hostUDPport);

    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */
    if (fillingWindow(SWND) == -1) {
        perror("Error during filling window");
    }


    num_pkt_total = (unsigned long long int) ceil(bytesToTransfer * 1.0 / MSS);

    sendAllowedPackets(sockfd);
    setTimeout(sockfd, (int) estimatedRTT);
    while (!file_read_finished && num_pkt_sent < num_pkt_total) {
        //TODO: condition need to modify
        // Wait for ack
        if((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, NULL, NULL)) == -1) {
            perror("can not receive data");
            exit(2);
        }
        packet pkt;
        memcpy(&pkt, buf, sizeof(packet));
        if(pkt.msg_type == ACK) {
            handleACK(pkt, sockfd);
            continue;
        }
    }

    fclose(fp);

    endConnection(sockfd);
}