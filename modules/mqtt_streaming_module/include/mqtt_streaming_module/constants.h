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
static constexpr uint32_t DEFAULT_SUB_DATA_INTERVAL = 5000; // ms

static constexpr const char* DEFAULT_VALUE_SIGNAL_LOCAL_ID = "MQTTValueSignal";
static constexpr const char* DEFAULT_TS_SIGNAL_LOCAL_ID = "MQTTTimestampSignal";


static constexpr const char* PROPERTY_NAME_CLIENT_BROKER_ADDRESS = "BrokerAddress";
static constexpr const char* PROPERTY_NAME_CLIENT_BROKER_PORT = "BrokerPort";
static constexpr const char* PROPERTY_NAME_CLIENT_USERNAME = "Username";
static constexpr const char* PROPERTY_NAME_CLIENT_PASSWORD = "Password";
static constexpr const char* PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT = "ConnectionTimeout";

static constexpr const char* PROPERTY_NAME_SUB_JSON_CONFIG = "JSONConfig";
static constexpr const char* PROPERTY_NAME_SUB_JSON_CONFIG_FILE = "JSONConfigFile";
static constexpr const char* PROPERTY_NAME_SUB_QOS = "QoS";
static constexpr const char* PROPERTY_NAME_SUB_TOPIC = "Topic";
static constexpr const char* PROPERTY_NAME_SUB_PREVIEW_SIGNAL = "EnablePreviewSignal";
static constexpr const char* PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE = "DomainMode";
static constexpr const char* PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING = "MessageIsString";
static constexpr const char* PROPERTY_NAME_SUB_DATA_INTERVAL = "DataInterval";

static constexpr const char* PROPERTY_NAME_DEC_VALUE_NAME = "ValueKey";
static constexpr const char* PROPERTY_NAME_DEC_TS_MODE = "DomainMode";
static constexpr const char* PROPERTY_NAME_DEC_TS_NAME = "DomainKey";
static constexpr const char* PROPERTY_NAME_DEC_UNIT = "Unit";

static constexpr const char* PROPERTY_NAME_PUB_MODE = "Mode";
static constexpr const char* PROPERTY_NAME_PUB_TOPIC_MODE = "TopicMode";
static constexpr const char* PROPERTY_NAME_PUB_TOPIC_NAME = "Topic";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES = "GroupValues";
static constexpr const char* PROPERTY_NAME_PUB_VALUE_FIELD_NAME = "SignalValueJSONKey";
static constexpr const char* PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE = "SamplesPerMessage";
static constexpr const char* PROPERTY_NAME_PUB_QOS = "QoS";
static constexpr const char* PROPERTY_NAME_PUB_READ_PERIOD = "ReaderWaitPeriod";
static constexpr const char* PROPERTY_NAME_PUB_TOPICS = "Topics";
static constexpr const char* PROPERTY_NAME_PUB_SCHEMA = "Schema";
static constexpr const char* PROPERTY_NAME_PUB_PREVIEW_SIGNAL = "EnablePreviewSignal";
static constexpr const char* PUB_PREVIEW_SIGNAL_NAME = "PreviewSignal";

static constexpr const char* SUB_FB_NAME = "MQTTSubscriberFB";
static constexpr const char* PUB_FB_NAME = "MQTTJSONPublisherFB";
static constexpr const char* CLIENT_FB_NAME = "MQTTClientFB";
static constexpr const char* JSON_DECODER_FB_NAME = "MQTTJSONDecoderFB";

static const char* MQTT_LOCAL_CLIENT_FB_ID_PREFIX = "MQTTClientFB";
static const char* MQTT_LOCAL_PUB_FB_ID_PREFIX = "MQTTJSONPublisherFB";
static const char* MQTT_LOCAL_SUB_FB_ID_PREFIX = "MQTTSubscriberFB";
static const char* MQTT_LOCAL_JSON_DECODER_FB_ID_PREFIX = "MQTTJSONDecoderFB";


static const char* MQTT_CLIENT_FB_CON_STATUS_TYPE = "DAQ_MQTT_ConnectionStatusType";
static const char* MQTT_PUB_FB_SIG_STATUS_TYPE = "DAQ_MQTT_SignalStatusType";
static const char* MQTT_PUB_FB_PUB_STATUS_TYPE = "DAQ_MQTT_PublishingStatusType";
static const char* MQTT_PUB_FB_SET_STATUS_TYPE = "DAQ_MQTT_SettingStatusType";


static const char* MQTT_CLIENT_FB_CON_STATUS_NAME = "ConnectionStatus";
static const char* MQTT_PUB_FB_SIG_STATUS_NAME = "SignalStatus";
static const char* MQTT_PUB_FB_PUB_STATUS_NAME = "PublishingStatus";
static const char* MQTT_PUB_FB_SET_STATUS_NAME = "SettingStatus";

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
