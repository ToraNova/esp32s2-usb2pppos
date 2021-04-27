#define CFG_TUD_ENDPOINT0_SIZE    64

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

	.idVendor           = 0xCafe,
	.idProduct          = USB_PID,
	.bcdDevice          = 0x0101,

	.iManufacturer      = STRID_MANUFACTURER,
	.iProduct           = STRID_PRODUCT,
	.iSerialNumber      = STRID_SERIAL,

	.bNumConfigurations = CONFIG_ID_COUNT // multiple configurations
};
