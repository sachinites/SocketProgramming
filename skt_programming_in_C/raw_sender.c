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
#define NOT_REQ	2000
#define server	"127.0.0.1"

void sendRawData(char sendString[] )
{

        int sock;
        struct sockaddr_in server_addr;
        struct hostent *host;                   //hostent predefined structure use to store info about host
	int rc = 0;
        host = (struct hostent *) gethostbyname(server);//gethostbyname returns a pointer to hostent
        if ((sock = socket(AF_INET, SOCK_RAW, 5)) == -1)
        {
                perror("socket");
                exit(1);
        }

        //destination address structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(NOT_REQ);
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);       //host->h_addr gives address of host
        //bzero(&(server_addr.sin_zero),8);
        rc = sendto(sock, sendString, strlen(sendString), 0,(struct sockaddr *)&server_addr, sizeof(struct sockaddr));

	
        //sendto() function shall send a message through a connectionless-mode socket.
        printf("\nFORWARD REQUEST : '%s' has been forwarded to server, rc = %d\n",sendString, rc);
        close(sock);
}

int 
main(int argc , char **argv){
	char buff[] = "hello, i am sender\0";
	sendRawData(buff);
}
