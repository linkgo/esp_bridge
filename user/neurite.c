#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "ringbuf.h"
#include "driver/uart.h"
#include "user_utils.h"

#define NEURITE_CMD_TASK_QUEUE_SIZE	1
#define NEURITE_CMD_TASK_PRIO		USER_TASK_PRIO_1
#define NEURITE_WORKER_TASK_QUEUE_SIZE	1
#define NEURITE_WORKER_TASK_PRIO	USER_TASK_PRIO_0

struct neurite_data_s {
	bool wifi_connected;
	bool mqtt_connected;
	os_timer_t worker_timer;
};

struct neurite_data_s g_nd;

os_event_t neurite_worker_rx_queue[NEURITE_WORKER_TASK_QUEUE_SIZE];
os_event_t neurite_cmd_rx_queue[NEURITE_CMD_TASK_QUEUE_SIZE];
RINGBUF cmd_rx_rb;
uint8_t cmd_rx_buf[256];

const char *WIFI_STATUS_STR[] = {
	"IDLE",
	"CONNECTING",
	"WRONG_PASSWORD",
	"NO_AP_FOUND",
	"CONNECT_FAIL",
	"GOT_IP"
};

static inline uint32 ICACHE_FLASH_ATTR system_get_time_ms(void)
{
	return system_get_time()/1000;
}

void ICACHE_FLASH_ATTR wifi_cb(uint8_t status)
{
	log_info("wifi status: %d(%s)\n", status, WIFI_STATUS_STR[status]);
	if (status == STATION_GOT_IP) {
		g_nd.wifi_connected = true;
	} else {
		if (status != STATION_IDLE) {
			g_nd.wifi_connected = false;
		}
	}
}

void ICACHE_FLASH_ATTR neurite_cmd_input(uint8_t data)
{
	RINGBUF_Put(&cmd_rx_rb, data);
	system_os_post(NEURITE_CMD_TASK_PRIO, 0, 0);
}

static void ICACHE_FLASH_ATTR neurite_cmd_task(os_event_t *events)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)events->par;
	uint8_t c;
	while(RINGBUF_Get(&cmd_rx_rb, &c) == 0) {
		log_info("input: %c\n", c);
	}
}

void ICACHE_FLASH_ATTR neurite_cmd_init(struct neurite_data_s *nd)
{
	RINGBUF_Init(&cmd_rx_rb, cmd_rx_buf, sizeof(cmd_rx_buf));

	system_os_task(neurite_cmd_task,
			NEURITE_CMD_TASK_PRIO,
			neurite_cmd_rx_queue,
			NEURITE_CMD_TASK_QUEUE_SIZE);

	system_os_post(NEURITE_CMD_TASK_PRIO, 1, (os_param_t)nd);
}

void ICACHE_FLASH_ATTR neurite_net_init(struct neurite_data_s *nd)
{
	WIFI_Connect("NETGEAR68", "basicbox565", wifi_cb);
}

enum worker_state_e {
	WORKER_ST_0 = 0,
	WORKER_ST_1,
	WORKER_ST_2,
	WORKER_ST_3
};

static enum worker_state_e worker_st = WORKER_ST_0;

static void ICACHE_FLASH_ATTR neurite_worker_task(os_event_t *events)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)events->par;

	switch (worker_st) {
		case WORKER_ST_0:
			worker_st = WORKER_ST_1;
			break;
		case WORKER_ST_1:
			worker_st = WORKER_ST_2;
			break;
		case WORKER_ST_2:
			worker_st = WORKER_ST_3;
			break;
		case WORKER_ST_3:
			break;
		default:
			log_err("unknown worker state: %d\n", worker_st);
			break;
	}
}

static inline void ICACHE_FLASH_ATTR worker_timer_handler(void *arg)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)arg;
	system_os_post(NEURITE_WORKER_TASK_PRIO, 1, (os_param_t)nd);
}

void ICACHE_FLASH_ATTR neurite_worker_init(struct neurite_data_s *nd)
{
	system_os_task(neurite_worker_task,
			NEURITE_WORKER_TASK_PRIO,
			neurite_worker_rx_queue,
			NEURITE_WORKER_TASK_QUEUE_SIZE);

	os_timer_disarm(&nd->worker_timer);
	os_timer_setfn(&nd->worker_timer, (os_timer_func_t *)worker_timer_handler, nd);
	os_timer_arm(&nd->worker_timer, 1, 1);
}

void ICACHE_FLASH_ATTR neurite_init(void)
{
	struct neurite_data_s *nd = &g_nd;
	log_info("in\n");
	bzero(nd, sizeof(struct neurite_data_s));
	neurite_worker_init(nd);
	neurite_net_init(nd);
	neurite_cmd_init(nd);
	log_info("out\n");
}
