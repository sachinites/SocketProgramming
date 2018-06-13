
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
#include "../include/common.h"
#include <linux/ip.h>

int main(int argc, char **argv){
        int pim_sock_fd = 0, len, addr_len;
	char rec_buff[256];
	pim_hdr_t *pimhdr = NULL;
	struct sockaddr_in server_addr,
			   client_addr;

	if ((pim_sock_fd = socket(AF_INET, SOCK_RAW, _IPPROTO_PIM )) == -1)
	{

		printf("socket creation failed\n");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = 0;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	addr_len = sizeof(struct sockaddr);
	if (bind(pim_sock_fd,(struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
        {
            printf("socket bind failed\n");
            exit(1);
        }
READ:
	len = recvfrom(pim_sock_fd, rec_buff, 256, 0,(struct sockaddr *)&client_addr, &addr_len);
	printf("Recvd From ip = %-20s", 
		inet_ntoa(client_addr.sin_addr));

	pimhdr = (pim_hdr_t *)(rec_buff + sizeof(struct iphdr));
	printf("protocol = %-15s  pkt_type = %-15s  seqno = %d\n", 
		get_string(_IPPROTO_PIM), get_string(pimhdr->type), pimhdr->seqno);
	goto READ;
	return 0;
}
