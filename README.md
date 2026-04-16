# MQTTStreamingModule

## Description

MQTT module for the [OpenDAQ SDK](https://github.com/openDAQ/openDAQ). The module is designed for software communication via the *MQTT 3.1.1* protocol using an external broker. It allows publishing and subscribing to openDAQ signal data over MQTT. The module consists of five key openDAQ components: the *MQTT client function block* (**MQTTClientFB**) and its nested function blocks — the *publisher* (**MQTTJSONPublisherFB**) and the *subscriber* (**MQTTSubscriberFB**) with its nested block *JSON decoder* (**MQTTJSONDecoderFB**).   

### Functional
- Connecting to an MQTT broker;
- Publishing openDAQ signals as MQTT JSON messages (*publisher FB*);
- Subscribing to MQTT topics and converting incoming messages into openDAQ signals (*subscriber FB, JSON decoder FB*);
- Support for multiple message types and formats for both publishing and subscribing;
- A set of examples and *gtests* for verifying functionality.

### Key components
1) **MQTT client Function Block (MQTTClientFB)**:
   - **Where**: *mqtt_streaming_module/src/mqtt_client_fb_impl.cpp, include/mqtt_streaming_module/...*
   - **Purpose**: Represents the MQTT broker as an openDAQ function block - the connection point through which function blocks are created.
   - **Main properties:**
      - *BrokerAddress* (string) — MQTT broker address. It can be an IP address or a hostname. By default, it is set to *"127.0.0.1"*.
      - *BrokerPort* (integer) — Port number for the MQTT broker connection. By default, it is set to *1883*.
      - *Username* (string) — Username for MQTT broker authentication. By default, it is empty.
      - *Password* (string) — Password for MQTT broker authentication. By default, it is empty.
      - *ConnectionTimeout* (integer) — Timeout in milliseconds for the initial connection to the MQTT broker. If the connection fails, an exception is thrown. By default, it is set to *3000 ms*.
2) **MQTT publisher Function Block (MQTTJSONPublisherFB)**:
   - **Where**: *include/mqtt_streaming_module/mqtt_publisher_fb_impl.h, src/mqtt_publisher_fb_impl.cpp*
   - **Purpose**: Publishes openDAQ signal data to MQTT topics.  
      There are **five** general data publishing schemes:
      1) Raw data publishing. When using this approach, the data is transmitted in raw binary form without any additional wrapper. Domain data is not transmitted.
      2) JSON message data publishing. One MQTT message per signal / one message per sample / one topic per signal / one timestamp for each sample. Example: *{"AI0": 1.1, "timestamp": 1763716736100000}*

      3) JSON message data publishing. One MQTT message per signal / one message containing several samples / one topic per signal / one timestamp per sample (array of samples). Example: *{"AI0": [1.1, 2.2, 3.3], "timestamps": [1763716736100000, 1763716736200000, 1763716736300000]}*
      
      4) JSON message data publishing. One MQTT message for all signals / one message per sample containing all signals / one topic for all signals / one shared timestamp for all signals. Example: *{"AI0": 1.1, "AI1": 2, "timestamp": 1763716736100000}*

      5) JSON message data publishing. One MQTT message for all signals / one message containing several samples for all signals / one topic for all signals / one shared timestamp for all signals (array of samples). Example: *{"AI0": [1.1, 2.2, 3.3], "AI1": [4.1, 4.2, 4.3], "timestamp": [1763716736100000, 1763716736200000, 1763716736300000]}*

      The schemes are configured through combinations of properties.

   - **Main properties**:
      - *Mode* (list) — Selects the mode of publishing (JSON, Raw). Choose *0* for *JSON* mode and *1* for *Raw* mode. In JSON mode, the function block converts signal samples into JSON messages and publishes them to MQTT topics. In Raw mode, the function block publishes raw signal samples to MQTT topics without any conversion. By default it is set to JSON mode.
      - *TopicMode* (list) — The property is used **only** in *JSON* mode. Selects whether to publish all signals to separate MQTT topics (one per signal, *TopicPerSignal mode*) or to a single topic (*SingleTopic mode*), one for all signals. Choose *0* for *TopicPerSignal* mode and *1* for *SingleTopic* mode. By default, it is set to *TopicPerSignal* mode.
      - *QoS* (list) — MQTT Quality of Service level. It can be *0* (at most once), *1* (at least once), or *2* (exactly once). By default, it is set to *1*.
      - *Topic* (string) — The property is used **only** in *JSON* mode. Topic name for publishing in *SingleTopic* mode. If left empty, the Publisher's *Global ID* is used as the topic name.
      - *Topics* (list of strings, read-only) — Contains a list of topics used for publishing data in the *TopicPerSignal* mode. The order in the list is the same as the input ports order.
      - *GroupValues* (bool) — The property is used **only** in *JSON* mode. Enables the use of a sample pack for a signal. By default, it is set to *false*.
      - *SignalValueJSONKey* (list) — The property is used **only** in *JSON* mode. Describes how to name a JSON value field. By default it is set to *GlobalID*.
      - *SamplesPerMessage* (integer) — The property is used **only** in *JSON* mode. Sets the size of the sample pack when publishing grouped values. By default, it is set to *1*.
      - *ReaderWaitPeriod* (integer) — Polling period in milliseconds, specifying how often the server calls an internal reader to collect and publish the connected signals’ data to an MQTT broker. By default, it is set to *20 ms*.
      - *EnablePreviewSignal* (bool) — Enable the creation of preview signals: one signal in *SingleTopic* mode and one signal per connected input port in *TopicPerSignal* mode. These signals contain the same JSON string data that is published to MQTT topics.
      - *Schema* (string, read-only) - Describes the general representation of a JSON data packet according to the current function block settings.

      To configure the publishing schemes, set the properties as follows:
        1) *Mode(1)*;
        2) *Mode(0), TopicMode(0), GroupValues(false)*;
        3) *Mode(0), TopicMode(0), GroupValues(true), SamplesPerMessage(<pack_size>)*;
        4) *Mode(0), TopicMode(1), GroupValues(false)*;
        5) *Mode(0), TopicMode(1), GroupValues(true), SamplesPerMessage(<pack_size>)*;
        

