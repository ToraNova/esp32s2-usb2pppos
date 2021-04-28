/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
//#include "dhserver.h"
//#include "dnserver.h"
//#include "lwip/init.h"
//#include "lwip/timeouts.h"

#include "sdkconfig.h"

static const char *TAG = "example";
// usb descriptors
extern tusb_desc_device_t desc_device;
extern tusb_desc_strarray_device_t desc_string;
// fp
bool tud_network_recv_cb(const uint8_t *src, uint16_t size);
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);
void tud_network_init_cb(void);

// usb ether network interface
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};
//static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(169, 254, 254, 1);
//static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 252);
//static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);
//static void usbnet_init(void) {
//	struct netif *usbif = &usbet_netif;
//	lwip_init();
//	usbif->hwaddr_len = sizeof(tud_network_mac_address);
//	memcpy(usbif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
//	usbif->hwaddr[5] ^= 0x01; //toggle LSB of vmac addr
//	usbif = netif_add(usbif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ip_input);
//	netif_set_default(usbif);
//	return;
//}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

void app_main(void) {
    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = {
        .descriptor = &desc_device,
        .string_descriptor = desc_string,
        .external_phy = false // In the most cases you need to use a `false` value
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Set default handlers to process TCP/IP stuffs
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;//TODO
    phy_config.reset_gpio_num = 5;//TODO

    //TODO
    //ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    /* start Ethernet driver state machine */
    //ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "RNDIS/ECM initialization DONE");
}

//static void service_traffic(void) {
//	/* handle any packet received by tud_network_recv_cb() */
//	if (received_frame)
//	{
//		ethernet_input(received_frame, &usbet_netif);
//		pbuf_free(received_frame);
//		received_frame = NULL;
//		tud_network_recv_renew();
//	}
//
//	sys_check_timeouts();
//}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
	/* this shouldn't happen, but if we get another packet before
	   parsing the previous, we must signal our inability to accept it */
	//if (received_frame) return false;

	//if (size){
	//	struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
	//	if (p){
	//		/* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
	//		memcpy(p->payload, src, size);

	//		/* store away the pointer for service_traffic() to later handle */
	//		received_frame = p;
	//	}
	//}
	return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
	//struct pbuf *p = (struct pbuf *)ref;
	//struct pbuf *q;
	//uint16_t len = 0;

	//(void)arg; /* unused for this example */

	///* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
	//for(q = p; q != NULL; q = q->next)
	//{
	//	memcpy(dst, (char *)q->payload, q->len);
	//	dst += q->len;
	//	len += q->len;
	//	if (q->len == q->tot_len) break;
	//}
	return 0;
}

void tud_network_init_cb(void) {
	/* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
	//if (received_frame)
	//{
	//	pbuf_free(received_frame);
	//	received_frame = NULL;
	//}
}
