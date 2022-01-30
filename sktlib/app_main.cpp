#include <list>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "tcp_mgmt.h"

extern void tcp_build_cli();

/* GLobal appln wide Objects */
std::list<TcpServer *> tcp_server_lst;
TcpServer *
TcpServer_lookup (std::string tcp_sever_name);

TcpServer *
TcpServer_lookup (std::string tcp_sever_name) {

    TcpServer *tcp_server;
    std::list<TcpServer *>::iterator it;

    for (it = tcp_server_lst.begin() ; it != tcp_server_lst.end(); ++it) {

        tcp_server = *it;
        if (tcp_server->name != tcp_sever_name ) continue;
        return tcp_server;
    }
    return NULL;
}

int 
main(int argc, char **argv) {

    init_libcli();
    tcp_build_cli();
    start_shell();
    return 0;
}