#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CFG_HOLDER	0x00FF55A4	/* Change this value to load default configurations */
#define CFG_LOCATION	0x3C		/* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST	"accrete.org"
#define MQTT_PORT	1883
#define MQTT_BUF_SIZE	1024
#define MQTT_KEEPALIVE	120

#define MQTT_CLIENT_ID	"neurite_%08x"
#define MQTT_USER	"neurite-tester"
#define MQTT_PASS	"neurite"

#define STA_SSID	"NETGEAR68"
#define STA_PASS	"basicbox565"
#define STA_TYPE	AUTH_WPA2_PSK

#define MQTT_RECONNECT_TIMEOUT	5

#define DEFAULT_SECURITY	0
#define QUEUE_BUFFER_SIZE	2048

#define PROTOCOL_NAMEv31	/*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311		/*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
//#define INFO
#endif
