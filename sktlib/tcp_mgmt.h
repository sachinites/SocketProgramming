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
#include <list>
#include <string>
#include "network_utils.h"

#define MAX_CLIENT_SUPPORTED    100
#define TCP_CONN_RECV_BUFFER_SIZE    1028
#define TCP_DEFAULT_PORT_NO 40000

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

typedef enum {

    tcp_server_operation_none,
    tcp_server_stop_listening_client,
    tcp_server_resume_listening_client,
    tcp_server_stop_accepting_new_connections,
    tcp_server_resume_accepting_new_connections,
    tcp_server_close_client_connection,
    tcp_server_operations_max
} tcp_server_operations_t;

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
    void Display();
};

class TcpClient {

    public:
    TcpConn *tcp_conn;
    timer_t re_try_timer;
    uint8_t retry_time_interval;
    uint8_t ref_count;
    bool listen_by_server;
    
    /* Methods */
    TcpClient();
    ~TcpClient();
    bool TcpClientConnect(uint32_t server_ip, uint16_t server_port);  
    void TcpClientDisConnect();
    void TcpClientAbort();
    void Display();
};


class TcpServerNotification {

    public:
    void (*client_connected)(const TcpClient*);
    void (*client_disconnected)(const TcpClient*);
    void (*client_msg_recvd)(const TcpClient*, unsigned char *, uint16_t);
};

typedef enum {

    TCP_SERVER_STATE_LISTENING = 1,
    TCP_SERVER_STATE_ACCEPTING_CONNECTIONS = 2,
    TCP_SERVER_STATE_MAX
} tsp_server_state_t;

class TcpServer {

    private:
        int16_t fd_array[MAX_CLIENT_SUPPORTED + 2]; 
        uint16_t GetMaxFd();
        void add_to_fd_array(uint16_t fd);
        void remove_from_fd_array(uint16_t fd);
        int n_clients_connected;
        uint32_t dummy_master_skt_fd;
        pthread_t server_thread;
        TcpServerNotification *tcp_notif;
        sem_t semaphore_wait_for_thread_start;
        sem_t semaphore_wait_for_client_operation_complete;
        uint8_t tcp_server_state_flags;
        fd_set active_client_fds;
        fd_set backup_client_fds;
        tcp_server_operations_t server_pending_operation;
        TcpClient *pending_tcp_client;
        bool TcpServerChangeState(TcpClient *, tcp_server_operations_t);
        void TcpServerSetState(uint8_t flag);
        void TcpServerClearState(uint8_t flag);\
        uint8_t TcpServerGetStateFlag();

    public:
        std::string name;
        uint32_t self_ip_addr;
        uint16_t self_port_no;
        std::list<TcpClient *> tcp_client_conns;
        uint32_t master_skt_fd;

        /* Methods */
        TcpServer();
        ~TcpServer();
        void *TcpServerThreadFn();
        void Display();
        void StopAcceptingNewConnections();
        void StopListeningAllClients();
        void ResumeListeningAllClients();
        void StopListeningClient();
        void ResumeListeningClient();
        TcpServer(const uint32_t &self_ip_addr, const uint16_t &self_port_no, std::string name);
        TcpClient *GetTcpClientbyFd(uint16_t fd);
        TcpClient *GetTcpClient(uint32_t ip_addr, uint16_t port_no);
        void RemoveTcpClient(TcpClient *tcp_client);
        void Start();
        void TcpSelfConnect();
        void ForceDisconnectAllClients();
        void DiscoconnectClient(TcpClient *);
        void Stop();
        void RegisterClientConnectCbk(void (*)(const TcpClient *));
        void RegisterClientDisConnectCbk(void (*)(const TcpClient *));
        void RegisterClientMsgRecvCbk(void (*)(const TcpClient *, char *, uint16_t));
        void Cleanup();
}; 

#endif 
