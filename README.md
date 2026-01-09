# MQTTStreamingModule

## Description

MQTT module for the [OpenDAQ SDK](https://github.com/openDAQ/openDAQ). The module is designed for software communication via the *MQTT 3.1.1* protocol using an external broker. It allows publishing and subscribing to openDAQ signal data over MQTT. The module consists of five key openDAQ components: the *MQTT root function block* (**rootMqttFb**) and its nested function blocks — the *publisher* (**publisherMqttFb**) with its nested block *JSON decoder* (**jsonDecoderMqttFb**) , the *raw subscriber* (**rawMqttFb**), and the *JSON subscriber* (**jsonMqttFb**).   

### Functional
- Connecting to an MQTT broker;
- Publishing openDAQ signals as MQTT messages (*publisher FB*);
- Subscribing to MQTT topics and converting incoming messages into openDAQ signals (*raw FB and JSON FB + JSON decoder FB*);
- Support for multiple message types and formats for both publishing and subscribing;
- A set of examples and *gtests* for verifying functionality.

### Key components
1) **MQTT root Function Block (rootMqttFb)**:
   - **Where**: *mqtt_streaming_module/src/mqtt_root_fb_impl.cpp, include/mqtt_streaming_module/...*
   - **Purpose**: Represents the MQTT broker as an openDAQ function block - the connection point through which function blocks are created.
   - **Main properties:**
      - *MqttBrokerAddress* (string) - MQTT broker address. It can be an IP address or a hostname. By default, it is set to *"127.0.0.1"*.
      - *MqttBrokerPort* (integer) - Port number for the MQTT broker connection. By default, it is set to *1883*.
      - *MqttUsername* (string) — Username for MQTT broker authentication. By default, it is empty.
      - *MqttPassword* (string) — Password for MQTT broker authentication. By default, it is empty.
      - *ConnectTimeout* (integer) — Timeout in milliseconds for the initial connection to the MQTT broker. If the connection fails, an exception is thrown. By default, it is set to *3000 ms*.
2) **Publisher MQTT Function Block (publisherMqttFb)**:
   - **Where**: *include/mqtt_streaming_module/mqtt_publisher_fb_impl.h, src/mqtt_publisher_fb_impl.cpp*
   - **Purpose**: Publishes openDAQ signal data to MQTT topics. There are **four** general data publishing schemes:
      1) One MQTT message per signal / one message per sample / one topic per signal / one timestamp for each sample. Example: *{"AI0": 1.1, "timestamp": 1763716736100000}*

      2) One MQTT message per signal / one message containing several samples / one topic per signal / one timestamp per sample (array of samples). Example: *{"AI0": [1.1, 2.2, 3.3], "timestamps": [1763716736100000, 1763716736200000, 1763716736300000]}*

      3) One MQTT message for several signals (from 1 to N) / one message per sample for each signal / one topic for all signals / separate timestamps for each signal. Example: *[{"AI0": 1.1, "timestamp": 1763716736100000}, {"AI1": 2, "timestamp": 1763716736700000}]*
      
      4) One MQTT message for all signals / one message per sample containing all signals / one topic for all signals / one shared timestamp for all signals. Example: *{"AI0": 1.1, "AI1": 2, "timestamp": 1763716736100000}*

      The schemes are configured through combinations of properties.

   - **Main properties**:
      - *TopicMode* (list) — Selects whether to publish all signals to separate MQTT topics (one per signal, *single-topic mode*) or to a single topic (*multiple-topic mode*), one for all signals. Choose *0* for *single-topic* mode and *1* for *multiple-topic* mode. By default, it is set to *single-topic* mode.
      - *MqttQoS* (integer) — MQTT Quality of Service level. It can be *0* (at most once), *1* (at least once), or *2* (exactly once). By default, it is set to *1*.
      - *Topic* (string) — Topic name for publishing in multiple-topic mode. If left empty, the Publisher's *Global ID* is used as the topic name.
      - *SharedTimestamp* (bool) — Enables the use of a shared timestamp for all signals when publishing in *multiple-topic* mode. By default, it is set to *false*.
      - *GroupValues* (bool) — Enables the use of a sample pack for a signal when publishing in *single-topic* mode. By default, it is set to *false*.
      - *UseSignalNames* (bool) — Uses signal names as JSON field names instead of Global IDs. By default, it is set to *false*.
      - *GroupValuesPackSize* (integer) — Sets the size of the sample pack when publishing grouped values in *single-topic* mode. By default, it is set to *1*.
      - *ReaderPeriod* (integer) — Polling period in milliseconds, specifying how often the server collects and publishes the connected signals’ data to an MQTT broker. By default, it is set to *20 ms*.

      To configure the publishing schemes, set the properties as follows:
        1) *TopicMode(0), SharedTimestamp(false), GroupValues(false)*;
        2) *TopicMode(0), SharedTimestamp(false), GroupValues(true), GroupValuesPackSize(<pack_size>)*;
        3) *TopicMode(1), SharedTimestamp(false), GroupValues(false)*;
        4) *TopicMode(1), SharedTimestamp(true), GroupValues(false)*;
        

