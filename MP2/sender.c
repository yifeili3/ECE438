#include "sender.h"
#include "receiver.h"

//struct socketaddr_in bind_addr, receiver_addr;
struct sockaddr_storage their_addr; // connector's address information
socklen_t addr_len = sizeof their_addr;
struct addrinfo hints, *recvinfo, *p;


FILE *fp;
// For congestion control
double cwnd = 1;
int ssthread = 8, dupACK = 0;
int send_base = 0, next_send = 0, file_pt = 0;
int send_base_acked = 0, global_seq_num = 0;

// Timing related
int64_t timeout, estimatedRTT = 1000, deviation = 1;
uint64_t start_time;

// Tmp parameters or flags
ssize_t numbytes;
int file_read_finished = 0;
unsigned long long int bytesToRead;
unsigned long long int num_pkt_sent = 0, num_pkt_received = 0, num_pkt_total = 0;

// Sliding window related
packet window_buffer[SWND];
int64_t sent_time[SWND];
uint8_t buf[sizeof(packet)];
int soc_state = CLOSED;

void openFile(char* filename, unsigned long long int bytesToTransfer) {
    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */
    fseek (fp, 0, SEEK_END);
    unsigned long long int file_size = ftell(fp);
    rewind(fp);
    bytesToRead = min(file_size, bytesToTransfer);

    num_pkt_total = (unsigned long long int) ceil(bytesToRead * 1.0 / MSS);
    cout << num_pkt_total << endl;
}

uint64_t time_now() {
    struct timeval current;
    gettimeofday(&current, 0);
    return (uint64_t)(current.tv_sec * 1000000 + current.tv_usec);
}

int64_t proc_time_now() {
    return (int64_t) (time_now() - start_time);
}

/*
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
 */

void updateTimeout(int64_t sentTime) {
    int64_t sampleRTT = time_now() - sentTime;
    estimatedRTT = (int64_t)(0.875 * estimatedRTT + 0.125 * sampleRTT); // alpha = 0.875
    deviation += (int64_t)(0.25 * ( abs(sampleRTT - estimatedRTT) - deviation)); //delta = 0.25
    timeout = (estimatedRTT + 4 * deviation); // mu = 1, phi = 4
//    timeout = timeout/5;
}

int handshake(int sockfd) {
    packet pkt;
    pkt.msg_type = SYN;
    pkt.data_size=0;
    while(1) {
        memcpy(buf, &pkt, sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen )) == -1){
            perror("can not send SYN to sender");
            exit(2);
        }
        if((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1){
            perror("can not receive SYN_ACK from sender");
            exit(2);
        }
        memcpy(&pkt, buf, sizeof(packet));
        if(pkt.msg_type == SYN_ACK){
            soc_state = SLOW_START;
            cout << "Receive SYN_ACK" << endl;
            break;
        }
    }
    pkt.msg_type = ACK;
    memcpy(buf, &pkt, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
        perror("can not receive SYN_ACK from sender");
        exit(2);
    }

    return 0;
}

int buildSenderSocket(char* hostname, char* hostUDPport) {
    int rv, sockfd, optval=1;  // listen on sock_fd,

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

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
        break;
	}


	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
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
    char file_buffer[MSS+1];
    packet pkt;
    for (counter = 0; bytesToRead && counter < num_pkt; ++ counter) {
        int byte_trans_once = min(MSS, (int) bytesToRead);
        int read_size = fread (file_buffer, sizeof(char), byte_trans_once, fp);
        if (read_size > 0) {
            pkt.data_size = read_size;
            pkt.seq_num = global_seq_num;
            pkt.msg_type = DATA;
            memcpy(pkt.data, &file_buffer, sizeof(char) * byte_trans_once);
            memcpy(&window_buffer[file_pt], &pkt, sizeof(packet));
            file_pt = (file_pt + 1) % SWND;
            global_seq_num = (global_seq_num + 1) % MAX_SEQ;
        }
        bytesToRead -= read_size;
    }

    cout << "Filling the window" << endl;
    return counter;
}

int sendMultiPackets (int sockfd, int start_pt, int last_pt) {
    // Inclusive for start and last
    for (int i = start_pt; i < last_pt; ++i) {
        int i_pos_in_swnd = i % SWND;
        memcpy(buf, &window_buffer[i_pos_in_swnd], sizeof(packet));
        if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt in SWND", i_pos_in_swnd);
            exit(2);
        }
        cout << "-----------Sending packet " << window_buffer[i_pos_in_swnd].seq_num << "---------------" << endl;
        sent_time[i_pos_in_swnd] = proc_time_now();
    }
    num_pkt_sent += (last_pt - start_pt) % SWND;
    if (!file_read_finished) { fillingWindow( (last_pt- start_pt) % SWND ); }
    return 0;
}

