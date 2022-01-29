#include <list>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "tcp_mgmt.h"

extern void tcp_build_cli();

/* GLobal appln wide Objects */
std::list<TcpServer *> tcp_Server_lst;

int 
main(int argc, char **argv) {

    init_libcli();
    tcp_build_cli();
    start_shell();
    return 0;
}