3) **MQTT subscriber Function Block (MQTTSubscriberFB)**:

   - **Where**: *include/mqtt_streaming_module/mqtt_subscriber_fb_impl.h, src/mqtt_subscriber_fb_impl.cpp*
   - **Purpose**: Subscribes to raw MQTT messages and converts them into openDAQ signals (binary data) without any parsing — suitable for binary/unstructured messages, simple numeric values or for further processing by nested blocks (**MQTTJSONDecoderFB**).
   - **Main properties**:
      - *Topic* (string) — MQTT topic to subscribe to for receiving raw binary data.
      - *QoS* (list) — MQTT Quality of Service level. It can be *0* (at most once), *1* (at least once), or *2* (exactly once). By default, it is set to *1*.
      - *EnablePreviewSignal* (bool) — Enable the creation of a preview signal. This signal contains the raw binary data received from an MQTT topic.
      - *DomainMode* (list) — Defines the domain of the preview signal. By default it is set to *None* (0), which means that the preview signal doesn't have a timestamp. If set to *System time* (1), the preview signal's timestamp is set to the system time when the MQTT message is received.
      - *MessageIsString* (bool) — Interpret a received message as a string.
      - *JSONConfigFile* (string) — path to file with **JSON configuration string**. See the *JSONConfig* property for more details. This property could be set only at creation. It is not visible.
      - *JSONConfig* (string) — **JSON configuration string** that defines the MQTT topic and the corresponding signals to subscribe to. This property could be set only at creation. It is not visible. A typical string structure:
      ```json
      {
        "<topic>":[
            {
                "<signal_name>":{
                    "Value":"<field_name_in_JSON_MQTT_message_for_extracting_sample_value>",
                    "Timestamp":"<field_name_in_JSON_MQTT_message_for_extracting_sample_timestamp>",
                    "Unit":[
                        "<unit_symbol>",
                        "<unit_name>",      // is not used
                        "<unit_quantity>"   // is not used
                    ]
                }
            },
            {
                <another_signal>
            }
        ]
      }
      ```
    The *’Timestamp’* and *’Unit’* fields may be omitted. The fields inside *’Unit’* may also be omitted. The `"Value"` and `"Timestamp"` fields support dot-separated paths for accessing nested JSON fields, e.g. `"data.temperature"` or `"info.ts"` (see section 4 for details). Example:
    ```json
    {
        "/mirip/UNet3AC2/sensor/data":[
            {
                "temp":{
                    "Value":"temp",
                    "Timestamp":"ts",
                    "Unit":[
                        "°C"
                    ]
                }
            },
            {
                "humidity":{
                    "Value":"humi",
                    "Timestamp":"ts"
                }
            },
            {
                "tds":{
                    "Value":"tds_value",
                    "Unit":[
                        "ppm", "parts per million", "Total dissolved solids"
                    ]
                }
            }
        ]
    }
    ```
      In this example, the *MQTT subscriber Function Block* creates 3 nested *MQTT JSON decoder Function Block*, subscribes to the *"/mirip/UNet3AC2/sensor/data"* topic, and extracts 3 signal samples from each message (one sample per *jsonDecoderMqttFb*). The *“temp”* signal is created with a domain signal because the *“Timestamp”* field is present. Each domain-signal sample is extracted from the *“ts”* field of the JSON MQTT message. The value of the *“ts”* field (the timestamp field) may be in **ISO8601** format or **Unix epoch time** in seconds, milliseconds, or microseconds. The value of the *“temp”* signal sample is extracted from the *“temp”* field of the JSON message. The unit of the values is “°C”.
      Example of JSON MQTT message for this configuration:
      ```json
      {"ts":"2025-10-08 20:35:43", "bdn":"SanbonFishTank3", "temp":27.20,"humi":72.40, "tds_value":275.22, "fan_status":"off", "auto_mode":"on", "fan_comp":"26.3", "humi_comp":"55"}
      ```

