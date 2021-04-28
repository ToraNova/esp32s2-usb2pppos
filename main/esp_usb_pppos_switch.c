/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE //debug
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

static const char *TAG_APP = "main";
static const char *TAG_ETH = "usbeth";

// Internal functions declaration referenced in io object
void * netsuite_io_new(void);
static esp_err_t netsuite_io_transmit(void *h, void *buffer, size_t len);
static esp_err_t netsuite_io_transmit_wrap(void *h, void *buffer, size_t len, void *netstack_buf);
static esp_err_t netsuite_io_attach(esp_netif_t * esp_netif, void * args);
const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = NULL,
        .transmit = netsuite_io_transmit,
        .transmit_wrap = netsuite_io_transmit_wrap,
        .handle = "netsuite-io-object" // this IO object is a singleton, its handle uses as a name
};
const esp_netif_driver_base_t s_driver_base = {
        .post_attach =  netsuite_io_attach
};

// usb descriptors
extern tusb_desc_device_t desc_device;
extern tusb_desc_strarray_device_t desc_string;
// fp
static struct pbuf *tud_rx_frame; //tud_rx_frame
static void *tud_rx_buffer = NULL;
static size_t tud_rx_bufsz;

void tud_service_traffic(void *pvparam);
bool tud_network_recv_cb(const uint8_t *src, uint16_t size);
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);
void tud_network_init_cb(void);

void app_main(void) {
    ESP_LOGI(TAG_APP, "tinyusb initialization.");
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

    // interface configuration
    esp_netif_inherent_config_t ihcfg = {
            .flags = ESP_NETIF_FLAG_AUTOUP | ESP_NETIF_FLAG_GARP | ESP_NETIF_DHCP_SERVER,
            .ip_info = (esp_netif_ip_info_t*)&ip_info,
            .if_key = "USB",
            .if_desc = "usbether"
    };
    esp_netif_config_t config = {
        .base = &ihcfg,                          // use specific behaviour configuration
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH, // use default ethernet-like network stack configuration
    };

    // interface creation
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t* netif = esp_netif_new(&config);
    assert(netif);
    esp_netif_attach(netif, netsuite_io_new());

    // start netif
    esp_netif_set_mac(netif, tud_virtual_mac_address);
    esp_netif_action_start(netif, NULL, 0, NULL);

    while (!esp_netif_is_netif_up(netif));
    ESP_LOGI(TAG_APP, "usbnetif is up.");

    //create the service task
    xTaskCreate(&tud_service_traffic, "tud_service_traffic", 2048, netif, 10, NULL);
}

//TODO for all netsuite
void * netsuite_io_new(void) {
    return (void *)&s_driver_base;
}

static esp_err_t netsuite_io_attach(esp_netif_t * esp_netif, void * args) {
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    return ESP_OK;
}

static esp_err_t netsuite_io_transmit_wrap(void *h, void *buffer, size_t len, void *netstack_buf) {
    return netsuite_io_transmit(h, buffer, len);
}

static esp_err_t netsuite_io_transmit(void *h, void *buffer, size_t len) {
    for(;;){
        if(!tud_ready()) return ESP_FAIL;
	if(tud_network_can_xmit()){
            struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	    memcpy(p->payload, buffer, len);
	    tud_network_xmit(p, 0);
	    return ESP_OK;
	}
    }
}

void tud_service_traffic(void *pvparam){
    esp_netif_t* netif = (esp_netif_t*) pvparam;
    // read from received_frame and pass the data to usb netif
    while (1) {
        if (tud_rx_buffer){ //received tud_rx
            esp_netif_receive(netif, tud_rx_buffer, tud_rx_bufsz, NULL);
            //free(tud_rx_buffer); // TODO: memleak problem
            tud_rx_bufsz = 0;
            tud_rx_buffer = NULL;
            tud_network_recv_renew();
            ESP_LOGD(TAG_APP, "tud_rx_buffer processed.");
        }else{
	    vTaskDelay(10 / portTICK_RATE_MS);
	}
        sys_check_timeouts(); //TODO: is this important?
    }
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
    /* this shouldn't happen, but if we get another packet before
       parsing the previous, we must signal our inability to accept it */
    if (tud_rx_buffer) return false; //unable to process
    if(size){
        tud_rx_bufsz = size;
	void * p = calloc(size, sizeof(uint8_t));
        memcpy(p, src, tud_rx_bufsz);
	tud_rx_buffer = p;
    }
    ESP_LOGD(TAG_ETH, "network_recv_cb.");
    return true;
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
    ESP_LOGD(TAG_ETH, "network_xmit_cb.");
    return len; //return total length of the dst buffer
}

void tud_network_init_cb(void) {
    /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
    if (tud_rx_frame) {
	free(tud_rx_buffer);
	tud_rx_bufsz = 0;
        tud_rx_buffer= NULL;
    }
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

