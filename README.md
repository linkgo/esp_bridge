# Neurite
Linkgo Neurite is a WiFi bridge that simply works with the cloud.

## Typical Diagram
```
Neuron - Neurite <-> Cloud <-> Neurite - Neuron
(MCU) (WiFi bridge)         (WiFi bridge) (MCU)
```
## Brief
This Project is based on esp_bridge: https://github.com/tuanpmt/esp_bridge
So it benefits from esp_bridge features:

### Features
- Rock Solid wifi network client for Arduino (of course need to test more and resolve more issues :v)
- **More reliable** than AT COMMAND library (Personal comments)
- **Firmware applications written on ESP8266 can be read out completely. For security applications, sometimes you should use it as a Wifi client (network client) and other MCU with Readout protection.**
- It can be combined to work in parallel with the AT-COMMAND library
- MQTT module: 
    + MQTT client run stable as Native MQTT client (esp_mqtt)
    + Support subscribing, publishing, authentication, will messages, keep alive pings and all 3 QoS levels (it should be a fully functional client).
    + **Support multiple connection (to multiple hosts).**
    + Support SSL
    + Easy to setup and use
- REST module:
    + Support method GET, POST, PUT, DELETE
    + setContent type, set header, set User Agent
    + Easy to used API
    + Support SSL
- WIFI module:

### Original Licence
**LICENSE - "MIT License"**

Copyright (c) 2014-2015 Tuan PM, [https://twitter.com/tuanpmt](https://twitter.com/tuanpmt)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
