
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

int main(int argc, char **argv){
        int sock = 0, len, addr_len;
	char rec_buff[5000];

	struct sockaddr_in address,
			server_addr_udp,
			client_addr;

	if ((sock = socket(AF_INET, SOCK_RAW, 5)) == -1)          //Creating a UDP Socket
	{

		perror("Socket");
		exit(1);
	}

	server_addr_udp.sin_family = AF_INET;
	server_addr_udp.sin_port = htons(2004);
	server_addr_udp.sin_addr.s_addr = INADDR_ANY;
	//bzero(&(server_addr_udp.sin_zero),8);
	addr_len = sizeof(struct sockaddr);
	 if (bind(sock,(struct sockaddr *)&server_addr_udp, sizeof(struct sockaddr)) == -1)//Binding UDP socket
        {
            perror("Bind");
            exit(1);
        }

	len = recvfrom(sock, rec_buff, 5000,0,(struct sockaddr *)&client_addr, &addr_len);
	printf("ip = %s, port = %d, len = %d\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port), len);
	printf("%s\n", rec_buff+20);
	main(0, NULL);
	return 0;
}
