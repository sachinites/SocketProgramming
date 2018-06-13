
#include <stdio.h>
#include <stdlib.h>
#define TRUE   1
#define FALSE  0
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <net/if.h>


int main(int argc, char **argv){
        int sock = 0, len;
	char rec_buff[5000];
	char *interface = "eth2";

	struct sockaddr_ll sll;
	struct ifreq ifr;
	if ((sock = socket(AF_PACKET, SOCK_RAW, ETH_P_IP)) == -1)          //Creating a UDP Socket
	{

		perror("Socket");
		exit(1);
	}
	
	strncpy ((char *) ifr.ifr_name, interface, IFNAMSIZ);
	ioctl (sock, SIOCGIFINDEX, &ifr);	
	sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons (ETH_P_IP);

	/* IF we dont bind the socket, then packet from any inetrface wil be recieved*/
	/* Onne socket can be bind to only one interface. If i want to recieve a packet from
	a subset of interface, create as many sockets and bind*/
	/*
	if(bind ( sock, (struct sockaddr *) &sll, sizeof (sll) ) )
        {
            perror("Bind");
            exit(1);
        }
	*/
	//len = recvfrom(sock, rec_buff, 5000,0,(struct sockaddr *)&client_addr, &addr_len);
	len = read(sock, rec_buff, 5000);
	//printf("ip = %s, port = %d, len = %d\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port), len);
	
	printf("%s\n", rec_buff + sizeof(struct ether_header));
	//main(0, NULL);
	return 0;
}
