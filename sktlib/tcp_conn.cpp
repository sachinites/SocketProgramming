#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h> // for IPPROTO_TCP
#include "tcp_client.h"
#include "tcp_conn.h"
#include "network_utils.h"

/* Tcp Connection */
TcpConn::TcpConn() {
  
}

TcpConn::~TcpConn() {
    
}

void
TcpConn::Abort() {

}

int
TcpConn::SendMsg(char *msg, uint32_t msg_size) {

    if (this->comm_sock_fd == 0) return -1;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(this->peer_port_no);
    dest.sin_addr.s_addr = htonl(this->peer_addr);
    int rc = sendto(this->comm_sock_fd, msg, msg_size, 0, (struct sockaddr *)&dest, sizeof(struct sockaddr));
    if (rc < 0) {
        printf ("sendto failed\n");
    }
    this->ka_sent++;
    return rc;
}

void
TcpConn::Display() {

    const char *state;
    printf ("comm_fd = %d\n", comm_sock_fd);
    printf ("conn origin = %s\n", 
        conn_origin == tcp_via_accept ? "tcp_via_accept" : "tcp_via_connect");
    switch (conn_state)
    {
    case TCP_DISCONNECTED:
        state = "TCP_DISCONNECTED";
        break;
    case TCP_KA_PENDING:
        state = "TCP_KA_PENDING";
        break;
    case TCP_PEER_CLOSED:
        state = "TCP_PEER_CLOSED";
        break;
    case TCP_ESTABLISHED:
        state = "TCP_ESTABLISHED";
        break;
    default:;
    }
    printf ("conn state = %s\n", state);
    printf ("peer : [%s %d]\n", network_covert_ip_n_to_p(peer_addr, NULL), peer_port_no);
    printf ("self :  [%s %d]\n", network_covert_ip_n_to_p(self_addr, NULL), self_port_no);
    printf ("ka interval : %d       ka recvd : %d        ka sent : %d\n",
            ka_interval, ka_recvd, ka_sent);
}