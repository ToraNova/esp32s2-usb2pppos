#include "bsp/board.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
//#include "httpd.h"
#include "pppd.h"

/* lwip context */
static struct netif usbet_netif;
static struct netif pppos_netif;
static ppp_pcb *ppp;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;
static uint8_t g_buf[1];

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

/* network parameters of this MCU */
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(169, 254, 254, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 252);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] = {
	/* mac ip address                          lease time */
	{ {0}, IPADDR4_INIT_BYTES(169, 254, 254, 2), 24 * 60 * 60 },
	//{ {0}, IPADDR4_INIT_BYTES(192, 168, 7, 3), 24 * 60 * 60 },
	//{ {0}, IPADDR4_INIT_BYTES(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t dhcp_config = {
	.router = IPADDR4_INIT_BYTES(0, 0, 0, 0),  /* router address (if any) */
	.port = 67,                                /* listen port */
	.dns = IPADDR4_INIT_BYTES(169, 254, 254, 1), /* dns server (if any) */
	"usb",                                     /* dns suffix */
	TU_ARRAY_SIZE(entries),                    /* num entry */
	entries                                    /* entries */
};

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
	(void)netif;

	for (;;)
	{
		/* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
		if (!tud_ready())
			return ERR_USE;

		/* if the network driver can accept another packet, we make it happen */
		if (tud_network_can_xmit())
		{
			tud_network_xmit(p, 0 /* unused for this example */);
			return ERR_OK;
		}

		/* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
		tud_task();
	}
}

static err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *addr) {
	return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif) {
	LWIP_ASSERT("netif != NULL", (netif != NULL));
	netif->mtu = CFG_TUD_NET_MTU;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
	netif->state = NULL;
	netif->name[0] = 'E';
	netif->name[1] = 'X';
	netif->linkoutput = linkoutput_fn;
	netif->output = output_fn;
	return ERR_OK;
}

static void init_lwip(void) {
	struct netif *usbif = &usbet_netif;

	lwip_init();

	/* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
	usbif->hwaddr_len = sizeof(tud_network_mac_address);
	memcpy(usbif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
	usbif->hwaddr[5] ^= 0x01;
	usbif = netif_add(usbif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ip_input);
	netif_set_default(usbif);

	board_uart_log("pppos connecting...1\n\r");
	ppp = pppos_create(&pppos_netif, ppp_output_cb , ppp_link_status_cb, NULL);
	board_uart_log("pppos connecting...2\n\r");
	ppp_set_default(ppp);
	board_uart_log("pppos connecting...3\n\r");
	err_t err = ppp_connect(ppp,0);
	board_uart_log("pppos connecting...4\n\r");
	if (err == ERR_ALREADY) {
		board_uart_log("connected successfully.\n\r");
	}
	while(ppp->phase < PPP_PHASE_RUNNING){
		board_uart_read(g_buf, 1);
	}

	//configure pppos
	//pppos = pppos_create(&pppos_netif, ppp_output_cb, ppp_link_status_cb, NULL);
	//ppp_connect(ppp, 0); //connect
}

/* handle any DNS requests from dns-server */
bool dns_query_proc(const char *name, ip_addr_t *addr) {
	if (0 == strcmp(name, "tiny.usb"))
	{
		*addr = ipaddr;
		return true;
	}
	return false;
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
	/* this shouldn't happen, but if we get another packet before
	   parsing the previous, we must signal our inability to accept it */
	if (received_frame) return false;

	if (size){
		struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
		if (p){
			/* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
			memcpy(p->payload, src, size);

			/* store away the pointer for service_traffic() to later handle */
			received_frame = p;
		}
	}
	return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
	struct pbuf *p = (struct pbuf *)ref;
	struct pbuf *q;
	uint16_t len = 0;

	(void)arg; /* unused for this example */

	/* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
	for(q = p; q != NULL; q = q->next)
	{
		memcpy(dst, (char *)q->payload, q->len);
		dst += q->len;
		len += q->len;
		if (q->len == q->tot_len) break;
	}

	return len;
}

static void service_traffic(void) {
	/* handle any packet received by tud_network_recv_cb() */
	if (received_frame)
	{
		ethernet_input(received_frame, &usbet_netif);
		pbuf_free(received_frame);
		received_frame = NULL;
		tud_network_recv_renew();
	}

	sys_check_timeouts();
}

void tud_network_init_cb(void) {
	/* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
	if (received_frame)
	{
		pbuf_free(received_frame);
		received_frame = NULL;
	}
}

//static uint8_t txbuffer[1024];
//static uint8_t rxbuffer[12];
//static uint8_t rxbuffer[1024];

int main(void) {
	/* initialize hardware */
	board_init();
	//sanity check
	for(uint8_t i=0; i<10;i++){
		board_led_write(1);
		board_delay(50);
		board_led_write(0);
		board_delay(50);
	}
	board_uart_log("hardware init success.\n\r");

	/* initialize TinyUSB */
	tusb_init();
	board_uart_log("tinyusb init success.\n\r");

	/* initialize lwip, dhcp-server, dns-server, and http */
	init_lwip();
	while (!netif_is_up(&usbet_netif));
	while (dhserv_init(&dhcp_config) != ERR_OK);
	while (dnserv_init(&ipaddr, 53, dns_query_proc) != ERR_OK);
	//httpd_init();

	//board_uart_write("uart 2 test.\n\r", 15);
	board_uart_log("lightweight-ip init success.\n\r");

	while (1) {
		board_uart_read(g_buf, 1);
		tud_task();
		service_traffic();
	}

	return 0;
}

void user_uart_callback(uint8_t c){
	board_uart_log("recv: %02x.\n\r",c);
	pppos_input(ppp, g_buf, 1);
	board_led_write(1);
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void) {
	return 0;
}
void sys_arch_unprotect(sys_prot_t pval) {
	(void)pval;
}

/* lwip needs a millisecond time source, and the TinyUSB board support code has one available */
uint32_t sys_now(void) {
	return board_millis();
}

uint32_t sys_jiffies(void) {
	return board_millis();
}
