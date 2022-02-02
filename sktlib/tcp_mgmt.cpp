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

    this->self_ip_addr = 2130706433; // 127.0.0.1
    this->self_port_no = TCP_DEFAULT_PORT_NO;
    sem_init(&semaphore_wait_for_thread_start, 0 , 0);
    name = "Default";
    memcpy(this->ka_msg, "Default KA Msg\0", strlen("Default KA Msg\0"));
}

TcpServer::TcpServer(const uint32_t& self_ip_addr, const uint16_t& self_port_no, std::string name) {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0; i < array_size; i++)
    {
        this->fd_array[i] = -1;
    }

    this->self_ip_addr = self_ip_addr;
    this->self_port_no = self_port_no;
    sem_init(&semaphore_wait_for_thread_start, 0 , 0);
    this->name = name;
    memcpy(this->ka_msg, "Default KA Msg\0", strlen("Default KA Msg\0"));
}

TcpServer::~TcpServer() { 

    printf ("%s TcpServer shut-down successfully\n", this->name.c_str());
}

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
    server_addr.sin_addr.s_addr = INADDR_ANY; /* htonl(tcp_server->self_ip_addr);*/

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
    TcpServerSetState(TCP_SERVER_STATE_ACCEPTING_CONNECTIONS);
    add_to_fd_array(tcp_server->dummy_master_skt_fd);

    uint16_t max_fd = GetMaxFd();

     sem_post(&tcp_server->semaphore_wait_for_thread_start);

    while (true) {

        memcpy(&tcp_server->active_client_fds,
                      &tcp_server->backup_client_fds, sizeof(fd_set));

         printf ("\nServer blocked on select\n");
         select( max_fd +1 , &tcp_server->active_client_fds, NULL, NULL, NULL);

         if (FD_ISSET(tcp_server->master_skt_fd, &tcp_server->active_client_fds)){

             /* Connection initiation Request */
            int comm_socket_fd =  accept (tcp_server->master_skt_fd,
                                                        (struct sockaddr *)&client_addr, &addr_len);

            if (comm_socket_fd < 0 ){
                printf ("\nError : Bad Comm FD returned by accept\n");
                continue;
            }

            if ((tcp_server->TcpServerGetStateFlag() & 
                TCP_SERVER_STATE_ACCEPTING_CONNECTIONS) !=
                 TCP_SERVER_STATE_ACCEPTING_CONNECTIONS) {
                printf ("\nTcp Server is not accepting new connections\n");
                close(comm_socket_fd);
                continue;
            }

            if ((tcp_server->TcpServerGetStateFlag() &
                    TCP_SERVER_STATE_MULTITHREADED_MODE)) {

                    TcpServerCreateMultithreadedClient(comm_socket_fd, &client_addr);
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
            tcp_conn->peer_addr = htonl(client_addr.sin_addr.s_addr);
            tcp_conn->peer_port_no = htons(client_addr.sin_port);
            tcp_conn->self_port_no = tcp_server->self_port_no;
            tcp_conn->self_addr = tcp_server->self_ip_addr;
            TcpClient *tcp_client = new TcpClient();
            tcp_client->tcp_server = tcp_server;
            tcp_client->tcp_conn = tcp_conn;
            tcp_client->StartExpirationTimer();
            tcp_server->tcp_client_conns.push_back(tcp_client);
            /* Now send Connect notification */
            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_connected) {
                    tcp_server->tcp_notif->client_connected (tcp_client);
            }
         }

        else if (FD_ISSET(tcp_server->dummy_master_skt_fd, &tcp_server->active_client_fds)){

            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            int comm_socket_fd = accept(this->dummy_master_skt_fd,
                                                (struct sockaddr *)&client_addr, &addr_len);
            close(comm_socket_fd);

            tcp_server_operations_t opn = server_pending_operation;
            switch (opn)
            {
            case tcp_server_operation_none:
                break;
            case tcp_server_stop_listening_client:
                rc = TcpServerChangeState(pending_tcp_client, opn);
                pending_tcp_client = NULL;
                break;
            case tcp_server_resume_listening_client:
                rc = TcpServerChangeState(pending_tcp_client, opn);
                pending_tcp_client = NULL;
                 break;
             case tcp_server_stop_accepting_new_connections:
                rc = TcpServerChangeState(NULL, opn);
                 break;
             case tcp_server_resume_accepting_new_connections:
                rc = TcpServerChangeState(NULL, opn);
                 break;
             case tcp_server_close_client_connection:
                rc = TcpServerChangeState(pending_tcp_client, opn);
                pending_tcp_client = NULL;
                if (rc) max_fd = GetMaxFd();
                 break;
            case tcp_server_force_close_client_connection:
                rc = TcpServerChangeState(pending_tcp_client, opn);
                pending_tcp_client = NULL;
                if (rc) max_fd = GetMaxFd();
                break;
            case tcp_server_shut_down:
                rc = TcpServerChangeState(NULL, opn);
                pthread_exit(NULL);
             case tcp_server_operations_max:
             default:;
             }
              server_pending_operation = tcp_server_operation_none;
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
                        break;
                    }

                    if (tcp_client->tcp_conn->bytes_recvd &&
                         tcp_server->tcp_notif &&
                         tcp_server->tcp_notif->client_msg_recvd) {
                        tcp_server->tcp_notif->client_msg_recvd(tcp_client, 
                                tcp_client->tcp_conn->recv_buffer,
                                tcp_client->tcp_conn->bytes_recvd);
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
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    sem_init(&this->semaphore_wait_for_thread_start, 0, 0);
    pthread_create(&this->server_thread, &attr, tcp_server_fn, (void *)this);
    sem_wait(&this->semaphore_wait_for_thread_start);

    if (this->master_skt_fd < 0 ||
        this->dummy_master_skt_fd < 0) {

        printf ("Error : Could not start TCP Server\n");
        return;
    }

    printf ("TcpServer Thread Created Successfully\n");
    TcpServerSetState(TCP_SERVER_STATE_LISTENING);
}

void
TcpServer::Stop() {

    /* 
    1. Disconnect and Free all connected clients 
    2. Cancel Server thread
    3. Close Master and dummy skt FD
    4.  Free Server
    */
   //AbortAllClients();
   if (this->n_clients_connected) {
       this->server_pending_operation = tcp_server_shut_down;
       TcpSelfConnect();
       pthread_join(this->server_thread, NULL);
   }
   else {
       pthread_cancel(this->server_thread);
   }
  
   close (this->dummy_master_skt_fd);
   close (this->master_skt_fd);
   delete this->tcp_notif;
   sem_destroy(&this->semaphore_wait_for_thread_start);
   assert(!this->pending_tcp_client);
   assert(this->tcp_client_conns.empty());
   if (this->ka_timer) {
       delete_timer(this->ka_timer);
       this->ka_timer = NULL;
   }
   delete this;
}

void
TcpServer::TcpServerSetState(uint8_t flag) {

    this->tcp_server_state_flags |= flag;
}
void
TcpServer::TcpServerClearState(uint8_t flag) {

    this->tcp_server_state_flags &= (~flag);
}
uint8_t
TcpServer::TcpServerGetStateFlag() {

    return this->tcp_server_state_flags;
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
    if (this->server_pending_operation != tcp_server_shut_down) {
        sem_wait(&semaphore_wait_for_thread_start);
    }
    printf ("Client disconnection successful\n");

    close(sock_fd);
}

void 
TcpServer::AbortClient(TcpClient *tcp_client) {

    assert (tcp_client->tcp_conn->comm_sock_fd);
    assert(tcp_client->tcp_conn->conn_state == TCP_ESTABLISHED);
    assert(tcp_client->tcp_conn->conn_origin== tcp_via_accept);
    this->pending_tcp_client = tcp_client;
    tcp_client->ref_count++;
    this->server_pending_operation = tcp_server_force_close_client_connection;
    this->TcpSelfConnect();
}

void
TcpServer::AbortAllClients() {

        std::list<TcpClient *>::iterator it;
        TcpClient *next_tcp_client, *curr_tcp_client;
        for (it = tcp_client_conns.begin() ; it  != tcp_client_conns.end(); *it = next_tcp_client) {
            curr_tcp_client = *it;
            if (!curr_tcp_client) return;
            next_tcp_client = *(++it);
            AbortClient(curr_tcp_client);
        }
}

void 
TcpServer::RegisterClientDisConnectCbk (void (*cbk)(const TcpClient*)) {

    if (!this->tcp_notif) {
        this->tcp_notif = new TcpServerNotification();
    }
    this->tcp_notif->client_disconnected = cbk;
}

void 
TcpServer::RegisterClientConnectCbk (void (*cbk)(const TcpClient*)) {

    if (!this->tcp_notif) {
        this->tcp_notif = new TcpServerNotification();
    }
    this->tcp_notif->client_connected = cbk;
}

void 
TcpServer::RegisterClientMsgRecvCbk (void (*cbk)(const TcpClient*, unsigned char *, uint16_t)) {

    if (!this->tcp_notif) {
        this->tcp_notif = new TcpServerNotification();
    }
    this->tcp_notif->client_msg_recvd = cbk;
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
         tcp_client->tcp_server = NULL;
    }

bool
TcpServer::TcpServerChangeState(
        TcpClient *tcp_client, tcp_server_operations_t opn) {

    bool rc = false;
    
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
        TcpServerClearState(TCP_SERVER_STATE_ACCEPTING_CONNECTIONS);
        rc = true;
        break;
        case tcp_server_resume_accepting_new_connections:
        TcpServerSetState(TCP_SERVER_STATE_ACCEPTING_CONNECTIONS);
        rc = true;
        break;
        case tcp_server_close_client_connection:
        {
             assert(tcp_client);
             assert(tcp_client->tcp_conn->comm_sock_fd);
             FD_CLR (tcp_client->tcp_conn->comm_sock_fd, &this->backup_client_fds);
             remove_from_fd_array(tcp_client->tcp_conn->comm_sock_fd);
             if (tcp_notif &&  tcp_notif->client_disconnected) {
                tcp_notif->client_disconnected(tcp_client);
             }
             RemoveTcpClient(tcp_client);
             tcp_client->TcpClientAbort();
             n_clients_connected--;
             sem_post(&semaphore_wait_for_thread_start);
             rc = true;
        }
        break;
        case tcp_server_force_close_client_connection:
             assert(tcp_client);
             assert(tcp_client->tcp_conn->comm_sock_fd);
             FD_CLR (tcp_client->tcp_conn->comm_sock_fd, &this->backup_client_fds);
             remove_from_fd_array(tcp_client->tcp_conn->comm_sock_fd);
             if (tcp_notif &&  tcp_notif->client_disconnected) {
                tcp_notif->client_disconnected(tcp_client);
             }
             RemoveTcpClient(tcp_client);
             tcp_client->TcpClientAbort();
             n_clients_connected--;
             sem_post(&semaphore_wait_for_thread_start);
             rc = true;
             break;
        case tcp_server_shut_down:
        {
            std::list<TcpClient *>::iterator it;
            TcpClient *next_tcp_client, *curr_tcp_client;
            int i = this->n_clients_connected;
            for (it = tcp_client_conns.begin(); it != tcp_client_conns.end(); *it = next_tcp_client)
            {
                curr_tcp_client = *it;
                if (!curr_tcp_client)
                    break;
                next_tcp_client = *(++it);

                assert(curr_tcp_client->tcp_conn->comm_sock_fd);
                if (!curr_tcp_client->client_thread) {
                    FD_CLR(curr_tcp_client->tcp_conn->comm_sock_fd, &this->backup_client_fds);
                    remove_from_fd_array(curr_tcp_client->tcp_conn->comm_sock_fd);
                }
                if (tcp_notif && tcp_notif->client_disconnected)
                {
                    tcp_notif->client_disconnected(tcp_client);
                }
                RemoveTcpClient(curr_tcp_client);
                curr_tcp_client->TcpClientAbort();
                n_clients_connected--;
            }
            printf ("No of clients Disconnected = %d\n", i);
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

void
TcpServer::Display() {

    TcpClient *tcp_client;
    std::list<TcpClient *>::iterator it;
    TcpServer *tcp_server = this;

    printf ("%s  [ %s , %d, %d ]\n", tcp_server->name.c_str(),
                network_covert_ip_n_to_p(tcp_server->self_ip_addr, NULL),
                tcp_server->master_skt_fd,
                tcp_server->dummy_master_skt_fd);

    printf (" State flags : 0x%x\n", tcp_server->TcpServerGetStateFlag());
    printf (" # Connected Clients : %d\n", tcp_server->n_clients_connected);

    for ( it = tcp_server->tcp_client_conns.begin(); 
            it != tcp_server->tcp_client_conns.end(); ++it) {

        tcp_client = *it;
        printf ("Client : \n");
        tcp_client->Display();
        printf ("\n\n");
    }
}

void
 TcpServer::MultiThreadedClientThreadServiceFn(TcpClient *tcp_client) {

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    TcpServer *tcp_server = tcp_client->tcp_server;

    fd_set client_fd;
    FD_ZERO (&client_fd);

    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, NULL);

    if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_connected) {
        tcp_server->tcp_notif->client_connected(tcp_client);
    }

    sem_post(&tcp_server->semaphore_wait_for_thread_start);

    while(true) {

        FD_CLR(tcp_client->tcp_conn->comm_sock_fd, &client_fd);
        FD_SET(tcp_client->tcp_conn->comm_sock_fd, &client_fd);
        printf ("client blocked on select()\n");
        select(tcp_client->tcp_conn->comm_sock_fd + 1, &client_fd, NULL, NULL, NULL);
        pthread_testcancel();

        tcp_client->tcp_conn->bytes_recvd =
                        recvfrom(tcp_client->tcp_conn->comm_sock_fd,
                                        tcp_client->tcp_conn->recv_buffer,
                                        TCP_CONN_RECV_BUFFER_SIZE,
                                        0, (struct sockaddr *)&client_addr, &addr_len);

        if (tcp_client->tcp_conn->bytes_recvd == 0) {

            printf("Printf Client abrupt termination\n");

            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_disconnected) {
                tcp_server->tcp_notif->client_disconnected(tcp_client);
            }
            
            tcp_server->RemoveTcpClient(tcp_client);
            close (tcp_client->tcp_conn->comm_sock_fd);
            delete tcp_client->tcp_conn;
            tcp_client->tcp_conn = NULL;
            free(tcp_client->client_thread);
            tcp_client->client_thread = NULL;
            delete tcp_client;
            tcp_server->n_clients_connected--;
            break;
        }
        else if (tcp_client->tcp_conn->bytes_recvd &&
                    tcp_server->tcp_notif &&
                    tcp_server->tcp_notif->client_msg_recvd) {

            tcp_server->tcp_notif->client_msg_recvd(tcp_client, 
                                tcp_client->tcp_conn->recv_buffer,
                                tcp_client->tcp_conn->bytes_recvd);
         }
    }
}

