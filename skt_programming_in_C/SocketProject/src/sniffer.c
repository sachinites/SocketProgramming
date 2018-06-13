
#include <stdio.h>
#include <stdlib.h>
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
#include "../include/common.h"
#include <linux/ip.h>

int main(int argc, char **argv){
        int af_sock = 0, len;
	char rec_buff[256];
	char *interface[2] = {"eth2", "eth3"};

	struct sockaddr_ll sll;
	struct ifreq ifr;
	struct iphdr *_iphdr = NULL;
	if ((af_sock = socket(AF_PACKET, SOCK_RAW, ETH_P_IP)) == -1)          //Creating a UDP Socket
	{

		printf("Socket creation failed\n");
		exit(1);
	}
#if 1	
	strncpy ((char *) ifr.ifr_name, interface[0], IFNAMSIZ);
	ioctl (af_sock, SIOCGIFINDEX, &ifr);	
	sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons (ETH_P_IP);
	
	/* IF we dont bind the socket, then packet from any inetrface wil be recieved*/
	/* Onne socket can be bind to only one interface. If i want to recieve a packet from
	a subset of interface, create as many sockets and bind*/
	
	if(bind(af_sock, (struct sockaddr *) &sll, sizeof (sll)))
        {
            printf("Bind error\n");
            exit(1);
        }
#endif
READ:
	memset(rec_buff, 0, 256);
	len = read(af_sock, rec_buff, 256);
        _iphdr = (struct iphdr *)(rec_buff +  sizeof(struct ethhdr));
	switch(_iphdr->protocol){
		case _IPPROTO_IGMP:
		{
			igmp_hdr_t *igmphdr = (igmp_hdr_t *)(rec_buff + (_iphdr->ihl*4) + sizeof(struct ethhdr));
			switch(igmphdr->type){
			case IGMP_REPORTS:
			case IGMP_QUERY:
			case IGMP_LEAVE:
				printf("Stolen : protocol = %-20s pkt_type = %-15s seqno = %d\n", get_string(_iphdr->protocol), get_string(igmphdr->type), igmphdr->seqno);
				break;
			default:
				printf("Error : unknown packet type igmphdr->type = %d\n", igmphdr->type);
				break;
			}
		}
		break;
		case _IPPROTO_PIM:
		{

			pim_hdr_t *pimhdr = (pim_hdr_t *)(rec_buff + (_iphdr->ihl*4) + sizeof(struct ethhdr));
			switch(pimhdr->type){
			case PIM_HELLO:
			case PIM_JOIN:
			case PIM_REGISTER:
				printf("Stolen : protocol = %-20s pkt_type = %-15s seqno = %d\n", get_string(_iphdr->protocol), get_string(pimhdr->type), pimhdr->seqno);
				break;
			default:
				printf("Error : unknown packet type pimhdr->type = %d\n", pimhdr->type);
				break;
			}
		}
		break;
		default:
			printf("Error : unknown IP protocol = %d\n", _iphdr->protocol);
	}
	goto READ;
	return 0;
}
