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

There are several example applications in the *"examples"* folder. These examples are based on OpenDAQ SDK and allow testing of *MQTTStreamingModule* client/server sides with each other and with third-party MQTT tools.

> ***Note:*** *Using the applications involves using a third-party broker. It must be started before example applications. See a **External MQTT tools** section for more details*

> ***Note:*** *The applications depend on **MQTTStreamingModule** and [**RefDeviceModule**](https://github.com/openDAQ/openDAQ/tree/main/examples/modules/ref_device_module).*

#### ref-dev-mqtt-pub

The *ref-dev-mqtt-pub* application is a console example which publishes *ref-device Signal* samples via the *MQTTStreamingModule* server.

#### ref-dev-mqtt-sub

The *ref-dev-mqtt-sub* application is a console example which subscribes to an available MQTT openDAQ device and prints signal samples.