/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019
 *    William D. Jones (thor0505@comcast.net),
 *    Ha Thach (tinyusb.org)
 *    Uwe Bonnes (bon@elektron.ikp.physik.tu-darmstadt.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "stm32h7xx_hal.h"
#include "bsp/board.h"

#define LED_STATE_ON          1
#define BUTTON_STATE_ACTIVE   1
#define OTG_FS_VBUS_SENSE     1
#define OTG_HS_VBUS_SENSE     0


//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Despite being call USB2_OTG
// OTG_FS is marked as RHPort0 by TinyUSB to be consistent across stm32 port
void OTG_FS_IRQHandler(void)
{
	tud_int_handler(0);
}

// Despite being call USB2_OTG
// OTG_HS is marked as RHPort1 by TinyUSB to be consistent across stm32 port
void OTG_HS_IRQHandler(void)
{
	tud_int_handler(1);
}

void err(void){
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, 1);
	while(1);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

UART_HandleTypeDef uart2;
UART_HandleTypeDef uart3;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

void board_init(void) {

	//--------------------------------------------------------------------+
	// RCC Clock
	//--------------------------------------------------------------------+
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	/* The PWR block is always enabled on the H7 series- there is no clock
	   enable. For now, use the default VOS3 scale mode (lowest) and limit clock
	   frequencies to avoid potential current draw problems from bus
	   power when using the max clock speeds throughout the chip. */

	/* Enable HSE Oscillator and activate PLL1 with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
	RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = HSE_VALUE/1000000;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	RCC_OscInitStruct.PLL.PLLR = 2; /* Unused */
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_0;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOMEDIUM;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | \
			RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | \
			RCC_CLOCKTYPE_D3PCLK1);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;

	/* Unlike on the STM32F4 family, it appears the maximum APB frequencies are
	   device-dependent- 120 MHz for this board according to Figure 2 of
	   the datasheet. Dividing by half will be safe for now. */
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	/* 4 wait states required for 168MHz and VOS3. */
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);

	/* Like on F4, on H7, USB's actual peripheral clock and bus clock are
	   separate. However, the main system PLL (PLL1) doesn't have a direct
	   connection to the USB peripheral clock to generate 48 MHz, so we do this
	   dance. This will connect PLL1's Q output to the USB peripheral clock. */
	RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct;

	RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
	RCC_PeriphCLKInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
	HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);

	// Enable All GPIOs clocks
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE(); // USB ULPI NXT
	__HAL_RCC_GPIOC_CLK_ENABLE(); // USB ULPI NXT
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE(); // USB ULPI NXT
	__HAL_RCC_GPIOI_CLK_ENABLE(); // USB ULPI NXT
	//__HAL_RCC_GPIOJ_CLK_ENABLE();

	// Enable UART Clock
	//UART_CLK_EN();
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_USART3_CLK_ENABLE();
  	__HAL_RCC_SYSCFG_CLK_ENABLE(); //TODO?
	__HAL_RCC_DMA1_CLK_ENABLE();

	// 1ms tick timer
	SysTick_Config(SystemCoreClock / 1000);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
	// If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
	NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
	NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

	GPIO_InitTypeDef  GPIO_InitStruct;

	// LED
	GPIO_InitStruct.Pin   = GPIO_PIN_0|GPIO_PIN_14;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  	GPIO_InitStruct.Pin = GPIO_PIN_1;
  	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  	GPIO_InitStruct.Pull = GPIO_PULLUP;
  	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	// Button
	GPIO_InitStruct.Pin   = GPIO_PIN_13;
	GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// Uart
	GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	uart2.Instance        = USART2;
	uart2.Init.BaudRate   = CFG_BOARD_UART_BAUDRATE;
	uart2.Init.WordLength = UART_WORDLENGTH_8B;
	uart2.Init.StopBits   = UART_STOPBITS_1;
	uart2.Init.Parity     = UART_PARITY_NONE;
	uart2.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	uart2.Init.Mode       = UART_MODE_TX_RX;
	uart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&uart2) != HAL_OK) err();

	//USART3 initialization
	GPIO_InitStruct.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	uart3.Instance        = USART3;
	uart3.Init.BaudRate   = CFG_BOARD_UART_BAUDRATE;
	uart3.Init.WordLength = UART_WORDLENGTH_8B;
	uart3.Init.StopBits   = UART_STOPBITS_1;
	uart3.Init.Parity     = UART_PARITY_NONE;
	uart3.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	uart3.Init.Mode       = UART_MODE_TX_RX;
	uart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&uart3) != HAL_OK) err();

	HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USART3_IRQn);

