#include "esp_netif.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "lwip/pbuf.h"

static esp_err_t netsuite_io_transmit(void *h, void *buffer, size_t len);
static esp_err_t netsuite_io_transmit_wrap(void *h, void *buffer, size_t len, void *netstack_buf);
static esp_err_t netsuite_io_attach(esp_netif_t * esp_netif, void * args);

// TODO: implement rx here?
const esp_netif_driver_ifconfig_t driver_ifconfig = {
    .driver_free_rx_buffer = NULL,
    .transmit = netsuite_io_transmit,
    .transmit_wrap = netsuite_io_transmit_wrap,
    .handle = "netsuite-io-object" // this IO object is a singleton, its handle uses as a name
};
const esp_netif_driver_base_t s_driver_base = {
    .post_attach =  netsuite_io_attach
};

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
        if(tud_network_can_xmit(0)){
            struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
            memcpy(p->payload, buffer, len);
            tud_network_xmit(p, 0);
            pbuf_free(p); // crucial to prevent memleak
            return ESP_OK;
        }
    }
}
