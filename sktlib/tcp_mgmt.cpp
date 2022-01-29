#include <sys/socket.h>
#include <netinet/in.h> // for IPPROTO_TCP
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "tcp_mgmt.h"

/* Tcp Connection */
TcpConn::TcpConn() {
  
}

TcpConn::~TcpConn() {
  
}

void
TcpConn::Abort() {

}

uint32_t
TcpConn::SendMsg(char *msg, uint32_t msg_size) {

    return 0;
}

uint16_t
TcpServer::GetMaxFd() {

    int i ;
    uint16_t max_fd = 0;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0; i < array_size; i++) {
        
        if (this->fd_array[i] > max_fd) {
            max_fd = this->fd_array[i];
        }
    }

    return max_fd;
}

void
TcpServer::add_to_fd_array(uint16_t fd) {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0 ; i < array_size; i++) {
        if (this->fd_array[i] == -1) {
            this->fd_array[i] = fd;
            return;
        }
    }
    assert(0);
}

void
TcpServer::remove_from_fd_array(uint16_t fd) {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0 ; i < array_size; i++) {
        if (this->fd_array[i] == fd) {
            this->fd_array[i] = -1;
            return;
        }
    }
    assert(0);
}


/* Tcp Server */
TcpServer::TcpServer() {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for ( i = 0 ; i < array_size; i++) {
        this->fd_array[i] = -1;
    }
    sem_init(&semaphore_wait_for_client_operation_complete, 0 , 0);
    accept_new_conn = true;
}

TcpServer::TcpServer(const uint32_t& self_ip_addr, const uint16_t& self_port_no) {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0; i < array_size; i++)
    {
        this->fd_array[i] = -1;
    }

    this->self_ip_addr = self_ip_addr;
    this->self_port_no = self_port_no;
    sem_init(&semaphore_wait_for_client_operation_complete, 0 , 0);
    accept_new_conn = true;
}

TcpServer::~TcpServer() { }

void *
TcpServer::TcpServerThreadFn() {

  int opt = 1;
  bool rc;

    TcpServer *tcp_server = this;
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    tcp_server->master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );
    tcp_server->dummy_master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if( tcp_server->master_skt_fd == -1 ||
        tcp_server->dummy_master_skt_fd == -1){

        printf("TCP Server Socket Creation Failed\n");
        /* Signal the main routine that thread routine fn has started */
         sem_post(&tcp_server->semaphore_wait_for_thread_start);
         pthread_exit(NULL);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(tcp_server->self_port_no);
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
    socklen_t addr_len = sizeof(client_addr);

    FD_SET(tcp_server->master_skt_fd, &tcp_server->backup_client_fds);
    FD_SET(tcp_server->dummy_master_skt_fd, &tcp_server->backup_client_fds);
    add_to_fd_array(tcp_server->master_skt_fd);
    add_to_fd_array(tcp_server->dummy_master_skt_fd);

    uint16_t max_fd = GetMaxFd();

     sem_post(&tcp_server->semaphore_wait_for_thread_start);

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

            if (tcp_server->accept_new_conn == false) {
                printf ("Tcp Server is not accepting new connections\n");
                close(comm_socket_fd);
                continue;
            }

            FD_SET(comm_socket_fd, &tcp_server->backup_client_fds);
            n_clients_connected++;
            add_to_fd_array(comm_socket_fd);

            if (max_fd < comm_socket_fd) {
                max_fd = comm_socket_fd;
            }

            TcpConn *tcp_conn = new TcpConn();
            tcp_conn->comm_sock_fd = comm_socket_fd;
            tcp_conn->conn_state = TCP_ESTABLISHED;
            tcp_conn->conn_origin = tcp_via_accept;
            tcp_conn->peer_addr = client_addr.sin_addr.s_addr;
            tcp_conn->peer_port_no = client_addr.sin_port;
            tcp_conn->self_port_no = tcp_server->self_port_no;
            tcp_conn->self_addr = tcp_server->self_ip_addr;
            TcpClient *tcp_client = new TcpClient();
            tcp_client->tcp_conn = tcp_conn;
            tcp_server->tcp_client_conns.push_back(tcp_client);
            /* Now send Connect notification */
            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_connected) {
                    tcp_server->tcp_notif->client_connected (tcp_client);
            }
         }

        else if (FD_ISSET(tcp_server->dummy_master_skt_fd, &tcp_server->active_client_fds)){
             
             tcp_server_operations_t opn = server_pending_operation;
             switch (opn)
             {
             case tcp_server_operation_none:
                 break;
             case tcp_server_stop_listening_client:
                 break;
             case tcp_server_resume_listening_client:
                 break;
             case tcp_server_stop_accepting_new_connections:
                 break;
             case tcp_server_resume_accepting_new_connections:
                 break;
             case tcp_server_close_client_connection:
                rc = TcpServerChangeState(pending_tcp_client, opn);
                if (rc) max_fd = GetMaxFd();
                 break;
             case tcp_server_operations_max:
             default:;
             }
         }

        else {
            std::list<TcpClient *>::iterator it;
            TcpClient *tcp_client;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            for (it = tcp_client_conns.begin(); it != tcp_client_conns.end(); ++it)
            {
                tcp_client = *it;

                if (tcp_client->listen_by_server == false) continue;

                if (FD_ISSET(tcp_client->tcp_conn->comm_sock_fd, &active_client_fds))
                {
                    tcp_client->tcp_conn->bytes_recvd =
                        recvfrom(tcp_client->tcp_conn->comm_sock_fd,
                                        tcp_client->tcp_conn->recv_buffer,
                                        TCP_CONN_RECV_BUFFER_SIZE,
                                        0, (struct sockaddr *)&client_addr, &addr_len);
                
                    if (tcp_client->tcp_conn->bytes_recvd == 0) {

                        printf ("Printf Client abrupt termination\n");

                        if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_disconnected)
                        {
                            tcp_server->tcp_notif->client_disconnected(tcp_client);
                        }
                        remove_from_fd_array(tcp_client->tcp_conn->comm_sock_fd);
                        tcp_server->RemoveTcpClient(tcp_client);
                        tcp_client->TcpClientAbort();
                         n_clients_connected--;
                        max_fd = GetMaxFd();
                        continue;
                    }

                    if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_msg_recvd)
                    {
                        tcp_server->tcp_notif->client_msg_recvd(tcp_client, tcp_client->tcp_conn->recv_buffer, tcp_client->tcp_conn->bytes_recvd);
                    }
                }
            } // for loop ends

        }

    } // server loop ends

    return NULL;
}

