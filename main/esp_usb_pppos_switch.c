/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#define MEMHEAP_CHECK xPortGetFreeHeapSize
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE //debug

#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "netsuite_io.h"
#include "httpd.h"

//http server test
//#include <esp_http_server.h>

static const char *TAG_APP = "main";
static const char *TAG_ETH = "usbeth";

//#include "lwip/netif.h"
//#include "netif/ethernet.h"
//extern void* esp_netif_get_netif_impl(esp_netif_t *esp_netif);

//tinyusb network interface callbacks
bool tud_network_recv_cb(const uint8_t *src, uint16_t size);
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);
void tud_network_init_cb(void);

// usb descriptors
extern tusb_desc_device_t desc_device;
extern tusb_desc_strarray_device_t desc_string;
// gv
static esp_netif_t* netif;
static uint8_t tud_rx_buf[2048];
static size_t tud_rx_len = 0;

void app_main(void) {
    ESP_LOGI(TAG_APP, "tinyusb initialization.");
    uint8_t tbuf[2048];
    tinyusb_config_t tusb_cfg = {
        .descriptor = &desc_device,
        .string_descriptor = desc_string,
        .external_phy = false // In the most cases you need to use a `false` value
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG_APP, "tinyusb driver installed.");

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    //// Create default event loop that runs in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // device virtual MAC address setup
    uint8_t tud_virtual_mac_address[6]; //device mac address
    memcpy(tud_virtual_mac_address, tud_network_mac_address, 6 );
    tud_virtual_mac_address[5] ^= 0x01; //toggle LSB of DEVICE macaddr to diff from HOST macaddr

    // ipv4 setup
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("169.254.100.100",&ip_info.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("255.255.255.0",&ip_info.netmask));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("0.0.0.0",&ip_info.gw));

    esp_netif_inherent_config_t ihcfg = {
        //.flags = ESP_NETIF_FLAG_AUTOUP | ESP_NETIF_FLAG_GARP | ESP_NETIF_DHCP_SERVER,
	.flags = (esp_netif_flags_t)(ESP_NETIF_FLAG_AUTOUP),
        .ip_info = (esp_netif_ip_info_t*)&ip_info,
        //.get_ip_event = 0,
        //.lost_ip_event = 0,
        .if_key = "USBNET",
        .if_desc = "usb",
        //.route_prio = 50
    };
    // interface configuration
    esp_netif_config_t config = {
        .base = &ihcfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
        //.driver = NULL,
    };

    // interface creation
    ESP_ERROR_CHECK(esp_netif_init());
    netif = esp_netif_new(&config);
    assert(netif);
    esp_netif_attach(netif, netsuite_io_new());

    // start netif
    ESP_ERROR_CHECK(esp_netif_set_mac(netif, tud_virtual_mac_address));
    //ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    esp_netif_action_start(netif, NULL, 0, NULL);

    while (!esp_netif_is_netif_up(netif)) vTaskDelay( 100 / portTICK_PERIOD_MS );
    ESP_LOGI(TAG_APP, "usbnetif is up.");

    //static httpd_handle_t server = NULL;
    start_webserver(); //start the server

    size_t hs;
    while (1){
        if (tud_rx_len > 0){
            hs = MEMHEAP_CHECK();
            //we have smth to read
	    for(size_t i=0; i< tud_rx_len; i++){
	        tbuf[i] = tud_rx_buf[i];
	    }
            esp_netif_receive(netif, tbuf, tud_rx_len, NULL);
            tud_network_recv_renew();
            tud_rx_len = 0; //indicate buf is now read
	    ESP_LOGD(TAG_APP, "esp_netif_recv heap (%u).", hs);
        }else{
            vTaskDelay( 10 / portTICK_PERIOD_MS );
        }
        //sys_check_timeouts(); //TODO: is this important?
    }

}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
    if (tud_rx_len < 1) {
	    for(size_t i=0; i<size;i++){
		    tud_rx_buf[i] = src[i];
	    }
	    //memcpy(tud_rx_buf, src, size);
	    tud_rx_len = size; //set tud_rx_len ONLY after finish copying
	    return true;
    }else{
	    //unable to receive, busy
	    ESP_LOGW(TAG_ETH, "network_recv_cb busy.");
	    return false;
    }
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
    struct pbuf *p = (struct pbuf *)ref; //a linked list of pbufs
    struct pbuf *q;
    uint16_t len = 0;
    (void)arg; /* unused for this example */
    /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
    for(q = p; q != NULL; q = q->next) {
        memcpy(dst, (char *)q->payload, q->len);
        dst += q->len; //increment the dst pointer (write further down)
        len += q->len; //increment the total length
        if (q->len == q->tot_len) break;
    }
    return len; //return total length of the dst buffer
}

void tud_network_init_cb(void) {
    /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
    ESP_LOGD(TAG_ETH, "network_init_cb.");
    return;
}

/** Event handler for Ethernet events */
//static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
//    uint8_t mac_addr[6] = {0};
//    /* we can get the ethernet driver handle from event data */
//    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
//
//    switch (event_id) {
//    case ETHERNET_EVENT_CONNECTED:
//        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
//        ESP_LOGI(TAG_ETH, "Ethernet Link Up");
//        ESP_LOGI(TAG_ETH, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
//                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
//        break;
//    case ETHERNET_EVENT_DISCONNECTED:
//        ESP_LOGI(TAG_ETH, "Ethernet Link Down");
//        break;
//    case ETHERNET_EVENT_START:
//        ESP_LOGI(TAG_ETH, "Ethernet Started");
//        break;
//    case ETHERNET_EVENT_STOP:
//        ESP_LOGI(TAG_ETH, "Ethernet Stopped");
//        break;
//    default:
//        break;
//    }
//}
//
///** Event handler for IP_EVENT_ETH_GOT_IP */
//static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
//    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
//    const esp_netif_ip_info_t *ip_info = &event->ip_info;
//
//    ESP_LOGI(TAG_ETH, "Ethernet Got IP Address");
//    ESP_LOGI(TAG_ETH, "~~~~~~~~~~~");
//    ESP_LOGI(TAG_ETH, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
//    ESP_LOGI(TAG_ETH, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
//    ESP_LOGI(TAG_ETH, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
//    ESP_LOGI(TAG_ETH, "~~~~~~~~~~~");
//}

