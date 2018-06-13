#define SUCCESS	0
#define FAILURE -1


typedef enum{
	FALSE,
	TRUE
} bool_t;


typedef enum{
	_IPPROTO_IGMP = 140,
	_IPPROTO_PIM
} protocol;

typedef enum{
	IGMP_REPORTS = 0,
	IGMP_QUERY,
	IGMP_LEAVE,
	PIM_HELLO,
	PIM_JOIN,
	PIM_REGISTER
} pkt_type;

typedef struct igmp_hdr{
	pkt_type type;
	unsigned int seqno;
} igmp_hdr_t;;


#define IGMP_TP_HEADER_SIZE	(sizeof(igmp_hdr_t))

typedef struct pim_hdr{
	pkt_type type;
	unsigned int seqno;
} pim_hdr_t;;

#define PIM_TP_HEADER_SIZE	(sizeof(pim_hdr_t))

char*
get_string(unsigned int arg);

