#include <stdlib.h>
#include "../include/common.h"

char*
get_string(unsigned int arg){
	switch(arg){
		case _IPPROTO_IGMP:
			return "_IPPROTO_IGMP"; 
		case _IPPROTO_PIM:
			return "_IPPROTO_PIM";
		case IGMP_REPORTS:
			return "IGMP_REPORTS";
		case IGMP_QUERY:
			return "IGMP_QUERY";
		case IGMP_LEAVE:
			return "IGMP_LEAVE";
		case PIM_HELLO:
			return "PIM_HELLO";
		case  PIM_JOIN:
			return "PIM_JOIN";
		case PIM_REGISTER:
			return "PIM_REGISTER";
		default:
			break;
	}
	return NULL;
}
