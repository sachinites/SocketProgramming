#ifndef __TCP_CLI_HANDLER__
#define __TCP_CLI_HANDLER__

#include <string>
#include <list>
#include "tcp_mgmt.h"

extern std::list<TcpServer *> tcp_Server_lst;

bool
config_create_new_tcp_server(std::string tcp_server_name,
                                                   std::string ip_addr,
                                                   uint16_t port_no);

#endif 