3) **Raw MQTT Function Block (rawMqttFb)**:

   - **Where**: *include/mqtt_streaming_module/mqtt_raw_receiver_fb_impl.h, src/mqtt_raw_receiver_fb_impl.cpp*
   - **Purpose**: Subscribes to raw MQTT messages and converts them into openDAQ signals (binary data) without any parsing — suitable for binary/unstructured messages or simple numeric values.
   - **Main properties**:
      - *Topic* (string) — MQTT topic to subscribe to for receiving raw binary data.
      - *MqttQoS* (integer) — MQTT Quality of Service level. It can be *0* (at most once), *1* (at least once), or *2* (exactly once). By default, it is set to *1*.

4) **JSON MQTT Function Block (jsonMqttFb)**:
   - **Where**: *include/mqtt_streaming_module/mqtt_json_receiver_fb_impl.h, src/mqtt_json_receiver_fb_impl.cpp*
   - **Purpose**: Subscribes to MQTT topics, extracts values and timestamps from MQTT JSON messages via nested *JSON decoder MQTT Function Blocks*.
   - **Main properties**:
      - *Topic* (string) — MQTT topic to subscribe to for receiving JSON data.
      - *MqttQoS* (integer) — MQTT Quality of Service level. It can be *0* (at most once), *1* (at least once), or *2* (exactly once). By default, it is set to *1*.
      - *JsonConfigFile* (string) — path to file with **JSON configuration string**. See the *JsonConfig* property for more details. This property could be set only at creation. It is not visible.
      - *JsonConfig* (string) — **JSON configuration string** that defines the MQTT topic and the corresponding signals to subscribe to. This property could be set only at creation. It is not visible. A typical string structure:
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
    The *‘Timestamp’* and *‘Unit’* fields may be omitted. The fields inside *‘Unit’* may also be omitted. Example:
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
      In this example, the *JSON MQTT Function Block* creates 3 nested *jsonDecoderMqttFb*, subscribes to the *"/mirip/UNet3AC2/sensor/data"* topic, and extracts 3 signal samples from each message (one sample per *jsonDecoderMqttFb*). The signals are named *“temp”*, *“humidity”*, and *“tds”*. The *“temp”* signal is created with a domain signal because the *“Timestamp”* field is present. Each domain-signal sample is extracted from the *“ts”* field of the JSON MQTT message. The value of the *“ts”* field (the timestamp field) may be in **ISO8601** format or **Unix epoch time** in seconds, milliseconds, or microseconds. The value of the *“temp”* signal sample is extracted from the *“temp”* field of the JSON message. The unit of the values is “°C”.
      Example of JSON MQTT message for this configuration:
      ```json
      {"ts":"2025-10-08 20:35:43", "bdn":"SanbonFishTank3", "temp":27.20,"humi":72.40, "tds_value":275.22, "fan_status":"off", "auto_mode":"on", "fan_comp":"26.3", "humi_comp":"55"}
      ```

5) **JSON decoder MQTT Function Block (jsonDecoderMqttFb)**:

   - **Where**: *include/mqtt_streaming_module/mqtt_json_decoder_fb_impl.h, src/mqtt_json_decoder_fb_impl.cpp*
   - **Purpose**: To parse JSON string data to extract a value and a timestamp, and to send data and domain samples based on this data.
   - **Main properties**:
      - *ValueName* (string) — indicates which JSON field contains the sample value.
      - *TimestampName* (string) — indicates which JSON field contains the timestamp.
      - *Unit* (string) — describes the unit symbol of the decoded signal value.
      - *SignalName* (string) — specifies the name to assign to the signal created by a *jsonDecoderMqttFb*.
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
 - **custom-mqtt-sub** - demonstrates how to work with the *JSON receiver MQTT FB* and *JSON decoder MQTT FB*. The application creates an *MQTT root FB* and a *JSON MQTT FB* to receive JSON MQTT messages, parse them, and create openDAQ signals to send the parsed data. The application also creates *packet readers* for all FB signals and prints the samples to standard output. The *JsonConfigFile* property of the JSON MQTT FB is set to the value of path whose is provided as a command-line argument when the application starts (see the **Key components** section). Usage:
  ```bash
 ./custom-mqtt-sub --address broker.emqx.io examples/custom-mqtt-sub/public-example0.json
 ```  
 - **raw-mqtt-sub**  - demonstrates how to work with the *raw MQTT FB*. The application creates an *MQTT root FB* and a *raw MQTT FB* to receive MQTT messages and create openDAQ signals to send the data as binary packets. The application also creates packet readers for all FB signals and prints the binary packets as strings to standard output. The *Topic* property of the raw MQTT FB is filled from the application arguments. Usage:
 ```bash
 ./raw-mqtt-sub --address broker.emqx.io /mirip/UNet3AC2/sensor/data
 ```
 - **ref-dev-mqtt-pub** - demonstrates how to work with the *publisher MQTT FB*. The application creates an *openDAQ ref-device* with four channels, an *MQTT root FB*, and a *publisher MQTT FB* to publish JSON MQTT messages with the channels’ data. The properties of the *publisher MQTT FB* are set according to the selected mode, which can be specified via the *--mode* option. Posible values are:
    - 0 - One MQTT message per signal / one message per sample / one topic per signal / one timestamp for each sample;
    - 1 - One MQTT message per signal / one message containing several samples / one topic per signal / one timestamp per sample (array of samples);
    - 2 - One MQTT message for several signals (from 1 to N) / one message per sample for each signal / one topic for all signals / separate timestamps for each signal;
    - 3 - One MQTT message for all signals / one message per sample containing all signals / one topic for all signals / one shared timestamp for all signals.
```bash
 ./ref-dev-mqtt-pub --address broker.emqx.io --mode 1
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
