#include "bsp/board.h"
#include "pppd.h"

void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx){
	struct netif *pppif = ppp_netif(pcb);
	LWIP_UNUSED_ARG(ctx);
	switch(err_code) {
		case PPPERR_NONE:               /* No error. */
			board_uart_log("ppp_link_status_cb: PPPERR_NONE.\n\r");
			board_uart_log("dev_ipv4addr = %s.\n\r",\
			ip4addr_ntoa(netif_ip4_addr(pppif)));
			board_uart_log("rem_ipv4addr = %s.\n\r",\
			ip4addr_ntoa(netif_ip4_gw(pppif)));
			board_uart_log("netmask      = %s.\n\r",\
			ip4addr_ntoa(netif_ip4_netmask(pppif)));
			break;
		case PPPERR_PARAM:             /* Invalid parameter. */
			board_uart_log("ppp_link_status_cb: PPPERR_PARAM.\n\r");
			break;
		case PPPERR_OPEN:              /* Unable to open PPP session. */
			board_uart_log("ppp_link_status_cb: PPPERR_OPEN.\n\r");
			break;
		case PPPERR_DEVICE:            /* Invalid I/O device for PPP. */
			board_uart_log("ppp_link_status_cb: PPPERR_DEVICE.\n\r");
			break;
		case PPPERR_ALLOC:             /* Unable to allocate resources. */
			board_uart_log("ppp_link_status_cb: PPPERR_ALLOC.\n\r");
			break;
		case PPPERR_USER:              /* User interrupt. */
			board_uart_log("ppp_link_status_cb: PPPERR_USER.\n\r");
			break;
		case PPPERR_CONNECT:           /* Connection lost. */
			board_uart_log("ppp_link_status_cb: PPPERR_CONNECT.\n\r");
			break;
		case PPPERR_AUTHFAIL:          /* Failed authentication challenge. */
			board_uart_log("ppp_link_status_cb: PPPERR_AUTHFAIL.\n\r");
			break;
		case PPPERR_PROTOCOL:          /* Failed to meet protocol. */
			board_uart_log("ppp_link_status_cb: PPPERR_PROTOCOL.\n\r");
			break;
		case PPPERR_PEERDEAD:          /* Connection timeout. */
			board_uart_log("ppp_link_status_cb: PPPERR_PEERDEAD.\n\r");
			break;
		case PPPERR_IDLETIMEOUT:       /* Idle Timeout. */
			board_uart_log("ppp_link_status_cb: PPPERR_IDLETIMEOUT.\n\r");
			break;
		case PPPERR_CONNECTTIME:       /* PPPERR_CONNECTTIME. */
			board_uart_log("ppp_link_status_cb: PPPERR_CONNECTTIME.\n\r");
			break;
		case PPPERR_LOOPBACK:          /* Connection timeout. */
			board_uart_log("ppp_link_status_cb: PPPERR_LOOPBACK.\n\r");
			break;
		default:
			board_uart_log("ppp_link_status_cb: unknown errno %d.\n\r", err_code);
			break;
	}
}

// Callback used by ppp connection
uint32_t ppp_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx){
	LWIP_UNUSED_ARG(pcb);
	LWIP_UNUSED_ARG(ctx);
	if (len > 0) {
		if (!board_uart_write(data,len)) return 0x05;
	}
	board_uart_log("pppos.write: len = %ld.\n\r", len);
	return len;
}
