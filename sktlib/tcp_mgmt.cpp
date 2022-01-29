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
    sem_init(&wait_for_client_disconnection, 0 , 0);
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
}

TcpServer::~TcpServer() { }

void *
TcpServer::TcpServerThreadFn() {

  int opt = 1;

    TcpServer *tcp_server = this;
    
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
    int bytes_recvd = 0;
    socklen_t addr_len = sizeof(client_addr);

    FD_SET(tcp_server->master_skt_fd, &tcp_server->backup_client_fds);
    FD_SET(tcp_server->dummy_master_skt_fd, &tcp_server->backup_client_fds);
    add_to_fd_array(tcp_server->master_skt_fd);
    add_to_fd_array(tcp_server->dummy_master_skt_fd);

    uint16_t max_fd = GetMaxFd();

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

             assert(tcp_server->tcp_client_fd_pending_diconnect);
             FD_CLR (tcp_server->tcp_client_fd_pending_diconnect, &tcp_server->backup_client_fds);
             remove_from_fd_array(tcp_server->tcp_client_fd_pending_diconnect);
             uint16_t pos;
             TcpClient *tcp_client = tcp_server->GetTcpClientbyFd(tcp_server->tcp_client_fd_pending_diconnect, &pos);
             assert(tcp_client);
             if (tcp_server->tcp_notif) {
             tcp_server->tcp_notif->client_disconnected(tcp_client);
             }
             tcp_server->RemoveTcpClient(tcp_client);
             tcp_client->TcpClientAbort();
             tcp_server->tcp_client_fd_pending_diconnect = 0;
             n_clients_connected--;
             sem_post(&wait_for_client_disconnection);
         }
        else {

            
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
    server_addr.sin_port = htons(this->self_port_no + 1);
    server_addr.sin_addr.s_addr = htonl(this->self_ip_addr);

    rc = connect(sock_fd, (struct sockaddr *)&server_addr,sizeof(struct sockaddr));
    if(rc < 0) {
        printf ("%s() Failed\n", __FUNCTION__);
        return;
    }
    
    printf ("Waiting for client disconnection\n");
    sem_wait(&wait_for_client_disconnection);
    printf ("Client disconnection successful\n");

    close(sock_fd);
}

void 
TcpServer::DiscoconnectClient(TcpClient *tcp_client) {

    assert (tcp_client->tcp_conn->comm_sock_fd);
    assert(tcp_client->tcp_conn->conn_state == TCP_ESTABLISHED);
    assert(tcp_client->tcp_conn->conn_origin== tcp_via_accept);
    this->tcp_client_fd_pending_diconnect = tcp_client->tcp_conn->comm_sock_fd;
    this->TcpSelfConnect();
}

void
TcpServer::ForceDisconnectAllClients() {

        uint16_t i;

        for (i = 0 ; i < tcp_client_conns.size(); i++) {
            DiscoconnectClient(tcp_client_conns[i]);
        }
}

void 
TcpServer::RegisterClientDisConnectCbk (void (*cbk)(const TcpClient*)) {

    if (!this->tcp_notif) {
        this->tcp_notif = new TcpServerNotification();
    }
    this->tcp_notif->client_disconnected = cbk;
}
\
    TcpClient *
    TcpServer::GetTcpClientbyFd(uint16_t fd, uint16_t *pos) { return NULL ; }
    TcpClient *
    TcpServer::GetTcpClient(uint32_t ip_addr, uint16_t port_no) { return NULL; }
    void TcpServer::RemoveTcpClient (TcpClient *tcp_client) {}

/* TcpClient */
TcpClient::TcpClient() {

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
    delete this;
}