void *
tcp_client_thread_fn(void *arg) {

    TcpClient *tcp_client = (TcpClient *)arg;
    tcp_client->tcp_server->MultiThreadedClientThreadServiceFn(tcp_client);
    return NULL;
}

void
TcpServer::TcpServerCreateMultithreadedClient(
                uint16_t comm_fd,
                struct sockaddr_in *client_addr) {

    TcpClient *tcp_client = new TcpClient();
    tcp_client->tcp_server = this;
    TcpConn *tcp_conn = new TcpConn();
    tcp_conn->comm_sock_fd = comm_fd;
    tcp_conn->conn_state = TCP_ESTABLISHED;
    tcp_client->tcp_server->n_clients_connected++;
    tcp_conn->conn_origin = tcp_via_accept;
    tcp_conn->peer_addr = htonl(client_addr->sin_addr.s_addr);
    tcp_conn->peer_port_no = htons(client_addr->sin_port);
    tcp_conn->self_port_no = this->self_port_no;
    tcp_conn->self_addr = this->self_ip_addr;
    tcp_client->tcp_conn = tcp_conn;
    tcp_client->StartExpirationTimer();
   this->tcp_client_conns.push_back(tcp_client);
   tcp_client->client_thread = (pthread_t *)calloc ( 1, sizeof (pthread_t));
   pthread_create (tcp_client->client_thread, NULL, tcp_client_thread_fn, (void *)tcp_client);
   sem_wait(&this->semaphore_wait_for_thread_start);
}

