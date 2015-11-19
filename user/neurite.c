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

#define NEURITE_CMD_BUF_SIZE		256
#define NEURITE_UID_LEN			32
#define NEURITE_TOPIC_LEN		64

/* external stuffs */
extern SYSCFG sysCfg;

struct cmd_parser_s;
typedef void (*cmd_parser_cb_fp)(struct cmd_parser_s *cp);

struct cmd_parser_s {
	char *buf;
	uint16_t buf_size;
	uint16_t data_len;
	bool cmd_begin;
	cmd_parser_cb_fp callback;
};

struct neurite_mqtt_cfg_s {
	char uid[NEURITE_UID_LEN];
	char topic_to[NEURITE_TOPIC_LEN];
	char topic_from[NEURITE_TOPIC_LEN];
};

struct neurite_data_s {
	bool wifi_connected;
	bool mqtt_connected;
	struct neurite_mqtt_cfg_s nmcfg;
	os_timer_t worker_timer;
	MQTT_Client mc;
	SYSCFG *cfg;
	struct cmd_parser_s *cp;
};

struct neurite_data_s g_nd;
struct cmd_parser_s g_cp;

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
	log_dbg("-> WORKER_ST_%d\n", st);
	worker_st = st;
}

static inline uint32_t ICACHE_FLASH_ATTR system_get_time_ms(void)
{
	return system_get_time()/1000;
}

void ICACHE_FLASH_ATTR neurite_cmd_input(uint8_t data)
{
	RINGBUF_Put(&cmd_rx_rb, data);
	system_os_post(NEURITE_CMD_TASK_PRIO, 1, (os_param_t)&g_nd);
}

static void ICACHE_FLASH_ATTR cmd_completed_cb(struct cmd_parser_s *cp)
{
	dbg_assert(cp);
	if (cp->data_len > 0) {
		log_dbg("msg launch(len %d): %s\n", cp->data_len, cp->buf);
		MQTT_Publish(&g_nd.mc, g_nd.nmcfg.topic_to, cp->buf, cp->data_len, 0, 0);
	}
	os_bzero(cp->buf, cp->buf_size);
	cp->data_len = 0;
}

uint8_t ICACHE_FLASH_ATTR cmd_parser_init(struct cmd_parser_s *cp, cmd_parser_cb_fp complete_cb, uint8_t *buf, uint16_t buf_size)
{
	dbg_assert(cp);
	cp->buf = buf;
	cp->buf_size = buf_size;
	cp->data_len = 0;
	cp->callback = complete_cb;
	return 0;
}

static void ICACHE_FLASH_ATTR cmd_parse_byte(struct cmd_parser_s *cp, char value)
{
	dbg_assert(cp);
	switch (value) {
		case '\r':
			if (cp->callback != NULL)
				cp->callback(cp);
			break;
		default:
			if (cp->data_len < cp->buf_size)
				cp->buf[cp->data_len++] = value;
			break;
	}
}

static void ICACHE_FLASH_ATTR neurite_cmd_task(os_event_t *events)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)events->par;
	uint8_t c;

	dbg_assert(nd);

	while (RINGBUF_Get(&cmd_rx_rb, &c) == 0) {
		os_printf("%c", c);
		cmd_parse_byte(nd->cp, c);
	}
}

void ICACHE_FLASH_ATTR neurite_cmd_init(struct neurite_data_s *nd)
{
	dbg_assert(nd);
	RINGBUF_Init(&cmd_rx_rb, cmd_rx_buf, sizeof(cmd_rx_buf));

	system_os_task(neurite_cmd_task,
			NEURITE_CMD_TASK_PRIO,
			neurite_cmd_rx_queue,
			NEURITE_CMD_TASK_QUEUE_SIZE);

	system_os_post(NEURITE_CMD_TASK_PRIO, 1, (os_param_t)nd);
	uart0_sendStr("linkgo.io\n\r");
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
	log_dbg("status: %d(%s)\n", status, WIFI_STATUS_STR[status]);
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
	dbg_assert(nd);
	WIFI_Connect(nd->cfg->sta_ssid, nd->cfg->sta_pwd, wifi_cb);
}

void mqtt_connected_cb(uint32_t *args)
{
	MQTT_Client *client = (MQTT_Client*)args;
	log_dbg("connected\r\n");
	g_nd.mqtt_connected = true;
}

void mqtt_disconnected_cb(uint32_t *args)
{
	MQTT_Client *client = (MQTT_Client*)args;
	log_dbg("disconnected\r\n");
	g_nd.mqtt_connected = false;
}

