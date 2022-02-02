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
#include "timerlib.h"
#include "network_utils.h"

/* Maximum no of clients which can be connected to out TCP Server */
#define TCP_SERVER_MAX_CLIENT_SUPPORTED    100
/* Default port on which our TCP Server would listen for new client connection
requests ( handshake )*/
#define TCP_SERVER_DEFAULT_PORT_NO                 40000

/* Operation to be performed on a TCP Server */
typedef enum {

    tcp_server_operation_none,
    /* This action enables TCP Server to Stop listening to client's
    data request, and silently ignores all of them. Note that it dont stop
    TCP server from responding to new client's handshake requests*/
    tcp_server_stop_listening_client,
    /* This action allows TCP Server to resume responding to client's data request */
    tcp_server_resume_listening_client,
    /* This action enables TCP server to stop entertaining any more new handshake
    requests, but continue to serve existing TCP clients */
    tcp_server_stop_accepting_new_connections,
    /* This action enable TCP Server to resume acceoting new client's connection requests */
    tcp_server_resume_accepting_new_connections,
    /* This action allows TCP sever to close client's connection and abort the TCP client
    when FIN packet arrives from TCP client. Tis is called active termination of client by
    TCPServer */
    tcp_server_close_client_connection,  // initiated by client
    /* This action allows TCP sever to close client's connection and abort the TCP client
    forcefully i.e. even when the client has not send any FIN request. This action is initiated
    by server itself. This is called Passive termination of client by TCP Server*/
    tcp_server_force_close_client_connection, // initiated by Tcp server
    /* This action allows TCP Server to force close all connected TCP clients i.e. even
    when no client has send any FIN request. This action is initiated
    by server itself. This is called Passive termination of clients by TCP Server*/
    tcp_server_force_close_all_client_connections,
    /* This actions allows TCP Server to send KA msg for all connected clients */
    tcp_server_send_ka_msg_all_clients,
    /* This action allows TCP Server to abort all its clients, free up all its resources ( threads,     FDs, Sockets etc) and eventually abort the Server completely */
    tcp_server_shut_down,
    tcp_server_operations_max
} tcp_server_operations_t;

/* Forward Declaration */
class TcpClient;

class TcpServerNotification {

    public:
    /* Used by TCP Server to notify the application that some client
    is connected to TCP Server */
    void (*client_connected)(const TcpClient*);
     /* Used by TCP Server to notify the application that some client
    has dis-connected from TCP Server - either actively or passively*/
    void (*client_disconnected)(const TcpClient*);
      /* Used by TCP Server to notify the application that some client
    has send some message over TCP Connection to TCPServer. Appln
    must process this msg and take action accordingly*/
    void (*client_msg_recvd)(const TcpClient*, unsigned char *, uint16_t);
    /* Used by TCP Server to notify the application that some client
    has transitioned to KA pending state, meaning this client connection is
    about to terminate due to non-reception of KA msgs from client*/
    void (*client_ka_pending)(const TcpClient*);
};

/* Tracking TCP Server States */
typedef enum {

    /* TCPServer is said to be in this state when it is created but has not yet
    started listening for new client's connection requests (SYN) packets */
    TCP_SERVER_STATE_IDLE = 1,
    /* TCPServer is said to be in this state when TCPServer has started listening on
    Client's Data Requests */
    TCP_SERVER_STATE_LISTENING = 2,
    /* TCPServer is said to be in this state when TCPServer has started listening
    on client's new connection initiation requests (SYN) pkts from clients */
    TCP_SERVER_STATE_ACCEPTING_CONNECTIONS = 4,
    /* Setting this state would allow TCP Server to fork new thread for each future
    clients which connects to TCP Server */
    TCP_SERVER_STATE_MULTITHREADED_MODE = 8,
    TCP_SERVER_STATE_MAX
} tsp_server_state_t;

class TcpServer {

