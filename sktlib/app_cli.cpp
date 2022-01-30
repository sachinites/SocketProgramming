#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "tcpcmdcodes.h"
#include "tcp_mgmt.h"
#include "tcp_cli_handler.h"

extern std::list<TcpServer *> tcp_server_lst;

extern  TcpServer *
TcpServer_lookup (std::string tcp_sever_name) ;

static int
config_tcp_server_handler (param_t *param,
                                  ser_buff_t *tlv_buf, 
                                  op_mode enable_or_disable) {

    int rc = 0;
    int CMDCODE;
    tlv_struct_t *tlv = NULL;
    TcpServer *tcp_server = NULL;
    const char *ip_addr = "127.0.0.1";
    const char *tcp_server_name = NULL;
    uint16_t port_no = TCP_DEFAULT_PORT_NO;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if     (strncmp(tlv->leaf_id, "tcp-server-name", strlen("tcp-server-name")) ==0)
            tcp_server_name = tlv->value;
        else if (strncmp(tlv->leaf_id, "tcp-server-addr", strlen("tcp-server-addr")) ==0)
            ip_addr = tlv->value;
        else if (strncmp(tlv->leaf_id, "tcp-server-port", strlen("tcp-server-port")) ==0)
            port_no = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;

    switch (CMDCODE) {
        case TCP_SERVER_CREATE:
            tcp_server = TcpServer_lookup(std::string(tcp_server_name));
            if (tcp_server) {
                printf ("Error : Tcp Server Already Exist\n");
                return -1;
            }
            tcp_server = config_create_new_tcp_server(
                            std::string(tcp_server_name),
                            std::string(ip_addr), port_no);
            if ( !tcp_server ) {
                printf ("Error : Tcp Server Creation Failed\n");
                return -1;
            }
            tcp_server_lst.insert(tcp_server_lst.begin(), tcp_server);
            break;
        case TCP_SERVER_START:
            tcp_server = TcpServer_lookup(std::string(tcp_server_name));
            if (!tcp_server) {
                printf ("Error : Tcp Server do not Exist\n");
                return -1;
            }
            tcp_server->Start();
            break;
        default:
            ;
    }
    return rc;
}

static void
tcp_build_config_cli_tree() {

    param_t *config_hook = libcli_get_config_hook();

    {
        /* config tcp-server ... */
        static param_t tcp_server;
        init_param(&tcp_server, CMD, "tcp-server", 0, 0, INVALID, 0, "config tcp-server");
        libcli_register_param(config_hook, &tcp_server);
        {
            /* config tcp-server <name> */
            static param_t tcp_server_name;
            init_param(&tcp_server_name, LEAF, 0, config_tcp_server_handler, 0, STRING, "tcp-server-name", "config tcp-server name");
            libcli_register_param(&tcp_server, &tcp_server_name);
            set_param_cmd_code(&tcp_server_name, TCP_SERVER_CREATE);
            {
                static param_t start;
                init_param(&start, CMD, "start", config_tcp_server_handler, 0, INVALID, 0, "start tcp-server");
                libcli_register_param(&tcp_server_name, &start);
                set_param_cmd_code(&start, TCP_SERVER_START);
            }
            {
                 /* config tcp-server <name> [<ip-addr>] */
                 static param_t tcp_server_addr;
                 init_param(&tcp_server_addr, LEAF, 0, config_tcp_server_handler, 0, IPV4, "tcp-server-addr", "config tcp-server address");
                 libcli_register_param(&tcp_server_name, &tcp_server_addr);
                 set_param_cmd_code(&tcp_server_name, TCP_SERVER_CREATE);
                 {
                     /* config tcp-server <name> [<ip-addr>][ <port-no> ]*/
                     static param_t tcp_server_port;
                     init_param(&tcp_server_port, LEAF, 0, config_tcp_server_handler, 0,  INT, "tcp-server-port", "config tcp-server port no");
                     libcli_register_param(&tcp_server_addr, &tcp_server_port);
                     set_param_cmd_code(&tcp_server_port, TCP_SERVER_CREATE);
                 }
            }
        }
    }
}
static void
tcp_build_run_cli_tree() {

    param_t *run_hook = libcli_get_run_hook();
}

static int
show_tcp_server_handler (param_t *param,
                                  ser_buff_t *tlv_buf, 
                                  op_mode enable_or_disable) {

    int rc = 0;
    int CMDCODE;
    tlv_struct_t *tlv = NULL;
    TcpServer *tcp_server = NULL;
    const char *tcp_server_name = NULL;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if     (strncmp(tlv->leaf_id, "tcp-server-name", strlen("tcp-server-name")) ==0)
            tcp_server_name = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    switch (CMDCODE) {
        case TCP_SERVER_SHOW_TCP_SERVER:
            tcp_server = TcpServer_lookup(std::string(tcp_server_name));  
            if (!tcp_server) {
                printf ("Error : Tcp Server do not Exist\n");
                return -1;
            }
            tcp_server->Display();
            break;
        default:
            ;
    }
    return rc;
}

static void
tcp_build_show_cli_tree() {

    param_t *show_hook = libcli_get_show_hook();

    /* config tcp-server ... */
        static param_t tcp_server;
        init_param(&tcp_server, CMD, "tcp-server", 0, 0, INVALID, 0, "config tcp-server");
        libcli_register_param(show_hook, &tcp_server);
        {
            /* config tcp-server <name> */
            static param_t tcp_server_name;
            init_param(&tcp_server_name, LEAF, 0, show_tcp_server_handler, 0, STRING, "tcp-server-name", "config tcp-server name");
            libcli_register_param(&tcp_server, &tcp_server_name);
            set_param_cmd_code(&tcp_server_name, TCP_SERVER_SHOW_TCP_SERVER);
        }
}

void
tcp_build_cli() {

    tcp_build_config_cli_tree();
    tcp_build_run_cli_tree();
    tcp_build_show_cli_tree();
}