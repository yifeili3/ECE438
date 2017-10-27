GCC = g++
COPTS = -g -Wall
all: reliable_sender reliable_receiver

reliable_sender: sender.o
	${GCC} ${COPTS} -D DEBUG -o reliable_sender sender_main.c sender.o

reliable_receiver: receiver.o
	${GCC} ${COPTS} -D DEBUG -o reliable_receiver receiver_main.c receiver.o

sender.o: sender.h sender.c
	${GCC} ${COPTS} -D DEBUG -c sender.c


receiver.o: receiver.h receiver.c
	${GCC} ${COPTS} -D DEBUG -c receiver.c 

clean:
	rm *.o
