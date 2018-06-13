#include <netinet/in.h>

typedef struct thread_arg{
	int sockfd;
	int protocol;
	unsigned int pkt_type;
	struct sockaddr_in dest_addr; // to be typecast in  struct sockaddr_in
} thread_arg_t;


int
generate_pkt(int sock, int protocol,
                unsigned int type,
                void *dest_addr);

int
igmp_pim_pkt_generator(void);
