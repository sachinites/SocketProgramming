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
};

class TcpServerNotification {

    public:
    void (*client_connected)(const TcpConn&);
    void (*client_disconnected)(const TcpConn&);
    void (*client_msg_recvd)(const TcpConn&, char *, uint16_t);
};

class TcpServer {

    public:
    uint32_t self_ip_addr;
    uint16_t self_port_no;
    std::vector <TcpConn> tcp_client_conns;
    pthread_t server_thread;
    TcpServerNotification *tcp_notif;
    sem_t thread_start_semaphore;
    uint32_t master_skt_fd;
    uint32_t dummy_master_skt_fd;
    fd_set active_client_fds;
    fd_set backup_client_fds;
    
    /* Methods */
    TcpServer();
    ~TcpServer();
    TcpServer(const uint32_t& self_ip_addr, const uint16_t& self_port_no);
    void Start();
    void ForceDisconnectAllClients();
    void DiscoconnectClient(const uint32_t& client_ip, const uint16_t& client_port_no);
    void Stop();
    void RegisterClientConnectCbk (void (*)(const TcpConn&));
    void RegisterClientDisConnectCbk (void (*)(const TcpConn&));
    void RegisterClientMsgRecvCbk (void (*)(const TcpConn&, char *, uint16_t));
    void Cleanup();
}; 

class TcpClient {

    public:
    TcpConn tcp_conn;
    timer_t re_try_timer;
    uint8_t retry_time_interval;
    
    /* Methods */
    void connect();
    void disconnect();
    void shutdown();
};
#endif 
