/*
 * descriptor configuration
 * copied from tinyusb/examples/device/net_lwip_webserver
 */
#include "tinyusb.h"

tusb_desc_device_t const desc_device =
{
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = 0x0200,

	// Use Interface Association Descriptor (IAD) device class
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,

	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor           = 0xeeee, //vendor ID
	.idProduct          = 0x0001, //product ID
	.bcdDevice          = 0x0101, //device

	.iManufacturer      = 0x01, //manufacturer index in string descriptors
	.iProduct           = 0x02, //product index in string descriptors
	.iSerialNumber      = 0x03, //serial number in string descriptors

	.bNumConfigurations = 0x02 // multiple configurations (RNDIS + ECM)
};

// array of pointer to string descriptors
tusb_desc_strarray_device_t desc_string = {
  [0] = (char[]) { 0x09, 0x04 }, // supported language is English (0x0409)
  [1] = "Tech Genesis Enterprise",     // Manufacturer
  [2] = "usb-rndis-2-pppos",             // Product
  [3] = "000001",                      // Serial
  [4] = "USB PPPOS Network Interface"  // Interface Description
  // STRID_MAC index is handled separately
};

// usb rndis/emc MAC address
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};
