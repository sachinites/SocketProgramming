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
#include "tcp_server.h"
#include "tcp_client.h"
#include "tcp_conn.h"

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


/* Default Constructor */
TcpServer::TcpServer() {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for ( i = 0 ; i < array_size; i++) {
        this->fd_array[i] = -1;
    }

    this->self_ip_addr = 2130706433; // 127.0.0.1
    this->self_port_no = TCP_SERVER_DEFAULT_PORT_NO;
    sem_init(&this->semaphore_wait_for_operation_complete, 0 , 0);
    this->name = "Default";
    memcpy(this->ka_msg, "Default KA Msg\0", strlen("Default KA Msg\0"));
}

/* Non-default Constructor */
TcpServer::TcpServer(const uint32_t& self_ip_addr,
                                    const uint16_t& self_port_no,
                                    std::string name) {

    int i;
    int array_size = sizeof(this->fd_array) / sizeof(this->fd_array[0]);

    for (i = 0; i < array_size; i++)
    {
        this->fd_array[i] = -1;
    }

    this->self_ip_addr = self_ip_addr;
    this->self_port_no = self_port_no;
    sem_init(&this->semaphore_wait_for_operation_complete, 0 , 0);
    this->name = name;
    memcpy(this->ka_msg, "Default KA Msg\0", strlen("Default KA Msg\0"));
}

/* In Destructors, check that TCPServer has really released all resources or not */
TcpServer::~TcpServer() { 

    assert(!this->master_skt_fd);
    assert(!this->dummy_master_skt_fd);
    assert(!this->n_clients_connected);
    assert(!this->tcp_notif);
    assert(this->server_pending_operation == tcp_server_operation_none);
    assert(!this->pending_tcp_client);
    assert(!this->ka_timer);
    assert(this->tcp_client_conns.empty());
    printf ("%s TcpServer shut-down successfully\n", this->name.c_str());
}

void *
TcpServer::TcpServerThreadFn() {

  int opt = 1;
  bool rc;

    TcpServer *tcp_server = this;
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, NULL);

    tcp_server->master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );
    tcp_server->dummy_master_skt_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if( tcp_server->master_skt_fd == -1 ||
        tcp_server->dummy_master_skt_fd == -1){

        printf("TCP Server Socket Creation Failed\n");
        /* Signal the main routine that thread routine fn has started */
         sem_post(&tcp_server->semaphore_wait_for_operation_complete);
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

     sem_post(&tcp_server->semaphore_wait_for_operation_complete);

    while (true) {

       memcpy(&tcp_server->active_client_fds,
                      &tcp_server->backup_client_fds, sizeof(fd_set));

         printf ("\nServer blocked on select\n");
         select( max_fd +1 , &tcp_server->active_client_fds, NULL, NULL, NULL);
         pthread_testcancel();

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
            if (tcp_server->TcpServerGetStateFlag() &
                   TCP_SERVER_STATE_SET_AUTO_CLIENT_DISCONNECTION ) {
                tcp_client->StartExpirationTimer();
            }
            tcp_server->tcp_client_conns.push_back(tcp_client);
            /* Now send Connect notification */
            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_connected) {
                    tcp_server->tcp_notif->client_connected (tcp_client);
            }
         }

        if (FD_ISSET(tcp_server->dummy_master_skt_fd, &tcp_server->active_client_fds)){

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
                assert(!pending_tcp_client);
                 break;
             case tcp_server_resume_accepting_new_connections:
                rc = TcpServerChangeState(NULL, opn);
                assert(!pending_tcp_client);
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
            case tcp_server_force_close_all_client_connections:
                rc  = TcpServerChangeState(NULL, opn);
                break;
            case tcp_server_send_ka_msg_all_clients:
                rc  = TcpServerChangeState(NULL, opn);
                break;
            case tcp_server_shut_down:
                rc = TcpServerChangeState(NULL, opn);
                assert(!pending_tcp_client);
                server_pending_operation = tcp_server_operation_none;
                pthread_exit(NULL);
             case tcp_server_operations_max:
             default:;
             }
              server_pending_operation = tcp_server_operation_none;
         }

         {
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

    sem_init(&this->semaphore_wait_for_operation_complete, 0, 0);
    pthread_create(&this->server_thread, &attr, tcp_server_fn, (void *)this);
    sem_wait(&this->semaphore_wait_for_operation_complete);

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

   this->StopAcceptingNewConnections();
   this->StopListeningAllClients();

   if (this->n_clients_connected) {
       this->server_pending_operation = tcp_server_shut_down;
       TcpSelfConnect();
       pthread_join(this->server_thread, NULL);
   }
   else {
       pthread_cancel(this->server_thread);
   }
  
   close (this->dummy_master_skt_fd);
   this->dummy_master_skt_fd = 0;
   close (this->master_skt_fd);
   this->master_skt_fd = 0;
   delete this->tcp_notif;
   sem_destroy(&this->semaphore_wait_for_operation_complete);
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
    
    if (this->server_pending_operation == tcp_server_shut_down ||
        this->server_pending_operation == tcp_server_send_ka_msg_all_clients) {
        
    }
    else {
         printf ("Waiting for client disconnection\n");
         sem_wait(&this->semaphore_wait_for_operation_complete);
    }
    printf ("Client disconnection successful\n");

    close(sock_fd);
}