4) **MQTT JSON decoder Function Block (MQTTJSONDecoderFB)**:

   - **Where**: *include/mqtt_streaming_module/mqtt_json_decoder_fb_impl.h, src/mqtt_json_decoder_fb_impl.cpp*
   - **Purpose**: To parse JSON string data to extract a value and a timestamp, and to send data and domain samples based on this data.
   - **Main properties**:
      - *ValueKey* (string) — Specifies the JSON field name (or dot-separated path for nested objects) from which value data will be extracted. Use `.` to access nested fields, e.g. `"data.temperature"` extracts the `temperature` field from inside the `data` object. This property is required. It should be contained in the incoming JSON messages. Otherwise, a parsing error will occur.
      - *DomainMode* (list) —  Defines how the timestamp of the decoded signal is generated. By default it is set to *None* (0), which means that the decoded signal doesn't have a timestamp. If set to *Extract from message* (1), the JSON decoder will try to extract the timestamp from the incoming JSON messages (see *DomainKey* property). If set to *System time* (2), the timestamp of the decoded signal is set to the system time when the JSON message is received.
      - *DomainKey* (string) — Specifies the JSON field name (or dot-separated path for nested objects) from which the timestamp will be extracted. Dot notation is supported, e.g. `"info.timestamp"` extracts `timestamp` from inside the `info` object. This property is optional. If it is set it should be contained in the incoming JSON messages. Otherwise, a parsing error will occur.
      - *Unit* (string) — Specifies the unit symbol for the decoded value. This property is optional.

      Dot-notation paths support arbitrary nesting depth. For example, `"sensor.values.temperature"` traverses `sensor` → `values` → `temperature`.
      Example of a nested JSON MQTT message and the corresponding property values:
      ```json
      {"data": {"temperature": 25.68, "humidity": 72.1}, "info": {"timestamp": 1776332277}}
      ```
      For this message, set *ValueKey* to `"data.temperature"` and *DomainKey* to `"info.timestamp"`.
---

## Building MQTTStreamingModule

### Building on Linux

#### 1. Install all required tools and packages

For example, on **Ubuntu**:

```shell
sudo apt-get update
sudo apt-get install -y git build-essential openssh-client wget curl lld cmake ninja-build mono-complete python3 libssl-dev
```

#### 2. Clone the openDAQ repository

```shell
git clone git@github.com:openDAQ/MQTTStreamingModule.git
cd MQTTStreamingModule
```

