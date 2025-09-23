#pragma once

#include "common.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE

static const char* MODULE_NAME = "OpenDAQMQTTServerModule";
static const char* MODULE_ID = "OpenDAQMQTTClientModule";
static constexpr const char* SERVER_ID_AND_CAPABILITY = "OpenDAQMQTT";

static constexpr const char* DEFAULT_BROKER_ADDRESS = "127.0.0.1";
static constexpr uint16_t DEFAULT_PORT = 1883;
static constexpr const char* DEFAULT_USERNAME = "";
static constexpr const char* DEFAULT_PASSWORD = "";
static constexpr size_t DEFAULT_MAX_PACKET_READ_COUNT = 1000;
static constexpr uint16_t DEFAULT_POLLING_PERIOD = 20; // milliseconds

static const char* MQTT_PREFIX = "daq.mqtt";
static const char* CONNECTION_TYPE = "TCP/IP";

static constexpr const char* PROPERTY_NAME_MQTT_BROKER_ADDRESS = "MqttBrokerAddress";
static constexpr const char* PROPERTY_NAME_MQTT_BROKER_PORT = "MqttBrokerPort";
static constexpr const char* PROPERTY_NAME_MQTT_USERNAME = "MqttUsername";
static constexpr const char* PROPERTY_NAME_MQTT_PASSWORD = "MqttPassword";
static constexpr const char* PROPERTY_NAME_MAX_PACKET_READ_COUNT = "MaxPacketReadCount";
static constexpr const char* PROPERTY_NAME_POLLING_PERIOD = "StreamingDataPollingPeriod";

static const char* TOPIC_ALL_SIGNALS_PREFIX = "openDAQ";
static const char* DEVICE_SIGNAL_LIST = "$signals";

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE
