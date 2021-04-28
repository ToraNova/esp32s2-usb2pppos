/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

/** \ingroup group_demo
 * \defgroup group_board Boards Abstraction Layer
 *  @{ */

#ifndef _BSP_BOARD_H_
#define _BSP_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

#include "ansi_escape.h"
#include "tusb.h"

#define CFG_BOARD_UART_BAUDRATE    115200
#define MAX_LOGGING_STRING_SIZE    1024

	//--------------------------------------------------------------------+
	// Board Porting API
	// For simplicity, only one LED and one Button are used
	//--------------------------------------------------------------------+

	// Initialize on-board peripherals : led, button, uart and USB
	void board_init(void);

	// Turn LED on or off
	void board_led_write(bool state);

	// Get the current state of button
	// a '1' means active (pressed), a '0' means inactive.
	uint32_t board_button_read(void);

	// Get characters from UART
	int board_uart_read(uint8_t* buf, int len);

	// Send characters to UART
	int board_uart_write(void const * buf, int len);

	int board_uart_log(const char *,...) __attribute__ ((format (printf, 1, 2)));

#if CFG_TUSB_OS == OPT_OS_NONE
	// Get current milliseconds, must be implemented when no RTOS is used
	uint32_t board_millis(void);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
	static inline uint32_t board_millis(void)
	{
		return ( ( ((uint64_t) xTaskGetTickCount()) * 1000) / configTICK_RATE_HZ );
	}

#elif CFG_TUSB_OS == OPT_OS_MYNEWT
	static inline uint32_t board_millis(void)
	{
		return os_time_ticks_to_ms32( os_time_get() );
	}

#elif CFG_TUSB_OS == OPT_OS_PICO
#include "pico/time.h"
	static inline uint32_t board_millis(void)
	{
		return to_ms_since_boot(get_absolute_time());
	}
#elif CFG_TUSB_OS == OPT_OS_RTTHREAD
	static inline uint32_t board_millis(void)
	{
		return (((uint64_t)rt_tick_get()) * 1000 / RT_TICK_PER_SECOND);
	}

#else
#error "board_millis() is not implemented for this OS"
#endif

	//--------------------------------------------------------------------+
	// Helper functions
	//--------------------------------------------------------------------+
	static inline void board_led_on(void)
	{
		board_led_write(true);
	}

	// delay vias millis
	static inline void board_delay(uint32_t ms){
		uint32_t a = board_millis();
		while(board_millis() - a < ms){
			//TODO: maybe we can run a task here?
#if CFG_TUSB_OS == OPT_OS_NONE
			tud_task();
#endif
		}
		return;
	}

	static inline void board_led_off(void)
	{
		board_led_write(false);
	}

	static inline int board_uart_getchar(void)
	{
		uint8_t c;
		return board_uart_read(&c, 1) ? (int) c : (-1);
	}

	static inline int board_uart_putchar(uint8_t c)
	{
		return board_uart_write(&c, 1);
	}

#ifdef __cplusplus
}
#endif

#endif /* _BSP_BOARD_H_ */

/** @} */
