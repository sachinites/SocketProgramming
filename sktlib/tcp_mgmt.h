/*
 * =====================================================================================
 *
 *       Filename:  tcp_mgmt.h
 *
 *    Description: This file implements the mgmt of TCP connection 
 *
 *        Version:  1.0
 *        Created:  01/24/2022 11:34:11 AM
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  ABHISHEK SAGAR (), sachinites@gmail.com
 *   Organization:  Juniper Networks
 *
 * =====================================================================================
 */

#ifndef __TCP__CONN_MGMT__
#define __TCP__CONN_MGMT__

#include <stdint.h>
#include <vector> 
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_CLIENT_SUPPORTED    100
#define TCP_CONN_RECV_BUFFER_SIZE    1028

typedef enum {

    TCP_DISCONNECTED,
    TCP_KA_PENDING,
    TCP_PEER_CLOSED,
    TCP_ESTABLISHED
} tcp_conn_state_t;

typedef enum {

    tcp_via_accept,
    tcp_via_connect
} conn_origin_type_t; 

class TcpConn {

    public:
    /* Communication file descriptor for this connection */
    uint16_t comm_sock_fd;
    /* State of this Connection */
    tcp_conn_state_t conn_state;
    /* How this Connection was Created */
    conn_origin_type_t conn_origin;
    /* Peer Address */
    uint32_t peer_addr;
    /* peer port_no */
    uint16_t peer_port_no;
    /* self port no */
    uint16_t self_port_no;
    /* Self IP Addr */
    uint32_t self_addr;
    /* time in sec to send KA msgs, if 0 then KA are not sent on
    this connection */
    uint16_t ka_interval;
    /* KA recvd */
    uint32_t ka_recvd;
    /* KA sent */
    uint32_t ka_sent;
    /* KA sending periodic timer */
    timer_t KA_send_timer;
    /* KA expiration timer */
    timer_t ka_expiration_timer;
    /* Recv Buffer */
    unsigned char recv_buffer[TCP_CONN_RECV_BUFFER_SIZE];
    /* Bytes recvd in recv_buffer */
    uint16_t bytes_recvd;

    TcpConn();
    ~TcpConn();

    void SetKaSendInterval(uint16_t ka_interval_in_sec);
    void SetExpirationInterval(uint16_t exp_interval_in_sec);
    void Abort();
    uint32_t SendMsg(char *msg, uint32_t msg_size);
};

class TcpClient {

    public:
    TcpConn *tcp_conn;
    timer_t re_try_timer;
    uint8_t retry_time_interval;
    
    /* Methods */
    TcpClient();
    bool TcpClientConnect(uint32_t server_ip, uint16_t server_port);  
    void TcpClientDisConnect();
    void TcpClientAbort();
};


class TcpServerNotification {

    public:
    void (*client_connected)(const TcpClient*);
    void (*client_disconnected)(const TcpClient*);
    void (*client_msg_recvd)(const TcpClient*, char *, uint16_t);
};

class TcpServer {

    private:
        int16_t fd_array[MAX_CLIENT_SUPPORTED + 2]; 
        uint16_t GetMaxFd();
        int n_clients_connected;
        void add_to_fd_array(uint16_t fd);
        void remove_from_fd_array(uint16_t fd);
        uint32_t dummy_master_skt_fd;
        pthread_t server_thread;
        TcpServerNotification *tcp_notif;
        sem_t thread_start_semaphore;
        sem_t wait_for_client_disconnection;
        fd_set active_client_fds;
        fd_set backup_client_fds;
        uint16_t tcp_client_fd_pending_diconnect;

    public:
    uint32_t self_ip_addr;
    uint16_t self_port_no;
    std::vector <TcpClient *> tcp_client_conns;
    uint32_t master_skt_fd;
    
    /* Methods */
    TcpServer();
    ~TcpServer();
    void* TcpServerThreadFn();
    TcpServer(const uint32_t& self_ip_addr, const uint16_t& self_port_no);
    TcpClient *GetTcpClientbyFd(uint16_t fd, uint16_t *pos);
    TcpClient *GetTcpClient(uint32_t ip_addr, uint16_t port_no);
    void RemoveTcpClient (TcpClient *tcp_client);
    void Start();
    void TcpSelfConnect();
    void ForceDisconnectAllClients();
    void DiscoconnectClient(TcpClient *);
    void Stop();
    void RegisterClientConnectCbk (void (*)(const TcpClient*));
    void RegisterClientDisConnectCbk (void (*)(const TcpClient*));
    void RegisterClientMsgRecvCbk (void (*)(const TcpClient*, char *, uint16_t));
    void Cleanup();
}; 

#endif 
