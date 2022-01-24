#include "tcp_mgmt.h"
#include <sys/socket.h>
#include <netinet/in.h> // for IPPROTO_TCP
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

TcpServer::TcpServer() {
}

TcpServer::TcpServer(const uint32_t& self_ip_addr, const uint16_t& self_port_no) {

    this->self_ip_addr = self_ip_addr;
    this->self_port_no = self_port_no;
}

TcpServer::~TcpServer() { }

/* Tcp Server Fn */

void *
tcp_server_fn (void *arg) {

    int opt = 1;

    TcpServer *tcp_server = (TcpServer *)arg;
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    
    tcp_server->master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );
    tcp_server->dummy_master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if( tcp_server->master_skt_fd == -1 ||
        tcp_server->dummy_master_skt_fd == -1){

        printf("TCP Server Socket Creation Failed\n");
        /* Signal the main routine that thread routine fn has started */
         sem_post(&tcp_server->thread_start_semaphore);
         pthread_exit(NULL);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = tcp_server->self_port_no;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (setsockopt(tcp_server->master_skt_fd, SOL_SOCKET,
                   SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) {
        printf("setsockopt Failed\n");
        exit(0);
    }

    if (setsockopt(tcp_server->master_skt_fd, SOL_SOCKET,
                   SO_REUSEPORT, (char *)&opt, sizeof(opt))<0) {
        printf("setsockopt Failed\n");
       exit(0);
    }

    if (bind(tcp_server->master_skt_fd, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr)) == -1) {
        printf("Error : Master socket bind failed\n");
        exit(0);
    }

    if (listen(tcp_server->master_skt_fd, 5) < 0 ) {

        printf("listen failed\n");
        exit(0);
    }

    server_addr.sin_port        = tcp_server->self_port_no + 1;

    if (setsockopt(tcp_server->dummy_master_skt_fd, SOL_SOCKET,
                   SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) {
        printf("setsockopt Failed\n");
       exit(0);
    }

    if (setsockopt(tcp_server->dummy_master_skt_fd, SOL_SOCKET,
                   SO_REUSEPORT, (char *)&opt, sizeof(opt))<0) {
        printf("setsockopt Failed\n");
        exit(0);
    }

    if (bind(tcp_server->dummy_master_skt_fd, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr)) == -1) {
        printf("Error : Dummy socket bind failed\n");
        exit(0);
    }

    if (listen(tcp_server->dummy_master_skt_fd, 5) < 0 ) {

        printf("listen failed\n");
        exit(0);
    }


    //pthread_cleanup_push(tcp_server_cleanup_handler, (void *)tcp_server);

    FD_ZERO(&tcp_server->active_client_fds);
    FD_ZERO(&tcp_server->backup_client_fds);

    struct sockaddr_in client_addr;
    int bytes_recvd = 0,
        addr_len = sizeof(client_addr);

    FD_SET(tcp_server->master_skt_fd, &tcp_server->backup_client_fds);
    FD_SET(tcp_server->dummy_master_skt_fd, &tcp_server->backup_client_fds);

    while (true) {

        memcpy(&tcp_server->active_client_fds,
                      &tcp_server->backup_client_fds, sizeof(fd_set));

        
    }

    return NULL;
}

void TcpServer::Start() {

    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    sem_init(&this->thread_start_semaphore, 0, 0);
    pthread_create(&this->server_thread, &attr, tcp_server_fn, (void *)this);
    sem_wait(&this->thread_start_semaphore);

    if (this->master_skt_fd < 0 ||
        this->dummy_master_skt_fd < 0) {

        printf ("Error : Could not start TCP Server\n");
        return;
    }


}