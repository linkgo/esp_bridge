#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "driver/uart.h"
#include "user_utils.h"

const char *WIFI_STATUS_STR[] = {
	"IDLE",
	"CONNECTING",
	"WRONG_PASSWORD",
	"NO_AP_FOUND",
	"CONNECT_FAIL",
	"GOT_IP"
};

void wifi_cb(uint8_t status)
{
	log_info("wifi status: %d(%s)\n", status, WIFI_STATUS_STR[status]);
}

void ICACHE_FLASH_ATTR neurite_init(void)
{
	log_info("in\n");
	WIFI_Connect("NETGEAR68", "basicbox565", wifi_cb);
	neurite_cmd_init();
	log_info("out\n");
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(100000);
	system_init_done_cb(neurite_init);
}
