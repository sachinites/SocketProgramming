
#include <stdio.h>
#include <stdlib.h>
#include "../include/common.h"
#include <sys/types.h> 
#include <sys/socket.h>
#include <pthread.h>
#include "../include/pkt_generator.h"
#include <memory.h>
#include <netdb.h>
#include <unistd.h> // for sleep

#define DEST_ADDR	"192.168.4.1"
#define NOT_REQ		 2000

pthread_mutex_t igmp_sock_mutex;
pthread_mutex_t pim_sock_mutex;

static void*
_generate_pkt(void *arg){
	thread_arg_t *_arg = (thread_arg_t *)arg;
	unsigned int seqno = 0;
	int rc = 0;
	while(1){
	switch(_arg->protocol){
	case _IPPROTO_IGMP:
	{
		igmp_hdr_t igmphdr;
		memset(&igmphdr, 0 , sizeof(igmp_hdr_t));
		igmphdr.type = _arg->pkt_type;
		igmphdr.seqno = seqno++;
		pthread_mutex_lock(&igmp_sock_mutex);
		rc = sendto(_arg->sockfd, &igmphdr, sizeof(igmp_hdr_t), 0, 
			(struct sockaddr *)&_arg->dest_addr, sizeof(struct sockaddr));
		printf("protocol = %-20s pkt_type = %-15s seqno = %-7d bytes send = %d\n", 
			get_string(_arg->protocol), get_string(_arg->pkt_type), seqno, rc);
		pthread_mutex_unlock(&igmp_sock_mutex);
	}
	break;
	case _IPPROTO_PIM:
	{
		pim_hdr_t pimhdr;
		memset(&pimhdr, 0 , sizeof(pim_hdr_t));
		pimhdr.type = _arg->pkt_type;
		pimhdr.seqno = seqno++;
		pthread_mutex_lock(&pim_sock_mutex);
		rc = sendto(_arg->sockfd, &pimhdr, sizeof(pim_hdr_t), 0, 
			(struct sockaddr *)&_arg->dest_addr, sizeof(struct sockaddr));
		printf("protocol = %-20s pkt_type = %-15s seqno = %-7d bytes send = %d\n", 
			get_string(_arg->protocol), get_string(_arg->pkt_type), seqno, rc);
		pthread_mutex_unlock(&pim_sock_mutex);
	}
	break;
	}

	sleep(2);
	}
	printf("%s(): Exiting ....\n", __FUNCTION__);
	free(arg);
	return NULL;
}

int 
generate_pkt(int sock, int protocol,  
		unsigned int type, 
		void *dest_addr){

	int rc = 0;		   
	pthread_t thread; 
	pthread_attr_t attr;	
	thread_arg_t *arg = NULL;
	dest_addr = (struct sockaddr_in *)dest_addr;

	if(!sock || !dest_addr){
		printf("Invalid argument : %s()", __FUNCTION__);
		return FAILURE;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	arg = calloc(1, sizeof(thread_arg_t)); // this memory should reside in heap

	arg->sockfd = sock;
	arg->protocol = protocol;
	arg->pkt_type = type;
	memcpy(&arg->dest_addr, dest_addr, sizeof(struct sockaddr_in));

	rc = pthread_create(&thread, &attr, _generate_pkt, (void *)arg);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
	}
	return SUCCESS;
}


int
igmp_pim_pkt_generator(void){
	int igmp_sockfd, pim_sockfd;
	struct sockaddr_in dest;
	int halt;
	dest.sin_family = AF_INET;
	dest.sin_port = NOT_REQ;
	struct hostent *host = (struct hostent *)gethostbyname(DEST_ADDR);
	dest.sin_addr = *((struct in_addr *)host->h_addr);

	pthread_mutex_init(&igmp_sock_mutex, NULL);
	
	igmp_sockfd = socket(AF_INET, SOCK_RAW, _IPPROTO_IGMP);
	if(igmp_sockfd == FAILURE){
		printf("igmp socket creation failed\n");
		return FAILURE;
	}
	printf("igmp socket creation success\n");
	generate_pkt(igmp_sockfd, _IPPROTO_IGMP, IGMP_REPORTS, (void *)&dest);
	generate_pkt(igmp_sockfd, _IPPROTO_IGMP, IGMP_LEAVE,   (void *)&dest);		 	
	generate_pkt(igmp_sockfd, _IPPROTO_IGMP, IGMP_QUERY,   (void *)&dest);		 	

	pim_sockfd = socket(AF_INET, SOCK_RAW, _IPPROTO_PIM);
	if(pim_sockfd == FAILURE){
		printf("pim socket creation failed\n");
		return FAILURE;
	}
	printf("pim socket creation success\n");
	
	generate_pkt(pim_sockfd, _IPPROTO_PIM, PIM_HELLO,    (void *)&dest);
	generate_pkt(pim_sockfd, _IPPROTO_PIM, PIM_JOIN,     (void *)&dest);
	generate_pkt(pim_sockfd, _IPPROTO_PIM, PIM_REGISTER, (void *)&dest);

	scanf("%d", &halt);	
	return 0;
}

int
main(int argc, char **argv){
	igmp_pim_pkt_generator();
	return 0;
}
