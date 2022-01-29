#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"

extern void tcp_build_cli();

int 
main(int argc, char **argv) {

    init_libcli();
    tcp_build_cli();
    start_shell();
    return 0;
}