#include <MacTypes.h>
#include "sender.h"
#include "receiver.h"

struct socketaddr_in bind_addr, receiver_addr;
struct sockaddr_storage their_addr; // connector's address information
socklen_t addr_len = sizeof their_addr;

int64_t timeOut, estimatedRTT = 1000, deviation = 1, difference = 0;

float cwnd = 1;
int ssthread = 8, dupACK = 0, pkt_start_time = 0;
int send_base = 0, next_send = 0, file_pt = 0;
int global_seq_num = 0, numbytes;
int file_read_finished = 0;
int window_ack[SWND] = {0};
packet window_buffer[SWND];
int64_t sent_time[SWND];
uint8_t buf[sizeof(packet)];
int soc_state = CLOSED;

int buildSocket(char* hostname, char* hostUDPport) {
    int rv, sockfd, optval=1;  // listen on sock_fd,
    int recver_port = atoi(hostUDPport);
    int sender_port = 8000;
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

	if((numbytes = sendto(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len))== -1){
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

int handshake(int sockfd) {
    int numbytes;
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
    pkt.mst_type = ACK;
    memcpy(&pkt, buf, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len))==-1) {
        perror("can not receive SYN_ACK from sender");
        exit(2);
    }

    return 1;
}

int fillingWindow(FILE *fp) {
    uint8_t file_buffer[MSS];
    packet pkt;
    while (!file_read_finished) {
        if( fgets (file_buffer, MSS, fp)== NULL ) {
            file_read_finished = 1;
            //TODO: tell the receiver it is the last?
            break;
        } else if (window_ack[file_pt] == 1) {
            pkt.data_size = strlen(file_buffer);
            pkt.seq_num = global_seq_num;
            pkt.msg_type = DATA;
            memcpy(pkt.data, &file_buffer, sizeof(file_buffer));
            memcpy(window_buffer[file_pt], &pkt, sizeof(pkt));
            window_ack[file_pt] = 0;
            file_pt = (file_pt + 1) % SWND;
            global_seq_num = (global_seq_num + 1) % MAX_SEQ;
        } else {
            break;
        }
    }

    return file_read_finished;
}

int sendMultiPackets (int sockfd, int last_pt) {
    uint8_t flag = 1;
    for (int i = 0; i < (int)cwnd && (next_send + i < send_base + SWND); ++i) {
        memcpy(buf, &window_buffer[send_base + i], sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, addr_len))== -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt", next_send + i);
            exit(2);
        }
        next_send++;
    }
}


int sendAllowedPackets (int sockfd) {
    int last_pk = min()
}


int handleACK (packet pkt, int sockfd) {
    if (pkt.seq_num > next_send ||
            (send_base < next_send && pkt.seq_num < send_base)) {
        perror("Out of order dupACK");
        return -1;
    }

    if (window_ack[pkt.seq_num] == 0) {
        // new ACK
        send_base = pkt.seq_num;    // Cumulative ACK
        while (window_ack[send_base] == 0) {
            window_ack[send_base++] = 1;
        }
        switch (soc_state) {
            case SLOW_START:
                cwnd += 1;
                dupACK = 0;
                transPackets(sockfd);
                break;
            case CONGESTION_AVOID:
                cwnd += 1.0 / cwnd;
                dupACK = 0;
                transPackets(sockfd);
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
    }




        // TODO: whether cwnd > ssthread

    }

    // TODO: deal with socket state
    if (soc_state == SLOW_START) {

    }

    // TODO: deal with CWND

    // TODO:
}

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    int sockfd = buildSocket(hostname, hostUDPport);
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */
    if (fillingWindow(fp) == -1) {
        perror("Error during filling window");
    }

    while (1) {
        sendAllCwnd(sockfd);
        // Wait for ack
        if((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, NULL, NULL)) == -1) {
            perror("can not receive data");
            exit(2);
        }
        packet pkt;
        memcpy(&pkt, buf, sizeof(packet));
        if(pkt.msg_type == ACK){
            handleACK(pkt, sockfd);
            continue;
        }
        //TODO
    }

    fclose(fp);


}