#### 3. Generate the CMake project for your specific compiler or preset

In the repository root folder, execute the following command to list available presets.  
Then select the one that fits your needs and generate the CMake project:

```shell
cmake --list-presets=all
cmake --preset "x64/gcc/debug"
```

#### 4. Build the project

```shell
# Build from the repository root
cmake --build build/x64/gcc/debug
# Or move to the build directory
cd build/x64/gcc/debug
cmake --build .
```

---

## Examples   

There are 3 example C++ application:
 - **custom-mqtt-sub** - demonstrates how to work with the *MQTT subscriber MQTT FB* and *MQTT JSON decoder MQTT FB*. The application creates an *MQTTClientFB* and a *MQTTSubscriberFB* with nested *MQTTJSONDecoderFB* function blocks to receive JSON MQTT messages, parse them, and create openDAQ signals to send the parsed data. The application also creates *packet readers* for all FB signals and prints the samples to standard output. The *JSONConfigFile* property of the *MQTTSubscriberFB* is set to the value of path whose is provided as a command-line argument when the application starts (see the **Key components** section). Usage:
  ```bash
 ./custom-mqtt-sub --address broker.emqx.io examples/custom-mqtt-sub/public-example0.json
 ```  
 - **raw-mqtt-sub**  - demonstrates how to work with the *MQTT subscriber MQTT FB* in a raw mode (binary data without parsing). The application creates an *MQTTClientFB* and a *MQTTSubscriberFB* to receive MQTT messages and create openDAQ signals to send the data as binary packets. The application also creates packet readers for all FB signals and prints the binary packets as strings to standard output. The *Topic* property of the *MQTTSubscriberFB* is filled from the application arguments. Usage:
 ```bash
 ./raw-mqtt-sub --address broker.emqx.io /mirip/UNet3AC2/sensor/data
 ```
 - **ref-dev-mqtt-pub** - demonstrates how to work with the *MQTTJSONPublisherFB*. The application creates an *openDAQ ref-device* with four channels, an *MQTTClientFB*, and a *MQTTJSONPublisherFB* to publish JSON MQTT messages with the channels’ data. The properties of the *MQTTJSONPublisherFB* are set according to the selected mode, which can be specified via the *--mode* option, and array size,  which can be specified via the *--array* option with size. Posible *--mode* option values are:
    - 0 - One MQTT topic per signal;
    - 1 - One MQTT message/topic for all signals.   
```bash
 ./ref-dev-mqtt-pub --address broker.emqx.io --mode 1 --array 5
 ```
Published messages can be observed using third-party tools (see the **External MQTT tools** section).
For all applications, by default, the IP address *127.0.0.1* is used for the broker connection. It can be set via the *--address* option, for example:
 ```bash
 ./<app_name> --address 192.168.0.100 <other_options> <args>
 ```   

They are located in the **examples/** directory.
> ***Note:*** *Using the applications involves using a third-party broker. It must be started before example applications. See a **External MQTT tools** section for more details*

> ***Note:*** *The **ref-dev-mqtt-pub** application depends on [**RefDeviceModule**](https://github.com/openDAQ/openDAQ/tree/main/examples/modules/ref_device_module).*


## External MQTT tools

It is suggested to use [***Eclipse Mosquitto***](https://github.com/eclipse-mosquitto/mosquitto) as a third-party MQTT tool set. It includes MQTT broker and MQTT publisher/subscriber clients. 
Utilities could be installed on **Ubuntu**:

```shell
sudo apt install mosquitto mosquitto-clients
```

The MQTT broker will be run automatically after installing. For simple testing run a subscriber with the following options:

```shell
mosquitto_sub -h 127.0.0.1 -t "#" -v
```
The subscriber will wait for incoming data and then print it. Then run a publisher with the following options:
```shell
mosquitto_pub -h 127.0.0.1 -t "openDAQ/publisher" -m '{"Input0":2, "Input1":1.2, "Input3":3.3}'
```
This command publishes a message and exits. From the subscriber's side you can see:

```shell
mosquitto_sub -h 127.0.0.1 -t "openDAQ/publisher" -v
openDAQ/publisher {"Input0":2, "Input1":1.2, "Input3":3.3}
```
