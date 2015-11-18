#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "ringbuf.h"
#include "driver/uart.h"
#include "user_utils.h"
#include "mqtt.h"
#include "config.h"

#define NEURITE_CMD_TASK_QUEUE_SIZE	1
#define NEURITE_CMD_TASK_PRIO		USER_TASK_PRIO_2
#define NEURITE_WORKER_TASK_QUEUE_SIZE	1
#define NEURITE_WORKER_TASK_PRIO	USER_TASK_PRIO_1

/* external stuffs */
extern SYSCFG sysCfg;

struct neurite_data_s {
	bool wifi_connected;
	bool mqtt_connected;
	os_timer_t worker_timer;
	MQTT_Client mc;
	SYSCFG *cfg;
};

struct neurite_data_s g_nd;

os_event_t neurite_worker_rx_queue[NEURITE_WORKER_TASK_QUEUE_SIZE];
os_event_t neurite_cmd_rx_queue[NEURITE_CMD_TASK_QUEUE_SIZE];
RINGBUF cmd_rx_rb;
uint8_t cmd_rx_buf[256];

enum worker_state_e {
	WORKER_ST_0 = 0,
	WORKER_ST_1,
	WORKER_ST_2,
	WORKER_ST_3
};

static enum worker_state_e worker_st = WORKER_ST_0;

static inline void update_worker_state(int st)
{
	log_info("-> WORKER_ST_%d\n", st);
	worker_st = st;
}

static inline uint32 ICACHE_FLASH_ATTR system_get_time_ms(void)
{
	return system_get_time()/1000;
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

const char *WIFI_STATUS_STR[] = {
	"IDLE",
	"CONNECTING",
	"WRONG_PASSWORD",
	"NO_AP_FOUND",
	"CONNECT_FAIL",
	"GOT_IP"
};

static void ICACHE_FLASH_ATTR wifi_cb(uint8_t status)
{
	log_info("status: %d(%s)\n", status, WIFI_STATUS_STR[status]);
	if (status == STATION_GOT_IP) {
		g_nd.wifi_connected = true;
	} else {
		if (status != STATION_IDLE) {
			g_nd.wifi_connected = false;
		}
	}
}

void ICACHE_FLASH_ATTR neurite_wifi_connect(struct neurite_data_s *nd)
{
	WIFI_Connect(nd->cfg->sta_ssid, nd->cfg->sta_pwd, wifi_cb);
}

void mqtt_connected_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	log_info("connected\r\n");
	g_nd.mqtt_connected = true;
}

void mqtt_disconnected_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	log_info("disconnected\r\n");
	g_nd.mqtt_connected = false;
}

void mqtt_published_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	log_info("published\r\n");
}

void mqtt_data_cb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char *)os_zalloc(topic_len+1);
	char *dataBuf = (char *)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	log_info("> topic (%d): %s\n", topic_len, topicBuf);
	log_info("> data (%d): %s\n", data_len, dataBuf);
	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR neurite_mqtt_connect(struct neurite_data_s *nd)
{
	MQTT_InitConnection(&nd->mc, nd->cfg->mqtt_host, nd->cfg->mqtt_port, nd->cfg->security);
	MQTT_InitClient(&nd->mc, nd->cfg->device_id, nd->cfg->mqtt_user, nd->cfg->mqtt_pass, nd->cfg->mqtt_keepalive, 1);
	MQTT_InitLWT(&nd->mc, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&nd->mc, mqtt_connected_cb);
	MQTT_OnDisconnected(&nd->mc, mqtt_disconnected_cb);
	MQTT_OnPublished(&nd->mc, mqtt_published_cb);
	MQTT_OnData(&nd->mc, mqtt_data_cb);
	MQTT_Connect(&nd->mc);
}

void ICACHE_FLASH_ATTR neurite_child_worker(struct neurite_data_s *nd)
{
	return;
}

static void ICACHE_FLASH_ATTR neurite_worker_task(os_event_t *events)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)events->par;

	switch (worker_st) {
		case WORKER_ST_0:
			neurite_wifi_connect(nd);
			update_worker_state(WORKER_ST_1);
			break;
		case WORKER_ST_1:
			if (!nd->wifi_connected)
				break;
			update_worker_state(WORKER_ST_2);
			neurite_mqtt_connect(nd);
			break;
		case WORKER_ST_2:
			if (!nd->mqtt_connected)
				break;
			update_worker_state(WORKER_ST_3);
			MQTT_Subscribe(&nd->mc, "/mqtt/topic/0", 0);
			MQTT_Subscribe(&nd->mc, "/mqtt/topic/1", 1);
			MQTT_Subscribe(&nd->mc, "/mqtt/topic/2", 2);
			MQTT_Publish(&nd->mc, "/mqtt/topic/0", "hello0", 6, 0, 0);
			MQTT_Publish(&nd->mc, "/mqtt/topic/1", "hello1", 6, 1, 0);
			MQTT_Publish(&nd->mc, "/mqtt/topic/2", "hello2", 6, 2, 0);
			break;
		case WORKER_ST_3:
			neurite_child_worker(nd);
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
	/* FIXME wake up every 1ms, too frequent? */
	os_timer_arm(&nd->worker_timer, 1, 1);
}

void ICACHE_FLASH_ATTR neurite_init(void)
{
	struct neurite_data_s *nd = &g_nd;
	log_info("in\n");
	bzero(nd, sizeof(struct neurite_data_s));
	nd->cfg = &sysCfg;
	CFG_Load();
	neurite_worker_init(nd);
	neurite_cmd_init(nd);
	log_info("out\n");
}
