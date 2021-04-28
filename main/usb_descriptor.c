#include "tinyusb.h"

tusb_desc_device_t desc_device =
{
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = 0x0200,

	// Use Interface Association Descriptor (IAD) device class
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,

	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor           = 0xCafe,
	.idProduct          = 0x3000, //TODO product id
	.bcdDevice          = 0x0101, //device FW version

	.iManufacturer      = 0x02,
	.iProduct           = 0x03,
	.iSerialNumber      = 0x04,

	//.bNumConfigurations = CONFIG_ID_COUNT // multiple configurations
	.bNumConfigurations = 0x02, //2 config (rndis+cdc/ecm)
};

tusb_desc_strarray_device_t desc_string = {
	// array of pointer to string descriptors
	(char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
	"Tech Genesis Enterprise",                  // 1: Manufacturer
	"usb2pppos-rndis",   // 2: Product
	"123458",            // 3: Serials, should use chip ID
};
