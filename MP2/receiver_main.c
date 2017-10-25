#include <stdio.h>
#include <stdlib.h>
#include "receiver.h"


int main(int argc, char** argv)
{

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	reliablyReceive(argv[1], argv[2]);
}
