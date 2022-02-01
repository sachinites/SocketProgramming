#include <stdio.h>
#include <unistd.h>
#include "tcp_mgmt.h"

void
client_disconnect_notif (const TcpClient *tcp_client) {
    printf ("client disconnected\n");
}

void
client_connect_notif (const TcpClient *tcp_client) {
    printf ("client connected\n");
}

void
client_recv_msg(const TcpClient *tcp_client, unsigned char *msg, uint16_t msg_size) {

    printf ("msg recvd = %d B\n", msg_size);
}

int
main(int argc, char **argv) {

    TcpServer *server1 = new TcpServer(0, 40000, "Default");
    server1->RegisterClientDisConnectCbk(client_disconnect_notif);
    server1->RegisterClientMsgRecvCbk(client_recv_msg);
    server1->RegisterClientConnectCbk(client_connect_notif);
    server1->Start();
    server1->Stop();
    return 0;
}