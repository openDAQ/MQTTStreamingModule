#pragma once

#include "common.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

static const char* MODULE_NAME = "OpenDaqMqttModule";
static const char* MODULE_ID = "OpenDaqMqttModule";
static const char* SHORT_MODULE_NAME = "MqttModule";

static constexpr const char* DEFAULT_BROKER_ADDRESS = "127.0.0.1";
static constexpr uint16_t DEFAULT_PORT = 1883;
static constexpr const char* DEFAULT_USERNAME = "";
static constexpr const char* DEFAULT_PASSWORD = "";
static constexpr uint32_t DEFAULT_INIT_TIMEOUT = 3000; // ms

static constexpr uint32_t DEFAULT_PUB_READ_PERIOD = 20; // ms
static constexpr uint32_t DEFAULT_PUB_QOS = 1;
static constexpr uint32_t DEFAULT_PUB_PACK_SIZE = 1;

static constexpr const char* PROPERTY_NAME_MQTT_BROKER_ADDRESS = "MqttBrokerAddress";
static constexpr const char* PROPERTY_NAME_MQTT_BROKER_PORT = "MqttBrokerPort";
static constexpr const char* PROPERTY_NAME_MQTT_USERNAME = "MqttUsername";
static constexpr const char* PROPERTY_NAME_MQTT_PASSWORD = "MqttPassword";
static constexpr const char* PROPERTY_NAME_CONNECT_TIMEOUT = "ConnectTimeout";
static constexpr const char* PROPERTY_NAME_SIGNAL_LIST = "SignalList";
static constexpr const char* PROPERTY_NAME_TOPIC = "Topic";

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
static constexpr const char* ROOT_FB_NAME = "@rootMqttFb";

static const char* MQTT_LOCAL_ROOT_FB_ID_PREFIX = "rootMqttFb";
static const char* MQTT_LOCAL_PUB_FB_ID_PREFIX = "publisherMqttFb";


static const char* MQTT_ROOT_FB_CON_STATUS_TYPE = "BrokerConnectionStatusType";
static const char* MQTT_RAW_FB_SUB_STATUS_TYPE = "MqttSubscriptionStatusType";

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