/* Tcp Server Fn */
void *
tcp_server_fn (void *arg) {

  TcpServer *tcp_server = (TcpServer *)arg;
  return tcp_server->TcpServerThreadFn();
}

void TcpServer::Start() {

    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    sem_init(&this->semaphore_wait_for_thread_start, 0, 0);
    pthread_create(&this->server_thread, &attr, tcp_server_fn, (void *)this);
    sem_wait(&this->semaphore_wait_for_thread_start);

    if (this->master_skt_fd < 0 ||
        this->dummy_master_skt_fd < 0) {

        printf ("Error : Could not start TCP Server\n");
        return;
    }

    printf ("TcpServer Thread Created Successfully\n");
}

void
TcpServer::TcpSelfConnect() {

    int rc = 0;
    int sock_fd = -1;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        return;
    }

    struct hostent *host;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = (this->self_port_no + 1);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    rc = connect(sock_fd, (struct sockaddr *)&server_addr,sizeof(struct sockaddr));
    if(rc < 0) {
        printf ("%s() Connect Failed\n", __FUNCTION__);
        return;
    }
    
    printf ("Waiting for client disconnection\n");
    sem_wait(&semaphore_wait_for_client_operation_complete);
    printf ("Client disconnection successful\n");

    close(sock_fd);
}

void 
TcpServer::DiscoconnectClient(TcpClient *tcp_client) {

    assert (tcp_client->tcp_conn->comm_sock_fd);
    assert(tcp_client->tcp_conn->conn_state == TCP_ESTABLISHED);
    assert(tcp_client->tcp_conn->conn_origin== tcp_via_accept);
    this->pending_tcp_client = tcp_client;
    tcp_client->ref_count++;
    this->TcpSelfConnect();
}

void
TcpServer::ForceDisconnectAllClients() {

        uint16_t i;
        std::list<TcpClient *>::iterator it;
        for (it = tcp_client_conns.begin() ; it  != tcp_client_conns.end(); ++it) {
            DiscoconnectClient(*it);
        }
}

