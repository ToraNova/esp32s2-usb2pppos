#include "usbx_net.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "freertos/FreeRTOS.h"
#include "esp_netif.h"
#include "lwip/pbuf.h"

// usb descriptors
extern tusb_desc_device_t desc_device;
extern tusb_desc_strarray_device_t desc_string;

// local use only
static uint8_t tud_rx_buf[4096];
static size_t tud_rx_len = 0;
static const char *TAG = "usbx_net";
static void task(void *arg);
static esp_err_t netsuite_io_transmit(void *h, void *buffer, size_t len);
static esp_err_t netsuite_io_transmit_wrap(void *h, void *buffer, size_t len, void *netstack_buf);
static esp_err_t netsuite_io_attach(esp_netif_t * esp_netif, void * args);

// TODO: implement rx here?
const esp_netif_driver_ifconfig_t driver_ifconfig = {
    .driver_free_rx_buffer = NULL,
    .transmit = netsuite_io_transmit,
    .transmit_wrap = netsuite_io_transmit_wrap,
    .handle = "usbx_net-netsuite-io-object" // this IO object is a singleton, its handle uses as a name
};
const esp_netif_driver_base_t s_driver_base = {
    .post_attach =  netsuite_io_attach
};

void * netsuite_io_new(void) {
    return (void *)&s_driver_base;
}

esp_err_t usbx_netif_init(esp_netif_t *netif){

    // install usb driver as rndis/ecm device
    tinyusb_config_t tusb_cfg = {
        .descriptor = &desc_device,
        .string_descriptor = desc_string,
        .external_phy = false // In the most cases you need to use a `false` value
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // device virtual MAC address setup
    uint8_t tud_virtual_mac_address[6]; //device mac address
    memcpy(tud_virtual_mac_address, tud_network_mac_address, 6 );
    tud_virtual_mac_address[5] ^= 0x01; //toggle LSB of DEVICE macaddr to diff from HOST macaddr

    // usb ipv4 setup
    esp_netif_ip_info_t usb_ip;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.2.2",&usb_ip.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("255.255.255.0",&usb_ip.netmask));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("0.0.0.0",&usb_ip.gw));

    esp_netif_inherent_config_t usb_ihcfg = {
        //.flags = ESP_NETIF_FLAG_AUTOUP | ESP_NETIF_FLAG_GARP | ESP_NETIF_DHCP_SERVER,
	    .flags = (esp_netif_flags_t)(ESP_NETIF_FLAG_AUTOUP),
        .ip_info = (esp_netif_ip_info_t*)&usb_ip,
        //.get_ip_event = 0,
        //.lost_ip_event = 0,
        .if_key = "USBNET",
        .if_desc = "usb",
        //.route_prio = 50
    };

    // interface configuration
    esp_netif_config_t usb_config = {
        .base = &usb_ihcfg,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };

    // interface creation
    netif = esp_netif_new(&usb_config);
    assert(netif);
    esp_netif_attach(netif, netsuite_io_new());

    // start netif
    ESP_ERROR_CHECK(esp_netif_set_mac(netif, tud_virtual_mac_address));
    //ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &usb_ip));
    esp_netif_action_start(netif, NULL, 0, NULL);

    // wait until usbnetif is up
    while (!esp_netif_is_netif_up(netif)) vTaskDelay( 100 / portTICK_PERIOD_MS );

    xTaskCreate(
        task, /* Task function. */
        "usbxTask", /* name of task. */
        3000, /* Stack size of task */
        netif, /* parameter of the task */
        1, /* priority of the task */
        NULL
    ); /* Task handle to keep track of created task */
    return ESP_OK;
}

void tud_network_init_cb(void) {
    /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
    ESP_LOGI(TAG, "tud_network_init_cb.");
    tud_rx_len = 0;
    return;
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
	    ESP_LOGW(TAG, "tud_network_recv_cb busy.");
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

static void task(void *arg){
    esp_netif_t *netif = (esp_netif_t *) arg;
    size_t hs;

    while(1){
        if (tud_rx_len > 0){
            esp_netif_receive(netif, tud_rx_buf, tud_rx_len, NULL);
            tud_network_recv_renew();
            tud_rx_len = 0; //indicate buf is now read

            hs = xPortGetFreeHeapSize();
            ESP_LOGI(TAG, "usbx_task rxb heap check (%u).", hs);
        } else {
            vTaskDelay( 10 / portTICK_PERIOD_MS );
        }
    }
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
        if(tud_network_can_xmit(0)){
            struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
            memcpy(p->payload, buffer, len);
            tud_network_xmit(p, 0);
            pbuf_free(p); // crucial to prevent memleak
            return ESP_OK;
        }
    }
}
