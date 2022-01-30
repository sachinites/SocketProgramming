#ifndef __TCP_CLI_HANDLER__
#define __TCP_CLI_HANDLER__

TcpServer *
config_create_new_tcp_server (std::string tcp_server_name,
                                                   std::string ip_addr,
                                                   uint16_t port_no);

void
show_tcp_sever(TcpServer *tcp_Server);

#endif 