#include <string>
#include <stdint.h>
#include "tcp_mgmt.h"
#include "tcp_cli_handler.h"
#include "network_utils.h"

TcpServer *
config_create_new_tcp_server(std::string tcp_server_name,
                                                   std::string ip_addr,
                                                   uint16_t port_no) {

    uint32_t ip_addr_int;
    ip_addr_int = network_covert_ip_p_to_n(ip_addr.c_str());
    return new TcpServer (ip_addr_int, port_no, tcp_server_name);
}

void
show_tcp_sever(TcpServer *tcp_server) {

    tcp_server->Display();
}