    private:
       /* Array to store FDs of all connected clients to TCPServer. This Array is required to
       computed the Max-FD among all. Max FD is required to work with select( ) */
        int16_t fd_array[TCP_SERVER_MAX_CLIENT_SUPPORTED + 2]; 
       /* Fn to compute Max FD from above Array. Simple fn to find maximum integer in an array */
        uint16_t GetMaxFd();
        /* Fn to add a new FD to the above array */
        void add_to_fd_array(uint16_t fd);
        /* Fn to remove an existing FD from above array */
        void remove_from_fd_array(uint16_t fd);
        /* Fn to keep track how many clients are connected to our TCP Server */
        int n_clients_connected;
        /* Dummy Master Socket FD required to unblock from select( ) at will and update TCPServer
        relevant data structures */
        uint32_t dummy_master_skt_fd;
        /* Server thread which runs in infinite loop to service all clients */
        pthread_t server_thread;
        /* Bunch of call-backs used by TCP-Server to notify about events to application. Appln
        is suppose to register its callback with TCP Server to recv notifications.*/
        TcpServerNotification *tcp_notif;
        /* In various scenarios, the caller threads needs to block until child thread finishes some work.
        This is done to make our code synchronous and less error prone. We use Zero semaphore for this purpose*/
        sem_t semaphore_wait_for_operation_complete;
        /* FLags which describe the state of TCP Server. see tsp_server_state_t enum*/
        uint8_t tcp_server_state_flags;
        /* FD Set on which select() blocks */
        fd_set active_client_fds;
        /* FD set which is a back up. All FDs from backup is copied to active just before select() call. This backup
        FD needs to be updated whenever any new client connects/disconnects so that TCPServer always monitor the
        correct number of client's FDs */
        fd_set backup_client_fds;
        /* A place holder which store the new Operation to be performed on TCPServer state machine. TCPServer code
        runs in a loop, and we need this variable to keep track the updates to be applied to TCPServer state variables */
        tcp_server_operations_t server_pending_operation;
        /* When client disconnects, this variable points to that TCPClient. TCPServer thread sees this variable to know which 
        TCPClient needs to be gracefully aborted*/
        TcpClient *pending_tcp_client;
        /* Fn to Change the state of TCP Server depending on the operation ( 2nd arg ). TCPClient ( Ist arg ) is also required
        if operation require involvement of TCPClient. For example, disconnection of TCPClient from TCPServer. */
        bool TcpServerChangeState(TcpClient *, tcp_server_operations_t);
        /* Fn to create a new TCPClient but in a separate thread. Such TCP Client are not monitored directly by TCPServers
        for Data Requests/disconnection requests */
        void TcpServerCreateMultithreadedClient(uint16_t , struct sockaddr_in *);
        /* Timer which is managed by TCPServer. This timer is used by TCPServer to send KA to all connected LIVE clients */
        Timer_t *ka_timer;
        
        
    public:
        std::string name;
        uint32_t self_ip_addr;
        uint16_t self_port_no;
        std::list<TcpClient *> tcp_client_conns;
        uint32_t master_skt_fd;
        uint16_t ka_interval;
        char ka_msg[32];

        /* Methods */
        TcpServer();
        ~TcpServer();
        void *TcpServerThreadFn();
        void TcpServer_KA_Dispatch_fn();
        void Display();
        void StopAcceptingNewConnections();
        void StopListeningAllClients();
        void ResumeListeningAllClients();
        void StopListeningClient();
        void TcpServerSetState(uint8_t flag);
        void TcpServerClearState(uint8_t flag);\
        uint8_t TcpServerGetStateFlag();
        void ResumeListeningClient();
        void MultiThreadedClientThreadServiceFn(TcpClient *);
        void Apply_ka_interval();
        TcpServer(const uint32_t &self_ip_addr, const uint16_t &self_port_no, std::string name);
        TcpClient *GetTcpClientbyFd(uint16_t fd);
        TcpClient *GetTcpClient(uint32_t ip_addr, uint16_t port_no);
        void RemoveTcpClient(TcpClient *tcp_client);
        void Start();
        void TcpSelfConnect();
        void AbortAllClients();
        void AbortClient(TcpClient *);
        void Stop();
        void RegisterClientConnectCbk(void (*)(const TcpClient *));
        void RegisterClientDisConnectCbk(void (*)(const TcpClient *));
        void RegisterClientMsgRecvCbk(void (*)(const TcpClient *, unsigned char *, uint16_t));
}; 

#endif 
