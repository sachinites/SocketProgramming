#include <stdio.h>
#include <stdlib.h>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"

static void
tcp_build_config_cli_tree() {

    param_t *config_hook = libcli_get_config_hook();
}
static void
tcp_build_run_cli_tree() {

    param_t *run_hook = libcli_get_run_hook();
}
static void
tcp_build_show_cli_tree() {

    param_t *show_hook = libcli_get_show_hook();
}

void
tcp_build_cli() {

    tcp_build_config_cli_tree();
    tcp_build_run_cli_tree();
    tcp_build_show_cli_tree();
}