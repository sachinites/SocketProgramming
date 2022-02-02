#ifndef __TCP_CONN_H__
#define __TCP_CONN_H__

#include <stdint.h>
#include "timerlib.h"

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
    int SendMsg(char *msg, uint32_t msg_size);
    void Display();
};


#endif 