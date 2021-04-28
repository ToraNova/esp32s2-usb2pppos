#ifndef __pppd_h__
#define __pppd_h__

#include "netif/ppp/ppp.h"
#include "netif/ppp/pppos.h"

#define RECV_BUF_LEN 2048

#ifdef __cplusplus
extern "C" {
#endif

	void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);
	uint32_t ppp_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);

#ifdef __cplusplus
}
#endif

#endif
