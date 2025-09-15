# MQTTStreamingModule

## Description

MQTT module for the [OpenDAQ SDK](https://github.com/openDAQ/openDAQ).  


## Building MQTTStreamingModule

### Building on Linux

#### 1. Install all required tools and packages

For example, on **Ubuntu**:

```shell
sudo apt-get update
sudo apt-get install -y git build-essential lld cmake ninja-build mono-complete python3
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

## Testing

There are several example applications in the *"examples"* folder. These examples are based on OpenDAQ SDK and allow testing of *SimpleMQTTModule* functional blocks with each other and with third-party MQTT tools.

> ***Note:*** *Using the applications involves using a third-party broker. It must be started before example applications. See a **External MQTT tools** section for more details*

> ***Note:*** *The applications depend on **MQTTStreamingModule** and [**RefDeviceModule**](https://github.com/openDAQ/openDAQ/tree/main/examples/modules/ref_device_module).*

#### ref-dev-mqtt-pub

The *ref-dev-mqtt-pub* application is a console example which publishes *ref-device Signal* samples via the *MQTTStreamingModule* server. By default it uses the following *MQTTStreamingModule* server settings:
```
StreamingDataPollingPeriod: 20
MaxPacketReadCount: 1000
BrokerAddress: "127.0.0.1"
MqttUsername: ""
MqttPassword: ""
``` 

### External MQTT tools

It is suggested to use [***Eclipse Mosquitto***](https://github.com/eclipse-mosquitto/mosquitto) as a third-party MQTT tool set. It includes MQTT broker and MQTT publisher/subscriber clients. 
Utilities could be installed on **Ubuntu**:

```shell
sudo apt install mosquitto mosquitto-clients
```

The MQTT broker will be run automatically after installing. For simple testing run a subscriber with the following options:

```shell
mosquitto_sub -h 127.0.0.1 -t "openDAQ/#" -v
```
The subscriber will wait for incoming data and then print it. Then run a publisher with the following options:
```shell
mosquitto_pub -h 127.0.0.1 -t "openDAQ/publisher" -m '{"Input0":2, "Input1":1.2, "Input3":3.3}'
```
This command publishes a message and exits. From the subscriber's side you can see:

```shell
user@machine:$ mosquitto_sub -h 127.0.0.1 -t "openDAQ/publisher" -v
openDAQ/publisher {"Input0":2, "Input1":1.2, "Input3":3.3}
```
Now, you can test examples with 3rd-party tools. For example, run *ref-dev-mqtt-pub* and *mosquitto_sub* in different terminals with proper settings. 
```shell
user@machine:$ ./ref-dev-mqtt-pub 
[tid: 29784][2025-09-15 11:17:02.663] [ModuleManager] [info] Loaded module [v3.31.0 ReferenceDeviceModule] from "libref_device_module-64-3-debug.module.so".
[tid: 29784][2025-09-15 11:17:02.664] [ModuleManager] [info] 	DEV [daqref] Reference device: "Reference device"
[tid: 29784][2025-09-15 11:17:02.668] [ModuleManager] [info] Loaded module [v3.4.0 OpenDAQMqttStreamingServerModule] from "libmqtt_stream_srv_module-64-3-debug.module.so".
[tid: 29784][2025-09-15 11:17:02.670] [ModuleManager] [info] 	SRV [OpenDAQMQTT] openDAQ MQTT Streaming server: "Streams data over MQTT"
[tid: 29784][2025-09-15 11:17:03.196] [ReferenceDevice] [info] Properties: NumberOfChannels 2
[tid: 29784][2025-09-15 11:17:03.201] [/RefDev1/IO/AI/RefCh0] [info] Properties: Waveform Sine, Frequency 10, DC 0, Amplitude 5, NoiseAmplitude 0, ConstantValue 2
[tid: 29784][2025-09-15 11:17:03.201] [/RefDev1/IO/AI/RefCh0] [info] Properties: SampleRate 1000, ClientSideScaling false
[tid: 29784][2025-09-15 11:17:03.207] [/RefDev1/IO/AI/RefCh1] [info] Properties: Waveform Sine, Frequency 10, DC 0, Amplitude 5, NoiseAmplitude 0, ConstantValue 2
[tid: 29784][2025-09-15 11:17:03.207] [/RefDev1/IO/AI/RefCh1] [info] Properties: SampleRate 1000, ClientSideScaling false
[tid: 29784][2025-09-15 11:17:03.209] [ReferenceDevice] [info] Properties: AcquisitionLoopTime 20
[tid: 29784][2025-09-15 11:17:03.216] [Instance] [info] Root device set to daqref://device1
[tid: 29784][2025-09-15 11:17:03.231] [ReferenceDevice] [info] Properties: NumberOfChannels 2
[tid: 29784][2025-09-15 11:17:03.235] [/RefDev1/Dev/RefDev0/IO/AI/RefCh0] [info] Properties: Waveform Sine, Frequency 10, DC 0, Amplitude 5, NoiseAmplitude 0, ConstantValue 2
[tid: 29784][2025-09-15 11:17:03.235] [/RefDev1/Dev/RefDev0/IO/AI/RefCh0] [info] Properties: SampleRate 1000, ClientSideScaling false
[tid: 29784][2025-09-15 11:17:03.240] [/RefDev1/Dev/RefDev0/IO/AI/RefCh1] [info] Properties: Waveform Sine, Frequency 10, DC 0, Amplitude 5, NoiseAmplitude 0, ConstantValue 2
[tid: 29784][2025-09-15 11:17:03.240] [/RefDev1/Dev/RefDev0/IO/AI/RefCh1] [info] Properties: SampleRate 1000, ClientSideScaling false
[tid: 29784][2025-09-15 11:17:03.242] [ReferenceDevice] [info] Properties: AcquisitionLoopTime 20
[tid: 29784][2025-09-15 11:17:03.253] [/RefDev1/Dev/RefDev0/IO/AI/ProtectedChannel] [info] Properties: Waveform Sine, Frequency 10, DC 0, Amplitude 5, NoiseAmplitude 0, ConstantValue 2
[tid: 29784][2025-09-15 11:17:03.253] [/RefDev1/Dev/RefDev0/IO/AI/ProtectedChannel] [info] Properties: SampleRate 1000, ClientSideScaling false
[tid: 29784][2025-09-15 11:17:03.265] [OpenDAQMQTT] [info] MQTT: Trying to connect to MQTT broker (127.0.0.1)
[tid: 29784][2025-09-15 11:17:03.267] [OpenDAQMQTT] [info] Adding the Signal to reader: /RefDev1/IO/AI/RefCh0/Sig/AI0;
[tid: 29784][2025-09-15 11:17:03.268] [OpenDAQMQTT] [info] Signal /RefDev1/IO/AI/RefCh0/Sig/AI0Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.268] [OpenDAQMQTT] [info] Adding the Signal to reader: /RefDev1/IO/AI/RefCh1/Sig/AI1;
[tid: 29784][2025-09-15 11:17:03.269] [OpenDAQMQTT] [info] Signal /RefDev1/IO/AI/RefCh1/Sig/AI1Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.269] [OpenDAQMQTT] [info] Signal /RefDev1/Sig/Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.269] [OpenDAQMQTT] [info] Adding the Signal to reader: /RefDev1/Dev/RefDev0/IO/AI/RefCh0/Sig/AI0;
[tid: 29784][2025-09-15 11:17:03.269] [OpenDAQMQTT] [info] Signal /RefDev1/Dev/RefDev0/IO/AI/RefCh0/Sig/AI0Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.269] [OpenDAQMQTT] [info] Adding the Signal to reader: /RefDev1/Dev/RefDev0/IO/AI/RefCh1/Sig/AI1;
[tid: 29784][2025-09-15 11:17:03.270] [OpenDAQMQTT] [info] Signal /RefDev1/Dev/RefDev0/IO/AI/RefCh1/Sig/AI1Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.270] [OpenDAQMQTT] [info] Adding the Signal to reader: /RefDev1/Dev/RefDev0/IO/AI/ProtectedChannel/Sig/AI2;
[tid: 29784][2025-09-15 11:17:03.270] [OpenDAQMQTT] [info] Signal /RefDev1/Dev/RefDev0/IO/AI/ProtectedChannel/Sig/AI2Time doesn't has domain signal assigned, skipping
[tid: 29784][2025-09-15 11:17:03.270] [OpenDAQMQTT] [info] Signal /RefDev1/Dev/RefDev0/Sig/Time doesn't has domain signal assigned, skipping
[tid: 29812][2025-09-15 11:17:03.271] [OpenDAQMQTT] [info] Streaming-to-device read thread started
[tid: 29784][2025-09-15 11:17:03.271] [OpenDAQMQTT] [info] Added Component: /RefDev1/Srv/OpenDAQMQTT;
Press "enter" to exit the application...
[tid: 29811][2025-09-15 11:17:03.276] [OpenDAQMQTT] [info] MQTT: Connection established

