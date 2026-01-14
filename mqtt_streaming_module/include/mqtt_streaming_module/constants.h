#pragma once

#include "common.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

static const char* MODULE_NAME = "OpenDAQMQTTModule";
static const char* MODULE_ID = "OpenDAQMQTTModule";
static const char* SHORT_MODULE_NAME = "MQTTModule";

static constexpr const char* DEFAULT_BROKER_ADDRESS = "127.0.0.1";
static constexpr uint16_t DEFAULT_PORT = 1883;
static constexpr const char* DEFAULT_USERNAME = "";
static constexpr const char* DEFAULT_PASSWORD = "";
static constexpr uint32_t DEFAULT_INIT_TIMEOUT = 3000; // ms

static constexpr uint32_t DEFAULT_PUB_READ_PERIOD = 20; // ms
static constexpr uint32_t DEFAULT_PUB_QOS = 1;
static constexpr uint32_t DEFAULT_SUB_QOS = 1;
static constexpr uint32_t DEFAULT_PUB_PACK_SIZE = 1;

static constexpr const char* DEFAULT_SIGNAL_NAME = "MQTTValueSignal";

static constexpr const char* PROPERTY_NAME_MQTT_BROKER_ADDRESS = "MQTTBrokerAddress";
static constexpr const char* PROPERTY_NAME_MQTT_BROKER_PORT = "MQTTBrokerPort";
static constexpr const char* PROPERTY_NAME_MQTT_USERNAME = "MQTTUsername";
static constexpr const char* PROPERTY_NAME_MQTT_PASSWORD = "MQTTPassword";
static constexpr const char* PROPERTY_NAME_CONNECT_TIMEOUT = "ConnectTimeout";
static constexpr const char* PROPERTY_NAME_SIGNAL_LIST = "SignalList";
static constexpr const char* PROPERTY_NAME_JSON_CONFIG = "JSONConfig";
static constexpr const char* PROPERTY_NAME_JSON_CONFIG_FILE = "JSONConfigFile";
static constexpr const char* PROPERTY_NAME_TOPIC = "Topic";
static constexpr const char* PROPERTY_NAME_VALUE_NAME = "ValueName";
static constexpr const char* PROPERTY_NAME_TS_NAME = "TimestampName";
static constexpr const char* PROPERTY_NAME_UNIT = "Unit";
static constexpr const char* PROPERTY_NAME_SIGNAL_NAME = "SignalName";


static constexpr const char* PROPERTY_NAME_PUB_TOPIC_MODE = "TopicMode";
static constexpr const char* PROPERTY_NAME_PUB_TOPIC_NAME = "Topic";
static constexpr const char* PROPERTY_NAME_PUB_SHARED_TS = "SharedTimestamp";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES = "GroupValues";
static constexpr const char* PROPERTY_NAME_PUB_USE_SIGNAL_NAMES = "UseSignalNames";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE = "GroupValuesPackSize";
static constexpr const char* PROPERTY_NAME_PUB_QOS = "QoS";
static constexpr const char* PROPERTY_NAME_SUB_QOS = "QoS";
static constexpr const char* PROPERTY_NAME_PUB_READ_PERIOD = "ReaderPeriod";

static constexpr const char* RAW_FB_NAME = "RawSubscriberMQTTFB";
static constexpr const char* JSON_FB_NAME = "JSONSubscriberMQTTFB";
static constexpr const char* PUB_FB_NAME = "PublisherMQTTFB";
static constexpr const char* ROOT_FB_NAME = "MQTTClientFB";
static constexpr const char* JSON_DECODER_FB_NAME = "JSONDecoderMQTTTFB";

static const char* MQTT_LOCAL_ROOT_FB_ID_PREFIX = "MQTTClientFB";
static const char* MQTT_LOCAL_PUB_FB_ID_PREFIX = "PublisherMQTTFB";
static const char* MQTT_LOCAL_RAW_FB_ID_PREFIX = "RawSubscriberMQTTFB";
static const char* MQTT_LOCAL_JSON_FB_ID_PREFIX = "JSONSubscriberMQTTFB";
static const char* MQTT_LOCAL_JSON_DECODER_FB_ID_PREFIX = "JSONDecoderMQTTTFB";


static const char* MQTT_ROOT_FB_CON_STATUS_TYPE = "BrokerConnectionStatusType";
static const char* MQTT_FB_SUB_STATUS_TYPE = "MQTTSubscriptionStatusType";
static const char* MQTT_PUB_FB_SIG_STATUS_TYPE = "MQTTSignalStatusType";
static const char* MQTT_PUB_FB_PUB_STATUS_TYPE = "MQTTPublishingStatusType";
static const char* MQTT_FB_PARSING_STATUS_TYPE = "MQTTParsingStatusType";
static const char* MQTT_PUB_FB_SET_STATUS_TYPE = "MQTTSettingStatusType";


static const char* MQTT_ROOT_FB_CON_STATUS_NAME = "ConnectionStatus";
static const char* MQTT_PUB_FB_SIG_STATUS_NAME = "SignalStatus";
static const char* MQTT_PUB_FB_PUB_STATUS_NAME = "PublishingStatus";
static const char* MQTT_FB_SUB_STATUS_NAME = "SubscriptionStatus";
static const char* MQTT_FB_PARSING_STATUS_NAME = "ParsingStatus";
static const char* MQTT_PUB_FB_SET_STATUS_NAME = "SettingStatus";

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
