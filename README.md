# nRF7002dk MQTT example

The nRF7002dk MQTT example implements the necessary function to run MQTT connection, with a previous configuration of the WiFi Stationing task.


# Overview
This sample implements different tasks. Firstly, it is focused on configuring the board to operate as a WiFi Station, establishing a connection with a router to enable internet connectivity. The status of the connection will be checked continuously, and, once the board is connected to the Wifi network, one of the LEDs of the board will be turned on. At this moment, the board will create the MQTT communication, with the following sequential instructions:

- MQTT initialization. The board will initialize the MQTT protocol.

- MQTT connection and subscription. The board will connect to the broker and then, it will subscribe to the specified subscription topic.

- MQTT publish. The board will publish the specified message on the publish topic repeatedly.

- MQTT disconnection. The MQTT disconnection will only occur when there’s a WiFi disconnection, so in normal scenarios, this state is never achieved.

In addition, the example contains callbacks, activated there is a connection, a disconnection, a published message on any of the subscribed topics, or when a message is correctly published.

# Requirments
To use this project it is necessary the Nordic's development kit nRF7002dk, and a WiFi router with stable Internet connection.

The software requirements (i.e. the specific software versions) are detailed on our [“Getting started with nRF7002dk”](https://abluethinginthecloud.com/getting-started-with-nrf7002/) tutorial. It is strongly recommended to read it because it includes instructions to install the IDE and make the necessary configurations to work with the Nordic's development kits.

In addition, to test the example, it is recommended to use any software able to create MQTT sessions, for example, MQTT Explorer.

# Testing

To test the example, the first thing to do is configuring the prj.conf file. This file contains a list of settings the board will implement, and some of them need to be changed depending on the WiFi network configuration, or the desired MQTT configuration

### WiFi Stationing configurations.
- Security level configurations. Select the security level of the router (on the example above, WPA2) and comment the other options.

- CONFIG_STA_SAMPLE_SSID. Introduce the name of the WiFi network.

- CONFIG_STA_SAMPLE_PASSWORD. Introduces the password of the WiFi network.

### MQTT configurations

- CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC. Desired publishing topic

- CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC. Desired subscribing topic

- CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME. Broker’s hostname

- CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_USERNAME. Broker’s username. If the broker has no username, introduce “noUser”

- CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_PASSWORD. Broker’s password. If the broker has no password, introduce “noPassword”

- CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID. Client’s ID

- CONFIG_MQTT_SAMPLE_TRANSPORT_MESSAGE. MQTT message that will be published on the publishing topic.

- CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS. Publishing period in seconds.

Then, build the example with the nrf7002dk configuration, and flash it. To test the working of the MQTT connections, it is proposed to publish and subscribe to crossed topics on MQTT Explorer. 