```
In this case you can see messages on the *mosquitto_sub* side:

```shell
user@machine:$ mosquitto_sub -h 127.0.0.1 -t "openDAQ/#" -v
openDAQ/RefDev1/$signals ["openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0","openDAQ/RefDev1/IO/AI/RefCh1/Sig/AI1","openDAQ/RefDev1/Dev/RefDev0/IO/AI/RefCh0/Sig/AI0","openDAQ/RefDev1/Dev/RefDev0/IO/AI/RefCh1/Sig/AI1","openDAQ/RefDev1/Dev/RefDev0/IO/AI/ProtectedChannel/Sig/AI2"]
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":1.243449435824274,"timestamp":1757928009227270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":0.9369065729286229,"timestamp":1757928009228270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":0.6266661678215204,"timestamp":1757928009229270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":0.3139525976465657,"timestamp":1757928009230270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-1.6081226496766366e-15,"timestamp":1757928009231270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-0.31395259764656677,"timestamp":1757928009232270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-0.6266661678215214,"timestamp":1757928009233270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-0.9369065729286239,"timestamp":1757928009234270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-1.2434494358242752,"timestamp":1757928009235270}
openDAQ/RefDev1/IO/AI/RefCh0/Sig/AI0 {"value":-1.5450849718747386,"timestamp":1757928009236270}
<...>
```