void 
TcpServer::RegisterClientDisConnectCbk (void (*cbk)(const TcpClient*)) {

    if (!this->tcp_notif) {
        this->tcp_notif = new TcpServerNotification();
    }
    this->tcp_notif->client_disconnected = cbk;
}

    TcpClient *
    TcpServer::GetTcpClientbyFd(uint16_t fd) { 

        std::list<TcpClient *>::iterator it;
        for ( it = tcp_client_conns.begin() ; it  != tcp_client_conns.end(); ++it) {
            if ((*it)->tcp_conn->comm_sock_fd == fd) {
                return *it;
            }
        }
        return NULL;
    }

    TcpClient *
    TcpServer::GetTcpClient(uint32_t ip_addr, uint16_t port_no) { return NULL; }

    void
    TcpServer::RemoveTcpClient (TcpClient *tcp_client) {
         tcp_client_conns.remove(tcp_client);
    }

bool
TcpServer::TcpServerChangeState(
        TcpClient *tcp_client, tcp_server_operations_t opn) {

    bool rc = false;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    switch (opn) {
        case tcp_server_operation_none:
        break;
        case tcp_server_stop_listening_client:
        tcp_client->listen_by_server = false;
        rc = false;
        break;
        case tcp_server_resume_listening_client:
        tcp_client->listen_by_server = true;
        rc = true;
        break;
        case tcp_server_stop_accepting_new_connections:
        this->accept_new_conn = false;
        rc = true;
        break;
        case tcp_server_resume_accepting_new_connections:
        this->accept_new_conn = true;
        rc = true;
        break;
        case tcp_server_close_client_connection:
        {
            assert(tcp_client);
            int comm_socket_fd =  accept (this->dummy_master_skt_fd,
                                                        (struct sockaddr *)&client_addr, &addr_len);
             close (comm_socket_fd);
             assert(tcp_client->tcp_conn->comm_sock_fd);
             FD_CLR (tcp_client->tcp_conn->comm_sock_fd, &this->backup_client_fds);
             remove_from_fd_array(tcp_client->tcp_conn->comm_sock_fd);
             if (tcp_notif &&  tcp_notif->client_disconnected) {
                tcp_notif->client_disconnected(tcp_client);
             }
             RemoveTcpClient(tcp_client);
             tcp_client->TcpClientAbort();
             n_clients_connected--;
             sem_post(&semaphore_wait_for_client_operation_complete);
             rc = true;
        }
        break;
        case tcp_server_operations_max:
        break;
        default:
            ;
    }
    return rc;
}




/* TcpClient */
TcpClient::TcpClient() {

    listen_by_server = true;
}

TcpClient::~TcpClient() {

    assert(!ref_count);
}

bool
TcpClient::TcpClientConnect(uint32_t server_ip, uint16_t server_port) {

    int rc = 0;
    int sock_fd = -1;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        return false;
    }

    struct hostent *host;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = server_port;
    server_addr.sin_addr.s_addr = server_ip;

    rc = connect(sock_fd, (struct sockaddr *)&server_addr,sizeof(struct sockaddr));

    if( rc < 0) {
        printf("connect failed , errno = %d\n", errno);
        close(sock_fd);
        return false;
    }

    this->tcp_conn->comm_sock_fd = sock_fd;
    this->tcp_conn->conn_origin = tcp_via_connect;
    this->tcp_conn->conn_state = TCP_ESTABLISHED;
    this->tcp_conn->peer_addr = server_ip;
    this->tcp_conn->peer_port_no = server_port;
    //this->tcp_conn->self_addr = 0;
    //this->tcp_conn->self_port_no = 0;  /* We dont have any port number when client connects via connect() */
    return true;
}

void
TcpClient::TcpClientDisConnect() {

    if (this->tcp_conn->conn_state != TCP_ESTABLISHED) return;
    close (this->tcp_conn->comm_sock_fd);
    this->tcp_conn->comm_sock_fd = 0;
    this->tcp_conn->conn_state = TCP_DISCONNECTED;
}

void
TcpClient::TcpClientAbort() {

    this->TcpClientDisConnect();
    delete this->tcp_conn;
    this->tcp_conn = NULL;
    ref_count--;
    if (ref_count == 0) {
        delete this;
    }
}