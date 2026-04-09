#pragma once

#include <string>

namespace Mqtt::Utils::Settings {
struct MqttConnectionSettings {
    std::string mqttUrl;
    int port;
    std::string username;
    std::string password;
    std::string clientId;
};
}
