all: pa1

pa1: whoisclient whoisserver
		g++ whoisclient.c -o whoisclient
		g++ whoisserver.c -o whoisserver

whoisclient: whoisclient.c
		g++ -c whoisclient.c

whoisserver: whoisserver.c
		g++ -c whoisserver.c

clean:
		rm -f *.o pa1