#if BOARD_DEVICE_RHPORT_NUM == 0
	// Despite being call USB2_OTG
	// OTG_FS is marked as RHPort0 by TinyUSB to be consistent across stm32 port
	// PA9 VUSB, PA10 ID, PA11 DM, PA12 DP

	/* Configure DM DP Pins */
	GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* This for ID line debug */
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// https://community.st.com/s/question/0D50X00009XkYZLSA3/stm32h7-nucleo-usb-fs-cdc
	// TODO: Board init actually works fine without this line.
	HAL_PWREx_EnableUSBVoltageDetector();
	__HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

#if OTG_FS_VBUS_SENSE
	/* Configure VBUS Pin */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Enable VBUS sense (B device) via pin PA9
	USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
	// Disable VBUS sense (B device) via pin PA9
	USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

	// B-peripheral session valid override enable
	USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
	USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif // vbus sense

#elif BOARD_DEVICE_RHPORT_NUM == 1
	// Despite being call USB2_OTG
	// OTG_HS is marked as RHPort1 by TinyUSB to be consistent across stm32 port

	/* CLK */
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* D0 */
	GPIO_InitStruct.Pin = GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* D1 D2 D3 D4 D5 D6 D7 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* STP */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* NXT */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/* DIR */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	// Enable USB HS & ULPI Clocks
	__HAL_RCC_USB1_OTG_HS_ULPI_CLK_ENABLE();
	__HAL_RCC_USB1_OTG_HS_CLK_ENABLE();

#if OTG_HS_VBUS_SENSE
#error OTG HS VBUS Sense enabled is not implemented
#else
	// No VBUS sense
	USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

	// B-peripheral session valid override enable
	USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
	USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif

	// Force device mode
	USB_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
	USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

	HAL_PWREx_EnableUSBVoltageDetector();
#endif // rhport = 1

}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void) {
	return (BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)) ? 1 : 0;
}

int board_uart_read(uint8_t* buf, int len) {
	//(void) buf; (void) len;
	if (HAL_UART_Receive_IT(&uart3, buf, len) == HAL_BUSY) return 1;
	return 0;
}

int board_uart_write(void const * buf, int len) {
	//(void) buf; (void) len;
	HAL_UART_Transmit(&uart3, (uint8_t*) buf, len, 0xffff);
	//HAL_UART_Transmit_DMA(&uart3, (uint8_t*) buf, len);
	//HAL_UART_Transmit(&uart2, (uint8_t*) buf, len, 0xffff);
	return len;
}

int board_uart_log(const char *format,...){
	char buf[MAX_LOGGING_STRING_SIZE]; uint16_t slen;
	va_list args;
	va_start (args, format);
	vsprintf(buf, format, args);
	va_end(args);
	buf[MAX_LOGGING_STRING_SIZE-1]=0; //null char
	slen = strlen(buf);
	HAL_UART_Transmit(&uart2, (uint8_t*) buf, slen, 0xffff);
	return slen;
}


#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void) {
	HAL_IncTick();
	system_ticks++;
}

uint32_t board_millis(void) {
	return system_ticks;
}
#endif

void HardFault_Handler (void){
	asm("bkpt");
}

extern void user_uart_callback(uint8_t c);
void USART3_IRQHandler(void){
	if(USART3->ISR & UART_IT_RXNE){ //if rx not empty
		//this is from the receive interrupt
		user_uart_callback(USART3->RDR);
	}
	if(USART3->ISR & UART_IT_TXE){
		//transmit smth
	}
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {

}

