#include "tcp_mgmt.h"
#include <sys/socket.h>
#include <netinet/in.h> // for IPPROTO_TCP
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

/* Tcp Connection */
TcpConn::TcpConn() {

}




/* Tcp Server */
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


    FD_ZERO(&tcp_server->active_client_fds);
    FD_ZERO(&tcp_server->backup_client_fds);

    struct sockaddr_in client_addr;
    int bytes_recvd = 0;
    socklen_t addr_len = sizeof(client_addr);

    FD_SET(tcp_server->master_skt_fd, &tcp_server->backup_client_fds);
    FD_SET(tcp_server->dummy_master_skt_fd, &tcp_server->backup_client_fds);

    uint16_t max_fd = tcp_server->master_skt_fd > tcp_server->dummy_master_skt_fd ?
                    tcp_server->master_skt_fd :
                    tcp_server->dummy_master_skt_fd;

    uint16_t sec_max_fd = tcp_server->master_skt_fd < tcp_server->dummy_master_skt_fd ?
                    tcp_server->master_skt_fd :
                    tcp_server->dummy_master_skt_fd;

     sem_post(&tcp_server->thread_start_semaphore);

    while (true) {

        memcpy(&tcp_server->active_client_fds,
                      &tcp_server->backup_client_fds, sizeof(fd_set));

         printf ("Server blocked on select\n");
         select( max_fd +1 , &tcp_server->active_client_fds, NULL, NULL, NULL);

         if (FD_ISSET(tcp_server->master_skt_fd, &tcp_server->active_client_fds)){

             /* Connection initiation Request */
            int comm_socket_fd =  accept (tcp_server->master_skt_fd,
                                                        (struct sockaddr *)&client_addr, &addr_len);

            if (comm_socket_fd < 0 ){
                printf ("Error : Bad Comm FD returned by accept\n");
                continue;
            }

            FD_SET(comm_socket_fd, &tcp_server->backup_client_fds);

            if (max_fd > comm_socket_fd) {

                if (sec_max_fd > comm_socket_fd) {

                }
                else {
                    sec_max_fd = comm_socket_fd;
                }
            }
            else {
                sec_max_fd = max_fd;
                max_fd = comm_socket_fd;
            }

            TcpConn *tcp_conn = new TcpConn();
            tcp_conn->comm_sock_fd = comm_socket_fd;
            tcp_conn->conn_state = TCP_ESTABLISHED;
            tcp_conn->conn_origin = tcp_via_accept;
            tcp_conn->peer_addr = client_addr.sin_addr.s_addr;
            tcp_conn->peer_port_no = client_addr.sin_port;
            tcp_conn->self_port_no = tcp_server->self_port_no;
            tcp_conn->self_port_no = tcp_server->self_ip_addr;
            TcpClient *tcp_client = new TcpClient();
            tcp_client->tcp_conn = tcp_conn;
            tcp_server->tcp_client_conns.push_back(tcp_client);
            /* Now send Connect notification */
            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_connected) {
                    tcp_server->tcp_notif->client_connected (tcp_client);
            }
         }
         else if (FD_ISSET(tcp_server->dummy_master_skt_fd, &tcp_server->active_client_fds)){

         }
        else {

        }

    } // server loop ends

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

    printf ("TcpServer Thread Created Successfully\n");
}

/* TcpClient */
TcpClient::TcpClient() {

}