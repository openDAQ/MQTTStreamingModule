#pragma once

#include "common.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

static const char* DaqMqttDeviceTypeId = "OpenDAQMQTTStreaming";
static const char* DaqMqttProtocolId = "OpenDAQMQTTStreaming";
static const char* DaqMqttDevicePrefix = "daq.mqtt";
static const char* MqttScheme = "mqtt";

static const char* MODULE_NAME = "OpenDAQMQTTClientModule";
static const char* MODULE_ID = "OpenDAQMQTTClientModule";
static const char* SHORT_MODULE_NAME = "MQTTClient";
static const char* PROTOCOL_NAME = "OpenDAQMQTT";
static const char* CONNECTION_TYPE = "TCP/IP";

static constexpr const char* DEFAULT_BROKER_ADDRESS = "127.0.0.1";
static constexpr uint16_t DEFAULT_PORT = 1883;
static constexpr const char* DEFAULT_USERNAME = "";
static constexpr const char* DEFAULT_PASSWORD = "";
static constexpr uint32_t DEFAULT_INIT_TIMEOUT = 3000; // ms
static constexpr uint32_t DEFAULT_DISCOVERY_TIMEOUT = 1000; // ms

static constexpr uint32_t DEFAULT_PUB_READ_PERIOD = 20; // ms
static constexpr uint32_t DEFAULT_PUB_QOS = 1;
static constexpr uint32_t DEFAULT_PUB_PACK_SIZE = 1;

static constexpr const char* PROPERTY_NAME_MQTT_BROKER_ADDRESS = "MqttBrokerAddress";
static constexpr const char* PROPERTY_NAME_MQTT_BROKER_PORT = "MqttBrokerPort";
static constexpr const char* PROPERTY_NAME_MQTT_USERNAME = "MqttUsername";
static constexpr const char* PROPERTY_NAME_MQTT_PASSWORD = "MqttPassword";
static constexpr const char* PROPERTY_NAME_CONNECT_TIMEOUT = "ConnectTimeout";
static constexpr const char* PROPERTY_NAME_DISCOVERY_TIMEOUT = "DiscoveryTimeout";
static constexpr const char* PROPERTY_NAME_SIGNAL_LIST = "SignalList";

static constexpr const char* PROPERTY_NAME_PUB_TOPIC_MODE = "TopicMode";
static constexpr const char* PROPERTY_NAME_PUB_SHARED_TS = "SharedTimestamp";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES = "GroupValues";
static constexpr const char* PROPERTY_NAME_PUB_USE_SIGNAL_NAMES = "UseSignalNames";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE = "GroupValuesPackSize";
static constexpr const char* PROPERTY_NAME_PUB_QOS = "MqttQoS";
static constexpr const char* PROPERTY_NAME_PUB_READ_PERIOD = "ReaderPeriod";



static constexpr const char* RAW_FB_NAME = "@rawMqttFb";
static constexpr const char* JSON_FB_NAME = "@jsonMqttFb";
static constexpr const char* PUB_FB_NAME = "@publisherMqttFb";

static const char* TOPIC_ALL_SIGNALS = "openDAQ/+/$signals";

static const char* MQTT_LOCAL_DEVICE_ID_PREFIX = "MqttDevice";
static const char* MQTT_DEVICE_NAME = "MqttStreamingClientPseudoDevice";

static const char* MQTT_LOCAL_PUB_FB_ID_PREFIX = "publisherMqttFb";
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
