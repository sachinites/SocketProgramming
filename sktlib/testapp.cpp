#include <stdio.h>
#include <unistd.h>
#include "tcp_mgmt.h"

void
client_disconnect_notif (const TcpClient *tcp_client) {
    printf ("client disconnected\n");
}

int
main(int argc, char **argv) {

    TcpServer *server1 = new TcpServer(0, 40000);
    server1->Start();
    server1->RegisterClientDisConnectCbk(client_disconnect_notif);
    sleep(10);
    server1->ForceDisconnectAllClients();
    pthread_exit(0);
    return 0;
}