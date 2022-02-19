/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#define MEMHEAP_CHECK xPortGetFreeHeapSize
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE //debug

#include <stdlib.h>
#include "sdkconfig.h"
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
//#include "netsuite_io.h"
#include "httpd.h"
#include "usbx_net.h"

static const char *TAG = "main";

// gv
#define BUFSIZE 4096
static esp_netif_t* usb_netif;
static esp_netif_t* ppp_netif;

void app_main(void) {
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "esp_netif init success.");
    ESP_ERROR_CHECK(usbx_netif_init(usb_netif));
    ESP_LOGI(TAG, "usbx_netif init success.");

    // setup ppp
    // ppp ipv4 setup
    esp_netif_ip_info_t ppp_ip;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.0.2",&ppp_ip.ip)); //server is 192.168.0.1
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("255.255.255.0",&ppp_ip.netmask));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("0.0.0.0",&ppp_ip.gw));

    esp_netif_inherent_config_t ppp_ihcfg = {
        .flags = (esp_netif_flags_t)(ESP_NETIF_FLAG_IS_PPP),
        .ip_info = (esp_netif_ip_info_t*)&ppp_ip,
        //.get_ip_event = 0,
        //.lost_ip_event = 0,
        .if_key = "PPPNET",
        .if_desc = "ppp",
        //.route_prio = 20
    };
    esp_netif_config_t ppp_config = {
        .base = &ppp_ihcfg,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP,
    };
    ppp_netif = esp_netif_new(&ppp_config);
    assert(ppp_netif);
    //esp_netif_attach(ppp_netif, netsuite_io_new());

    //static httpd_handle_t server = NULL;
    //start_webserver(); //start the server

    while (1){
        vTaskDelay( 10 / portTICK_PERIOD_MS );
        sys_check_timeouts();
    }
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

