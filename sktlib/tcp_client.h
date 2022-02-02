#ifndef __TCP_CLIENT__
#define __TCP_CLIENT__

#include <stdint.h>
#include <pthread.h>
#include "timerlib.h"

#define TCP_CLIENT_EXPIRATION_TIMER 5
#define TCP_CLIENT_HOLD_DOWN_TIMER 20

class TcpServer;
class TcpConn;
class TcpClient {

    public:
    TcpConn *tcp_conn;
    timer_t re_try_timer;
    uint8_t retry_time_interval;
    uint8_t ref_count;
    bool listen_by_server;
    pthread_t *client_thread;
    TcpServer *tcp_server;
    Timer_t *expiration_timer;
    void StartExpirationTimer();
    void CancelExpirationTimer();
    void DeleteExpirationTimer();
    void ReStartExpirationTimer();
    
    /* Methods */
    TcpClient();
    ~TcpClient();
    bool TcpClientConnect(uint32_t server_ip, uint16_t server_port);  
    void TcpClientDisConnect();
    void TcpClientAbort();
    void Display();
};



#endif 