#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h> // for IPPROTO_TCP
#include <memory.h>
#include "tcp_client.h"
#include "tcp_server.h"
#include "tcp_conn.h"

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
    
    if (tcp_client->tcp_conn->conn_state == TCP_ESTABLISHED) {
        tcp_client->tcp_conn->conn_state = TCP_KA_PENDING;
        tcp_client->CancelExpirationTimer();
        tcp_client->DeleteExpirationTimer();
        tcp_client->expiration_timer = setup_timer(tcp_client_expiration_timer_expired,
                                                                    TCP_CLIENT_HOLD_DOWN_TIMER * 1000,
                                                                    TCP_CLIENT_HOLD_DOWN_TIMER * 1000,
                                                                    0, (void *)tcp_client, false);
        start_timer(tcp_client->expiration_timer);
    }
    else if (tcp_client->tcp_conn->conn_state == TCP_KA_PENDING) {
        tcp_server->AbortClient(tcp_client);
    }
    else {
        assert(0);
    }
}

void
TcpClient::StartExpirationTimer() {
    
    if (this->expiration_timer) return;

    this->expiration_timer = setup_timer(tcp_client_expiration_timer_expired,
                                                TCP_CLIENT_EXPIRATION_TIMER * 1000,
                                                TCP_CLIENT_EXPIRATION_TIMER * 1000,
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
