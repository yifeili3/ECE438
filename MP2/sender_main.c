#include <stdio.h>
#include <stdlib.h>
#include "sender.h"
int main(int argc, char** argv)
{
	//unsigned short int udpPort;
	unsigned long long int numBytes;

	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}


	printf("timeval size = %ld\n",sizeof(timeval));
	//udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);
	reliablyTransfer(argv[1],argv[2], argv[3],numBytes);

	return 0;
}