void mqtt_published_cb(uint32_t *args)
{
	MQTT_Client *client = (MQTT_Client*)args;
	log_dbg("published\r\n");
}

void mqtt_data_cb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topic_buf = (char *)os_zalloc(topic_len+1);
	char *data_buf = (char *)os_zalloc(data_len+1);
	uint32_t i;

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topic_buf, topic, topic_len);
	topic_buf[topic_len] = 0;

	os_memcpy(data_buf, data, data_len);
	data_buf[data_len] = 0;

	uart0_sendStr(data_buf);
	uart0_write_char('\n');

	log_dbg("> topic (%d): %s\n", topic_len, topic_buf);
	log_dbg("> data (%d): %s\n", data_len, data_buf);

	os_free(topic_buf);
	os_free(data_buf);
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
#if 0
	static uint32_t ms = 0;
	char payload[64];

	dbg_assert(nd);

	if (system_get_time_ms() - ms > 2000) {
		ms = system_get_time_ms();
		os_bzero(payload, 64);
		os_sprintf(payload, "my time is: %d ms", system_get_time_ms());
		MQTT_Publish(&nd->mc, nd->nmcfg.topic_to, payload, strlen(payload), 1, 0);
	}
#endif
	return;
}

static void ICACHE_FLASH_ATTR neurite_worker_task(os_event_t *events)
{
	struct neurite_data_s *nd = (struct neurite_data_s *)events->par;
	dbg_assert(nd);

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
			neurite_cmd_init(nd);
			MQTT_Subscribe(&nd->mc, nd->nmcfg.topic_from, 1);

			uint8_t *payload_buf = (uint8_t *)os_malloc(32);
			dbg_assert(payload_buf);
			os_sprintf(payload_buf, "checkin: %s", nd->nmcfg.uid);
			MQTT_Publish(&nd->mc, nd->nmcfg.topic_to, payload_buf, strlen(payload_buf), 1, 0);
			os_free(payload_buf);

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
	dbg_assert(nd);
	system_os_post(NEURITE_WORKER_TASK_PRIO, 1, (os_param_t)nd);
}

void ICACHE_FLASH_ATTR neurite_worker_init(struct neurite_data_s *nd)
{
	dbg_assert(nd);
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
	uint8_t *cmd_buf = NULL;
	log_dbg("in\n");

	CFG_Load();

	os_bzero(nd, sizeof(struct neurite_data_s));
	nd->cfg = &sysCfg;

	os_bzero(&g_cp, sizeof(struct cmd_parser_s));
	nd->cp = &g_cp;
	cmd_buf = (uint8_t *)os_malloc(NEURITE_CMD_BUF_SIZE);
	dbg_assert(cmd_buf);
	os_bzero(cmd_buf, NEURITE_CMD_BUF_SIZE);
	cmd_parser_init(nd->cp, cmd_completed_cb, cmd_buf, NEURITE_CMD_BUF_SIZE);

	/* TODO these items need to be configured at runtime */
	os_bzero(&nd->nmcfg, sizeof(struct neurite_mqtt_cfg_s));
	os_sprintf(nd->nmcfg.uid, "neurite-%08x", system_get_chip_id());
#if 0
	os_sprintf(nd->nmcfg.topic_to, "/neuro/%s/to", nd->nmcfg.uid);
	os_sprintf(nd->nmcfg.topic_from, "/neuro/%s/to", nd->nmcfg.uid);
#else
	os_sprintf(nd->nmcfg.topic_to, "/neuro/chatroom", nd->nmcfg.uid);
	os_sprintf(nd->nmcfg.topic_from, "/neuro/chatroom", nd->nmcfg.uid);
#endif
//	os_sprintf(nd->cfg->sta_ssid, "%s", STA_SSID);
//	os_sprintf(nd->cfg->sta_pwd, "%s", STA_PASS);
	log_dbg("chip id: %08x\n", system_get_chip_id());
	log_dbg("uid: %s\n", nd->nmcfg.uid);
	log_dbg("topic_to: %s\n", nd->nmcfg.topic_to);
	log_dbg("topic_from: %s\n", nd->nmcfg.topic_from);
	log_dbg("device id: %s\n", nd->cfg->device_id);
	log_dbg("ssid: %s\n", nd->cfg->sta_ssid);
	log_dbg("psk : %s\n", nd->cfg->sta_pwd);
	log_dbg("mqtt user: %s\n", nd->cfg->mqtt_user);
	log_dbg("mqtt pass: %s\n", nd->cfg->mqtt_pass);

//	CFG_Save();

	neurite_worker_init(nd);

	log_dbg("out\n");
}