void 
TcpServer::AbortClient(TcpClient *tcp_client) {

    assert (tcp_client->tcp_conn->comm_sock_fd);
    assert(tcp_client->tcp_conn->conn_origin== tcp_via_accept);
    this->pending_tcp_client = tcp_client;
    tcp_client->ref_count++;
    this->server_pending_operation = tcp_server_force_close_client_connection;
    this->TcpSelfConnect();
}

void
TcpServer::AbortAllClients() {

        this->server_pending_operation = tcp_server_force_close_all_client_connections;
        this->TcpSelfConnect();
}

void
TcpServer::StopListeningAllClients() {

}

void
TcpServer::StopAcceptingNewConnections() {

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
             sem_post(&semaphore_wait_for_operation_complete);
             rc = true;
        }
        break;
        case tcp_server_force_close_client_connection:
             assert(tcp_client);
             assert(tcp_client->tcp_conn->comm_sock_fd);
             if (!tcp_client->client_thread) {
                FD_CLR (tcp_client->tcp_conn->comm_sock_fd, &this->backup_client_fds);
                remove_from_fd_array(tcp_client->tcp_conn->comm_sock_fd);
            }
             if (tcp_notif &&  tcp_notif->client_disconnected) {
                tcp_notif->client_disconnected(tcp_client);
             }
             RemoveTcpClient(tcp_client);
             tcp_client->TcpClientAbort();
             n_clients_connected--;
             sem_post(&semaphore_wait_for_operation_complete);
             rc = true;
             break;
        case tcp_server_force_close_all_client_connections:
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
            if (opn == tcp_server_force_close_all_client_connections) {
                sem_post(&this->semaphore_wait_for_operation_complete);
            }
        }
        break;
        case  tcp_server_send_ka_msg_all_clients:
        {
            TcpClient *tcp_client;
            std::list<TcpClient *>::iterator it;
            for (it = tcp_client_conns.begin(); it != tcp_client_conns.end(); ++it) {
                tcp_client = *it;
                tcp_client->tcp_conn->SendMsg(this->ka_msg, sizeof(this->ka_msg));
            }
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

    sem_post(&tcp_server->semaphore_wait_for_operation_complete);

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

            if (tcp_server->tcp_notif && tcp_server->tcp_notif->client_disconnected) {
                tcp_server->tcp_notif->client_disconnected(tcp_client);
            }
            
            tcp_server->AbortClient(tcp_client);
            pthread_testcancel();
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
    if (this->TcpServerGetStateFlag() &
         TCP_SERVER_STATE_SET_AUTO_CLIENT_DISCONNECTION ) {
        tcp_client->StartExpirationTimer();
    }
   this->tcp_client_conns.push_back(tcp_client);
   tcp_client->client_thread = (pthread_t *)calloc ( 1, sizeof (pthread_t));
   pthread_create (tcp_client->client_thread, NULL, tcp_client_thread_fn, (void *)tcp_client);
   sem_wait(&this->semaphore_wait_for_operation_complete);
}

void TcpServer::TcpServer_KA_Dispatch_fn() {

    this->server_pending_operation = tcp_server_send_ka_msg_all_clients;
    this->TcpSelfConnect();
}

static void
TcpServer_KA_Dispatch_fn_internal(Timer_t *timer, void *arg) {

    TcpServer *tcp_server = (TcpServer *)arg;
    if (tcp_server->tcp_client_conns.empty()) return;
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