int sendSinglePacket (int sockfd, int pt) {
    memcpy(buf, &window_buffer[pt], sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
        printf("Fail to send %d pkt", pt);
        perror("Error: data sending (single)");
        exit(2);
    }
    sent_time[pt] = proc_time_now();
    return 0;
}

int sendAllowedPackets (int sockfd) {

    if (num_pkt_sent == num_pkt_total) {
        cout << "No packet to send" << endl;
        return 0;
    }

    int last_pk = send_base + (int)cwnd;
    if (next_send < send_base) {
        next_send += SWND;
    }
    if (num_pkt_total - num_pkt_sent < last_pk - next_send ) {
        last_pk = next_send + num_pkt_total - num_pkt_sent;
    }

    sendMultiPackets(sockfd, next_send, last_pk);
    next_send = (last_pk) % SWND;
    cout << "send_base: " << send_base << " next_send: " << next_send << endl;
    return 0;
}

int handleACK (packet pkt, int sockfd) {
    int ack_pos_in_swnd = pkt.ack_num % SWND;
    if ((send_base < next_send && (ack_pos_in_swnd > next_send || ack_pos_in_swnd < send_base)) ||
        (send_base > ack_pos_in_swnd && ack_pos_in_swnd > next_send )) {
        perror("Out of order ACK");
        return -1;
    }

    if ((ack_pos_in_swnd > send_base) ||
            (next_send < send_base && ack_pos_in_swnd <= next_send)) {
        num_pkt_received += (pkt.ack_num - send_base) % SWND;
        send_base = ack_pos_in_swnd;
        cout << "Received a new ACK with " << pkt.ack_num << endl;
        // new ACK
        if (soc_state == FAST_RECOVERY) {
            cwnd = ssthread * 1.0;
            dupACK = 0;
            soc_state = CONGESTION_AVOID;
        }

        cout << "Total Received pkt: " << num_pkt_received << endl;

        switch (soc_state) {
            case SLOW_START:
                cwnd = (cwnd + 1 < SWND) ? cwnd + 1 : SWND - 1;
                dupACK = 0;
                break;
            case CONGESTION_AVOID:
                cwnd = (cwnd + 1.0/cwnd < SWND) ? cwnd + 1.0 / cwnd : SWND - 1;
                dupACK = 0;
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
    } else if (ack_pos_in_swnd == send_base) { // dup ACK
        if (soc_state == SLOW_START || soc_state == CONGESTION_AVOID) {
            ++dupACK;
        } else { cwnd = (cwnd + 1 < SWND) ? cwnd + 1 : SWND - 1; }
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
    updateTimeout(sent_time[ack_pos_in_swnd]);
    cout << "State: " << soc_state << "  cwnd: " << cwnd << " ssthread: " << ssthread
         << " dupACK: " << dupACK << endl;
    sendAllowedPackets(sockfd);

    return 0;
}

void endConnection(int sockfd){
    packet pkt;
    pkt.msg_type = FIN;
    pkt.data_size=0;
    memcpy(buf, &pkt, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
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
            cout << "Receive the FIN_ACK." << endl;
            soc_state = FIN_WAIT;
            break;
        }
    }
    // Wait for the FIN
    // TODO: wait for some time
    while (1) {
        packet ack;
        if ((numbytes = recvfrom(sockfd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&ack, buf, sizeof(packet));
        if (ack.msg_type == FIN) {
            cout << "Receive the last FIN" << endl;
            break;
        }
    }

    pkt.msg_type = FIN_ACK;
    pkt.data_size=0;
    memcpy(buf, &pkt, sizeof(packet));
    if((numbytes = sendto(sockfd, buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
        perror("can not send final FIN to sender");
        exit(2);
    }
    //TODO: Wait for some time to finially close the channel.

}

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    int sockfd = buildSenderSocket(hostname, hostUDPport);
    cout << "Successfully build socket with sockfd: " << sockfd << endl;
//    int sockfd = 3;

    openFile(filename, bytesToTransfer);

    if (fillingWindow(SWND) == -1) {
        perror("Error during filling window");
    }

    // Time started:
    start_time = time_now();

    sendAllowedPackets(sockfd);
//    sendSinglePacket(sockfd, 0);
//    setTimeout(sockfd, (int) estimatedRTT);
    while (num_pkt_sent < num_pkt_total ||
           num_pkt_received < num_pkt_sent) {
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

    //TODO: timeout
    //FIXME: Sending too many packets
}