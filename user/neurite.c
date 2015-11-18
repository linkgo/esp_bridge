#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "cmd.h"
#include "ringbuf.h"
#include "driver/uart.h"
#include "user_utils.h"

#define NEURITE_CMD_TASK_QUEUE_SIZE	1
#define NEURITE_CMD_TASK_PRIO		1

os_event_t neurite_cmd_rx_queue[NEURITE_CMD_TASK_QUEUE_SIZE];
RINGBUF cmd_rx_rb;
uint8_t cmd_rx_buf[256];

void ICACHE_FLASH_ATTR neurite_cmd_input(uint8_t data)
{
	RINGBUF_Put(&cmd_rx_rb, data);
	system_os_post(CMD_TASK_PRIO, 0, 0);
}

static void ICACHE_FLASH_ATTR neurite_cmd_task(os_event_t *events)
{
	uint8_t c;
	while(RINGBUF_Get(&cmd_rx_rb, &c) == 0) {
		log_info("input: %c\n", c);
	}
}

void ICACHE_FLASH_ATTR neurite_cmd_init(void)
{
	RINGBUF_Init(&cmd_rx_rb, cmd_rx_buf, sizeof(cmd_rx_buf));

	system_os_task(neurite_cmd_task,
			NEURITE_CMD_TASK_PRIO,
			neurite_cmd_rx_queue,
			NEURITE_CMD_TASK_QUEUE_SIZE);

	system_os_post(NEURITE_CMD_TASK_PRIO, 0, 0);
}
