#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "cmd.h"
#include "driver/uart.h"
#include "user_utils.h"

void ICACHE_FLASH_ATTR neurite_init(void)
{
	log_info("in\n");
	neurite_cmd_init();
	log_info("out\n");
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	wifi_station_set_auto_connect(TRUE);
	system_init_done_cb(neurite_init);
}
