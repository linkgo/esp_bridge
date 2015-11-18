#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "driver/uart.h"
#include "user_utils.h"

void ICACHE_FLASH_ATTR neurite_init(void);

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(100000);
	system_init_done_cb(neurite_init);
}
