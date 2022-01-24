#include "tcp_mgmt.h"

int
main(int argc, char **argv) {

    TcpServer *server1 = new TcpServer(0, 40000);
    server1->Start();
    pthread_exit(0);
    return 0;
}