void TcpServer::TcpServer_KA_Dispatch_fn() {

    std::list<TcpClient *>::iterator it;
    TcpClient *tcp_client;
    for (it = this->tcp_client_conns.begin(); it != this->tcp_client_conns.end(); ++it) {
        tcp_client = *it;
        tcp_client->tcp_conn->SendMsg(this->ka_msg, sizeof(this->ka_msg));
    }
}

static void
TcpServer_KA_Dispatch_fn_internal(Timer_t *timer, void *arg) {

    TcpServer *tcp_server = (TcpServer *)arg;
    tcp_server->TcpServer_KA_Dispatch_fn();
}

void
TcpServer::Apply_ka_interval() {

    if (this->ka_timer && is_timer_running(this->ka_timer)) {
       delete_timer(this->ka_timer);
       this->ka_timer = NULL;
    }

    if (this->ka_interval == 0) return;

    this->ka_timer = setup_timer(TcpServer_KA_Dispatch_fn_internal, 
                                this->ka_interval * 1000,
                                this->ka_interval * 1000,
                                0, (void *)this,
                                false);

    start_timer(this->ka_timer);
    assert(this->ka_timer);
}

/* TcpClient */
TcpClient::TcpClient() {

    listen_by_server = true;
}

TcpClient::~TcpClient() {

    assert(!ref_count);
}

void
TcpClient::Display() {

    printf ("retry timer interval = %d\n", retry_time_interval);
    printf ("ref count = %d\n", ref_count);
    if (client_thread) {
        printf ("listen by server : Multi-threaded\n");
    }
    else {
        printf ("listen by server : %s\n", listen_by_server ? "yes" : "no");
    }
    printf ("client thread = %p\n", client_thread);
    
    printf ("Expiration Timer Remaining (in msec )= %lu\n", 
        timer_get_time_remaining_in_mill_sec(this->expiration_timer));

    printf ("Tcp Conn = %p\n", tcp_conn);
    if (tcp_conn) {
        printf ("Conn : \n");
        tcp_conn->Display();
    }
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
    if (this->client_thread) {
        pthread_cancel(*this->client_thread);
        free(this->client_thread);
        this->client_thread = NULL;
    }
    else {
         ref_count--;
    }
    if (this->expiration_timer) {
        delete_timer(this->expiration_timer);
        this->expiration_timer = NULL;
    }
    if (ref_count == 0) {
        delete this;
    }
}

static void
tcp_client_expiration_timer_expired(Timer_t *timer, void *arg) {

    TcpClient *tcp_client = (TcpClient *)arg;
    TcpServer *tcp_server = tcp_client->tcp_server;
    tcp_server->AbortClient(tcp_client);
}

void
TcpClient::StartExpirationTimer() {
    
    if (this->expiration_timer) return;

    this->expiration_timer = setup_timer(tcp_client_expiration_timer_expired,
                                                TCP_HOLD_DOWN_TIMER * 1000,
                                                TCP_HOLD_DOWN_TIMER * 1000,
                                                0,
                                                (void *)this, false);

    assert(this->expiration_timer);
    start_timer(this->expiration_timer);
}

void TcpClient::CancelExpirationTimer() {

        if (!this->expiration_timer) return;
        if (!is_timer_running(this->expiration_timer)) return;
        cancel_timer(this->expiration_timer);
}

void TcpClient::DeleteExpirationTimer() {

    if (!this->expiration_timer) return;
    delete_timer(this->expiration_timer);
    this->expiration_timer = NULL;
}

void TcpClient::ReStartExpirationTimer() {
    
    assert(this->expiration_timer);
    restart_timer(this->expiration_timer);
}
