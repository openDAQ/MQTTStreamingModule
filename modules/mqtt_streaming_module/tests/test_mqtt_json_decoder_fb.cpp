#include "MqttAsyncClientWrapper.h"
#include "mqtt_streaming_helper/timer.h"
#include "mqtt_streaming_module/mqtt_subscriber_fb_impl.h"
#include "mqtt_streaming_module/mqtt_json_decoder_fb_impl.h"
#include "test_daq_test_helper.h"
#include "test_data.h"
#include "timestampConverter.h"
#include <cmath>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>
#include <chrono>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace
{
template <typename T>
struct is_pair : std::false_type
{
};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type
{
};

bool almostEqual(double a, double b, double relEpsilon = 1e-9, double absEpsilon = 1e-12)
{
    return std::fabs(a - b) <= std::max(absEpsilon, relEpsilon * std::max(std::fabs(a), std::fabs(b)));
}
template <typename T>
bool almostEqual(const std::vector<T>& a, const std::vector<T>& b, double relEpsilon = 1e-9, double absEpsilon = 1e-12)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (!almostEqual(a[i], b[i], relEpsilon, absEpsilon))
            return false;
    }
    return true;
}

template <typename T>
bool equal(const std::vector<T>& a, const std::vector<T>& b)
{
    if (a.size() != b.size())
        return false;
    if constexpr (std::is_same_v<double, T> || std::is_same_v<float, T>)
    {
        return almostEqual(a, b);
    }
    else
    {
        return a == b;
    }
}

template <typename T>
bool equal(T a, T b)
{
    if constexpr (std::is_same_v<double, T> || std::is_same_v<float, T>)
    {
        return almostEqual(a, b);
    }
    else
    {
        return a == b;
    }
}

template <typename T>
bool equalTs(T a, T b)
{
    const auto convertedA = mqtt::utils::numericToMicroseconds(a);
    const auto convertedB = mqtt::utils::numericToMicroseconds(b);
    return (convertedA == convertedB && convertedA != 0);
}

template <typename T>
bool equalTs(const std::vector<T>& a, const std::vector<T>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (!equalTs<T>(a[i], b[i]))
            return false;
    }
    return true;
}

bool equalTs(const std::string& a, uint64_t b)
{
    return (mqtt::utils::toUnixTicks(a) == b && b != 0);
}

bool equalTs(uint64_t b, const std::string& a)
{
    return equalTs(a, b);
}

bool equalTs(const std::vector<std::string>& a, const std::vector<uint64_t>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (!equalTs(a[i], b[i]))
            return false;
    }
    return true;
}

bool equalTs(const std::vector<uint64_t>& b, const std::vector<std::string>& a)
{
    return equalTs(a, b);
}

std::string doubleToString(double value, int precision = 12)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

template <typename vT, typename tsT>
void merge(const std::vector<std::pair<vT, tsT>>& input, std::vector<std::pair<std::vector<vT>, std::vector<tsT>>>& output)
{
    std::vector<vT> data;
    std::vector<tsT> ts;
    for (const auto& [sData, sTs] : input)
    {
        data.push_back(sData);
        ts.push_back(sTs);
    }
    output.emplace_back(std::pair(std::move(data), std::move(ts)));
}

template <typename T>
std::string replacePlaceholder(const std::string& jsonTemplate, const std::string& ph, const T& value)
{
    std::string result = jsonTemplate;
    size_t pos = result.find(ph);
    if (pos != std::string::npos)
    {
        std::string replacement;
        if constexpr (std::is_same_v<T, std::string>)
        {
            replacement = '"' + value + '"';
        }
        else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
        {
            replacement = doubleToString(value);
        }
        else if constexpr (mqtt::is_std_vector_v<T>)
        {
            replacement = "[";
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (i > 0)
                    replacement += ", ";
                if constexpr (std::is_same_v<mqtt::sample_type_t<T>, double> || std::is_same_v<mqtt::sample_type_t<T>, float>)
                    replacement += doubleToString(value[i]);
                else if constexpr (std::is_same_v<mqtt::sample_type_t<T>, std::string>)
                    replacement += '"' + value[i] + '"';
                else
                    replacement += std::to_string(value[i]);
            }
            replacement += "]";
        }
        else
        {
            replacement = std::to_string(value);
        }
        result.replace(pos, ph.length(), replacement);
    }
    return result;
}

template <typename vT, typename tsT>
std::vector<std::string> replacePlaceholders(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonTemplate)
{
    std::vector<std::string> result;
    for (const auto& [value, ts] : data)
    {
        auto str = replacePlaceholder(jsonTemplate, "<placeholder_value>", value);
        str = replacePlaceholder(str, "<placeholder_ts>", ts);
        result.push_back(str);
    }
    return result;
}

std::string extractFieldName(std::string jsonTemplate, const std::string& valuePh)
{
    std::string result;
    size_t pos = jsonTemplate.find(valuePh);
    if (pos == std::string::npos)
        return "";
    size_t posEnd = jsonTemplate.rfind("\"", pos);
    if (posEnd == std::string::npos)
        return "";
    size_t posStart = jsonTemplate.rfind("\"", posEnd - 1);
    if (posStart == std::string::npos)
        return "";
    ++posStart;
    result = jsonTemplate.substr(posStart, posEnd - posStart);
    return result;
}

template <typename vT, typename tsT0, typename tsT1>
bool compareData(const std::vector<std::pair<vT, tsT0>>& data0, const std::vector<std::pair<vT, tsT1>>& data1, bool compareTs = true)
{
    if (data0.size() != data1.size())
        return false;
    for (std::size_t i = 0; i < data0.size(); ++i)
    {
        const auto& [value0, ts0] = data0[i];
        const auto& [value1, ts1] = data1[i];
        if (!equal(value0, value1))
            return false;
        if (compareTs)
        {
            if (!equalTs(ts0, ts1))
                return false;
        }
    }
    return true;
}

template <typename vT, typename tsT>
bool compareData(const std::vector<std::pair<vT, tsT>>& data0, const std::vector<vT>& data1)
{
    if (data0.size() != data1.size())
        return false;
    for (std::size_t i = 0; i < data0.size(); ++i)
    {
        const auto& [value0, _] = data0[i];
        const auto& value1 = data1[i];

        if (!equal(value0, value1))
            return false;
    }
    return true;
}

template <typename T>
bool copyData(T& destination, const DataPacketPtr source)
{
    auto checkType = [](SampleType type) -> bool
    {
        switch (type)
        {
            case SampleType::Float32:
            case SampleType::Float64:
            case SampleType::UInt8:
            case SampleType::Int8:
            case SampleType::UInt16:
            case SampleType::Int16:
            case SampleType::UInt32:
            case SampleType::Int32:
            case SampleType::UInt64:
            case SampleType::Int64:
            case SampleType::RangeInt64:
            case SampleType::ComplexFloat32:
            case SampleType::ComplexFloat64:
                return true;
            case SampleType::String:
            case SampleType::Binary:
            case SampleType::Struct:
            case SampleType::Invalid:
            case SampleType::Null:
            case SampleType::_count:
                return false;
        }
        return true;
    };

    const auto dataType = source.getDataDescriptor().getSampleType();
    if (checkType(dataType) && getSampleSize(dataType) != sizeof(mqtt::sample_type_t<T>))
        return false;
    if constexpr (std::is_same_v<T, std::string>)
    {
        destination = std::string(static_cast<char*>(source.getData()), source.getDataSize());
    }
    else if constexpr (mqtt::is_std_vector_v<T>)
    {
        destination.resize(source.getSampleCount());
        memcpy(destination.data(), source.getRawData(), source.getSampleCount() * getSampleSize(dataType));
    }
    else
    {
        memcpy(&destination, source.getData(), sizeof(destination));
    }
    return true;
}

template <typename T>
std::vector<T> read(PacketReaderPtr reader, const SignalPtr signal, int timeoutMs = 1000)
{
    std::vector<T> result;

    auto timer = helper::utils::Timer(timeoutMs);
    while (!reader.getEmpty() || !timer.expired())
    {
        if (reader.getEmpty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        auto packet = reader.read();
        if (packet.getType() == PacketType::Event)
        {
            continue;
        }

        if (packet.getType() == PacketType::Data)
        {
            const auto dataPacket = packet.asPtr<IDataPacket>();
            if constexpr (is_pair<T>::value)
            {
                T dataToReceiveEntry;
                bool ok = true;
                ok &= copyData(dataToReceiveEntry.first, dataPacket);
                if (signal.getDomainSignal().assigned())
                    ok &= copyData(dataToReceiveEntry.second, dataPacket.getDomainPacket());
                if (!ok)
                    break;
                result.push_back(dataToReceiveEntry);
            }
            else
            {
                T dataToReceiveEntry;
                bool ok = copyData(dataToReceiveEntry, dataPacket);
                if (!ok)
                    break;
                result.push_back(dataToReceiveEntry);
            }
        }
    }

    return result;
}
} // namespace

namespace daq::modules::mqtt_streaming_module
{
class MqttJsonDecoderFbHelper : public DaqTestHelper
{
public:
    daq::FunctionBlockPtr decoderObj;

    void onSignalsMessage(const mqtt::MqttMessage& msg)
    {
        mqtt::MqttAsyncClient unused;
        auto fb = reinterpret_cast<MqttSubscriberFbImpl*>(*subMqttFb);
        fb->onSignalsMessage(unused, msg);
    }

    void CreateJsonFb(const std::string& topic)
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, String("")));
        const auto fbType = FunctionBlockType(SUB_FB_NAME, SUB_FB_NAME, "", config);
        config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic);
        subMqttFb = new MqttSubscriberFbImpl(NullContext(), nullptr, fbType, nullptr, config);
    }

    void CreateDecoderFB(std::string topic, std::string valueF, std::string tsF, std::string unitSymbol = "")
    {
        CreateJsonFb(topic);
        AddDecoderFb(valueF, tsF, unitSymbol);
    }

    daq::FunctionBlockPtr AddDecoderFb(std::string valueF, std::string tsF, std::string unitSymbol = "")
    {
        daq::StringPtr typeId = daq::String(JSON_DECODER_FB_NAME);
        auto config = subMqttFb.getAvailableFunctionBlockTypes().get(JSON_DECODER_FB_NAME).createDefaultConfig();

        config.setPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME, valueF);
        config.setPropertyValue(PROPERTY_NAME_DEC_TS_NAME, tsF);
        config.setPropertyValue(PROPERTY_NAME_DEC_UNIT, unitSymbol);
        decoderObj = subMqttFb.addFunctionBlock(typeId, config);
        return decoderObj;
    }

    auto getSignals()
    {
        return decoderObj.getSignals();
    }

    std::string buildTopicName(const std::string& postfix = "")
    {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + postfix;
    }

    std::string buildClientId()
    {
        return std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "_ClientId";
    }

    template <typename vT, typename tsT>
    std::vector<std::pair<vT, uint64_t>> transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, std::pair<vT, uint64_t>>(data, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    std::vector<std::pair<std::vector<vT>, std::vector<uint64_t>>>
    transferData(const std::vector<std::pair<std::vector<vT>, std::vector<tsT>>>& data, const std::string& jsonDataTemplate)
    {
        return transferData<std::vector<vT>, std::vector<tsT>, std::pair<std::vector<vT>, std::vector<uint64_t>>>(data, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    std::vector<vT> transferDataWithoutDomain(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, vT>(data, jsonDataTemplate);
    }

private:
    template <typename vT, typename tsT, typename returnT> std::vector<returnT> transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonDataTemplate)
    {
        const auto topic = buildTopicName();
        std::string valueF = extractFieldName(jsonDataTemplate, "<placeholder_value>");
        std::string tsF;
        if constexpr (is_pair<returnT>::value)
        {
            tsF = extractFieldName(jsonDataTemplate, "<placeholder_ts>");
        }

        CreateDecoderFB(topic, valueF, tsF);

        auto signal = getSignals()[0];
        auto reader = daq::PacketReader(signal);

        auto msgs = replacePlaceholders(data, jsonDataTemplate);
        for (const auto& str : msgs)
        {
            onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
        }

        std::vector<returnT> dataToReceive = read<returnT>(reader, signal, 0);
        return dataToReceive;
    }
};

class MqttJsonDecoderFbTest : public testing::Test, public MqttJsonDecoderFbHelper
{
};

class MqttJsonFbCommunicationTest : public testing::Test, public MqttJsonDecoderFbHelper
{
public:
    using data_set_t = std::vector<std::pair<double, uint64_t>>;

    struct Result
    {
        bool mqttFbProblem = false;
        bool publishingProblem = false;
        data_set_t dataReceived;
    };

    Result processTransfer(const std::string& url, const uint16_t port, const std::string& topic, const data_set_t& dataSet, daq::SignalPtr signal)
    {
        Result result;

        auto reader = daq::PacketReader(signal);

        MqttAsyncClientWrapper publisher(buildClientId());
        result.mqttFbProblem = !publisher.connect(url + ':' + std::to_string(port));
        if (result.mqttFbProblem)
            return result;

        for (const auto& [value, ts] : dataSet)
        {
            auto str = VALID_JSON_DATA_0;
            str = replacePlaceholder(str, "<placeholder_ts>", ts);
            str = replacePlaceholder(str, "<placeholder_value>", value);
            result.publishingProblem = !publisher.publishMsg({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
            if (result.publishingProblem)
                return result;
        }
        result.dataReceived = read<std::pair<double, uint64_t>>(reader, signal, 2000);
        return result;
    };
};

class MqttJsonFbRightJsonConfigPTest : public ::testing::TestWithParam<std::pair<std::string, int>>,
                                       public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbWrongJsonConfigPTest : public ::testing::TestWithParam<std::string>, public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbDoubleDataPTest : public ::testing::TestWithParam<std::vector<std::pair<double, uint64_t>>>,
                                  public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbIntDataPTest : public ::testing::TestWithParam<std::vector<std::pair<int64_t, uint64_t>>>,
                               public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbStringDataPTest : public ::testing::TestWithParam<std::vector<std::pair<std::string, uint64_t>>>,
                                  public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbStringTsPTest : public ::testing::TestWithParam<std::vector<std::pair<double, std::string>>>,
                                public MqttJsonDecoderFbHelper
{
};
class MqttJsonFbUnitPTest : public ::testing::TestWithParam<std::pair<std::string, std::vector<std::string>>>,
                            public MqttJsonDecoderFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttJsonDecoderFbTest, DefaultConfig)
{
    StartUp();
    AddSubFb(buildTopicName());
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = subMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(JSON_DECODER_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 3u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_DEC_VALUE_NAME));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_DEC_VALUE_NAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME).asPtr<IString>().getLength(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_DEC_TS_NAME));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_DEC_TS_NAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_DEC_TS_NAME).asPtr<IString>().getLength(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_DEC_UNIT));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_DEC_UNIT).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_DEC_UNIT).asPtr<IString>().getLength(), 0u);
}

TEST_F(MqttJsonDecoderFbTest, Config)
{
    StartUp();
    AddSubFb(buildTopicName());
    auto config = subMqttFb.getAvailableFunctionBlockTypes().get(JSON_DECODER_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME, "value");
    config.setPropertyValue(PROPERTY_NAME_DEC_TS_NAME, "timestamp");
    config.setPropertyValue(PROPERTY_NAME_DEC_UNIT, "ppm");
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = subMqttFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
    EXPECT_EQ(fb.getSignals().getCount(), 1u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(fb.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Waiting for data"), std::string::npos);
    const auto allProperties = fb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(fb.hasProperty(propName));
        ASSERT_EQ(fb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttJsonDecoderFbTest, CreationWithDefaultConfig)
{
    StartUp();
    AddSubFb(buildTopicName());
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = subMqttFb.addFunctionBlock(JSON_DECODER_FB_NAME));
    EXPECT_EQ(fb.getSignals().getCount(), 1u);
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(fb.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Configuration is invalid"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, CreationWithPartialConfig)
{
    StartUp();
    AddSubFb(buildTopicName());
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_DEC_VALUE_NAME, String("value")));
        ASSERT_NO_THROW(fb = subMqttFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
        EXPECT_EQ(fb.getSignals().getCount(), 1u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
        EXPECT_NE(fb.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Waiting for data"), std::string::npos);
        subMqttFb.removeFunctionBlock(fb);
    }
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_DEC_TS_NAME, String("ts")));
        ASSERT_NO_THROW(fb = subMqttFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
        EXPECT_EQ(fb.getSignals().getCount(), 1u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
        EXPECT_NE(fb.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Configuration is invalid"), std::string::npos);
        subMqttFb.removeFunctionBlock(fb);
    }
}

TEST_P(MqttJsonFbDoubleDataPTest, DataTransferOneSignalDouble)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_P(MqttJsonFbDoubleDataPTest, DataTransferOneSignalDoubleWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalDouble,
                         MqttJsonFbDoubleDataPTest,
                         ::testing::Values(DATA_DOUBLE_INT_0, DATA_DOUBLE_INT_1, DATA_DOUBLE_INT_2));

TEST_F(MqttJsonDecoderFbTest, DataTransferOneSignalDoubleArray)
{
    std::vector<std::pair<std::vector<double>, std::vector<uint64_t>>> dataToSend;
    merge(DATA_DOUBLE_INT_0, dataToSend);
    merge(DATA_DOUBLE_INT_1, dataToSend);
    merge(DATA_DOUBLE_INT_2, dataToSend);

    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, DataTransferOneSignalDoubleArrayWithoutDomain)
{
    std::vector<std::pair<std::vector<double>, std::vector<uint64_t>>> dataToSend;
    merge(DATA_DOUBLE_INT_0, dataToSend);
    merge(DATA_DOUBLE_INT_1, dataToSend);
    merge(DATA_DOUBLE_INT_2, dataToSend);

    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, DataTransferOneSignalIntArray)
{
    std::vector<std::pair<std::vector<int64_t>, std::vector<uint64_t>>> dataToSend;
    merge(DATA_INT_INT_0, dataToSend);
    merge(DATA_INT_INT_1, dataToSend);
    merge(DATA_INT_INT_2, dataToSend);

    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, DataTransferOneSignalIntArrayWithoutDomain)
{
    std::vector<std::pair<std::vector<int64_t>, std::vector<uint64_t>>> dataToSend;
    merge(DATA_INT_INT_0, dataToSend);
    merge(DATA_INT_INT_1, dataToSend);
    merge(DATA_INT_INT_2, dataToSend);

    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, DataTransferOneSignalDoubleArrayDomainString)
{
    std::vector<std::pair<std::vector<double>, std::vector<std::string>>> dataToSend;
    merge(DATA_DOUBLE_STR_0, dataToSend);
    merge(DATA_DOUBLE_STR_1, dataToSend);
    merge(DATA_DOUBLE_STR_2, dataToSend);

    auto dataToReceive = transferData<>(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_P(MqttJsonFbIntDataPTest, DataTransferOneSignalInt)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_P(MqttJsonFbIntDataPTest, DataTransferOneSignalIntWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalInt,
                         MqttJsonFbIntDataPTest,
                         ::testing::Values(DATA_INT_INT_0, DATA_INT_INT_1, DATA_INT_INT_2));

TEST_P(MqttJsonFbStringDataPTest, DataTransferOneSignalString)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_P(MqttJsonFbStringDataPTest, DataTransferOneSignalStringWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalString,
                         MqttJsonFbStringDataPTest,
                         ::testing::Values(DATA_STR_INT_0, DATA_STR_INT_1));

TEST_P(MqttJsonFbStringTsPTest, DataTransferOneSignalIntDomainString)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalInt,
                         MqttJsonFbStringTsPTest,
                         ::testing::Values(DATA_DOUBLE_STR_0, DATA_DOUBLE_STR_1, DATA_DOUBLE_STR_2));

TEST_F(MqttJsonDecoderFbTest, DataTransferSeveralSignals)
{
    const auto msgTemplate = VALID_JSON_DATA_2;
    const std::string valueF0 = extractFieldName(msgTemplate, "<placeholder_temperature>");
    const std::string valueF1 = extractFieldName(msgTemplate, "<placeholder_humi>");
    const std::string valueF2 = extractFieldName(msgTemplate, "<placeholder_pr>");
    const std::string tsF = extractFieldName(msgTemplate, "<placeholder_ts>");
    const auto topic = buildTopicName();

    DaqInstanceInit();
    auto clientFb0 = DaqAddClientMqttFb("127.0.0.1", DEFAULT_PORT);
    auto jsonFb0 = AddSubFb(topic);
    auto decoderFb0 = AddDecoderFb(valueF0, tsF);
    auto decoderFb1 = AddDecoderFb(valueF1, tsF);
    auto decoderFb2 = AddDecoderFb(valueF2, "");

    auto signalList = List<ISignal>();
    signalList.pushBack(decoderFb0.getSignals()[0]);
    signalList.pushBack(decoderFb1.getSignals()[0]);
    signalList.pushBack(decoderFb2.getSignals()[0]);

    std::vector<std::pair<PacketReaderPtr, SignalPtr>> readers;

    for (const auto& signal : signalList)
    {
        readers.emplace_back(std::pair<PacketReaderPtr, SignalPtr>(daq::PacketReader(signal), signal));
    }

    for (int cnt = 0; cnt < DATA_DOUBLE_INT_0.size(); ++cnt)
    {
        auto str = VALID_JSON_DATA_2;
        str = replacePlaceholder(str, "<placeholder_ts>", DATA_DOUBLE_INT_0[cnt].second);
        str = replacePlaceholder(str, "<placeholder_temperature>", DATA_DOUBLE_INT_0[cnt].first);
        str = replacePlaceholder(str, "<placeholder_humi>", DATA_DOUBLE_INT_1[cnt].first);
        str = replacePlaceholder(str, "<placeholder_pr>", DATA_DOUBLE_INT_2[cnt].first);
        onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
    }

    std::vector<std::vector<std::pair<double, uint64_t>>> dataToReceive(readers.size());
    for (int i = 0; i < readers.size(); ++i)
    {
        auto& [reader, signal] = readers[i];
        dataToReceive[i] = read<std::pair<double, uint64_t>>(reader, signal, 0);
    }
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), dataToReceive[0].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_0, dataToReceive[0]));
    ASSERT_EQ(decoderFb0.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb0.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);

    EXPECT_EQ(DATA_DOUBLE_INT_1.size(), dataToReceive[1].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_1, dataToReceive[1]));
    ASSERT_EQ(decoderFb1.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb1.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);

    EXPECT_EQ(DATA_DOUBLE_INT_2.size(), dataToReceive[2].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_2, dataToReceive[2], false));
    ASSERT_EQ(decoderFb2.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb2.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);

}

TEST_F(MqttJsonDecoderFbTest, DataTransferMissingFieldOneSignal)
{
    const auto topic = buildTopicName();
    std::string valueF = extractFieldName(VALID_JSON_DATA_1, "<placeholder_value>");
    std::string tsF = "ts";
    CreateDecoderFB(topic, valueF, tsF);

    auto signal = getSignals()[0];
    auto reader = daq::PacketReader(signal);

    for (int cnt = 0; cnt < DATA_DOUBLE_INT_0.size(); ++cnt)
    {
        auto str = VALID_JSON_DATA_1;
        str = replacePlaceholder(str, "<placeholder_value>", DATA_DOUBLE_INT_0[cnt].first);
        onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
    }
    std::vector<std::pair<double, uint64_t>> dataToReceive = read<std::pair<double, uint64_t>>(reader, signal, 0);
    ASSERT_EQ(dataToReceive.size(), 0);
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", decoderObj.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing failed"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, DataTransferMissingFieldSeveralSignals)
{
    const auto msgTemplate = VALID_JSON_DATA_2;
    const std::string valueF0 = extractFieldName(msgTemplate, "<placeholder_temperature>");
    const std::string valueF1 = extractFieldName(msgTemplate, "<placeholder_humi>");
    const std::string valueF2 = extractFieldName(msgTemplate, "<placeholder_pr>");
    const std::string tsF = extractFieldName(msgTemplate, "<placeholder_ts>");
    const auto topic = buildTopicName();

    DaqInstanceInit();
    auto clientFb0 = DaqAddClientMqttFb("127.0.0.1", DEFAULT_PORT);
    auto jsonFb0 = AddSubFb(topic);
    auto decoderFb0 = AddDecoderFb(valueF0, tsF);
    auto decoderFb1 = AddDecoderFb(valueF1, tsF);
    auto decoderFb2 = AddDecoderFb(valueF2, "");

    auto signalList = List<ISignal>();
    signalList.pushBack(decoderFb0.getSignals()[0]);
    signalList.pushBack(decoderFb1.getSignals()[0]);
    signalList.pushBack(decoderFb2.getSignals()[0]);

    std::vector<std::pair<PacketReaderPtr, SignalPtr>> readers;

    for (const auto& signal : signalList)
    {
        readers.emplace_back(std::pair<PacketReaderPtr, SignalPtr>(daq::PacketReader(signal), signal));
    }

    for (int cnt = 0; cnt < DATA_DOUBLE_INT_0.size(); ++cnt)
    {
        auto str = MISSING_FIELD_JSON_DATA_2;
        str = replacePlaceholder(str, "<placeholder_ts>", DATA_DOUBLE_INT_0[cnt].second);
        str = replacePlaceholder(str, "<placeholder_temperature>", DATA_DOUBLE_INT_0[cnt].first);
        str = replacePlaceholder(str, "<placeholder_pr>", DATA_DOUBLE_INT_2[cnt].first);
        onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
    }

    std::vector<std::vector<std::pair<double, uint64_t>>> dataToReceive(readers.size());
    for (int i = 0; i < readers.size(); ++i)
    {
        auto& [reader, signal] = readers[i];
        dataToReceive[i] = read<std::pair<double, uint64_t>>(reader, signal, 0);
    }
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), dataToReceive[0].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_0, dataToReceive[0]));
    ASSERT_EQ(decoderFb0.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb0.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);

    EXPECT_EQ(0u, dataToReceive[1].size());
    ASSERT_EQ(decoderFb1.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb1.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing failed"), std::string::npos);

    EXPECT_EQ(DATA_DOUBLE_INT_2.size(), dataToReceive[2].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_2, dataToReceive[2], false));
    ASSERT_EQ(decoderFb2.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb2.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonFbCommunicationTest, FullDataTransfer)
{
    StartUp("127.0.0.1", DEFAULT_PORT);
    const std::string topic = buildTopicName();
    const auto msgTemplate = VALID_JSON_DATA_0;
    const std::string valueF = extractFieldName(msgTemplate, "<placeholder_value>");
    const std::string tsF = extractFieldName(msgTemplate, "<placeholder_ts>");
    AddSubFb(topic);
    AddDecoderFb(valueF, tsF);

    const auto result = processTransfer("127.0.0.1", DEFAULT_PORT, topic, DATA_DOUBLE_INT_0, decoderObj.getSignals()[0]);

    ASSERT_FALSE(result.mqttFbProblem);
    ASSERT_FALSE(result.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), result.dataReceived.size());
    ASSERT_TRUE(compareData(DATA_DOUBLE_INT_0, result.dataReceived));
    ASSERT_EQ(decoderObj.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderObj.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonFbCommunicationTest, FullDataTransferFor2MqttFbs)
{
    const auto msgTemplate = VALID_JSON_DATA_0;
    const std::string valueF = extractFieldName(msgTemplate, "<placeholder_value>");
    const std::string tsF = extractFieldName(msgTemplate, "<placeholder_ts>");
    const std::string topic0 = buildTopicName("0");
    const std::string topic1 = buildTopicName("1");

    DaqInstanceInit();
    auto clientFb0 = DaqAddClientMqttFb("127.0.0.1", 1883);
    auto jsonFb0 = AddSubFb(topic0);
    auto decoderFb0 = AddDecoderFb(valueF, tsF);

    auto clientFb1 = DaqAddClientMqttFb("127.0.0.1", 1884);
    auto jsonFb1 = AddSubFb(topic1);
    auto decoderFb1 = AddDecoderFb(valueF, tsF);

    const auto result0 = processTransfer("127.0.0.1", 1883, topic0, DATA_DOUBLE_INT_0, decoderFb0.getSignals()[0]);
    const auto result1 = processTransfer("127.0.0.1", 1884, topic1, DATA_DOUBLE_INT_1, decoderFb1.getSignals()[0]);

    ASSERT_FALSE(result0.mqttFbProblem);
    ASSERT_FALSE(result0.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), result0.dataReceived.size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_0, result0.dataReceived));
    ASSERT_EQ(decoderFb0.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb0.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);

    ASSERT_FALSE(result1.mqttFbProblem);
    ASSERT_FALSE(result1.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_1.size(), result1.dataReceived.size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_1, result1.dataReceived));
    ASSERT_EQ(decoderFb1.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(decoderFb1.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Parsing succeeded"), std::string::npos);
}

TEST_F(MqttJsonDecoderFbTest, RemovingNestedFunctionBlock)
{
    StartUp();
    AddSubFb(buildTopicName());
    daq::FunctionBlockPtr jsonDecoderFb;
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_DEC_VALUE_NAME, String("temp")));
        ASSERT_NO_THROW(jsonDecoderFb = subMqttFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
    }
    ASSERT_EQ(subMqttFb.getFunctionBlocks().getCount(), 1u);

    ASSERT_NO_THROW(subMqttFb.removeFunctionBlock(jsonDecoderFb));
    ASSERT_EQ(subMqttFb.getFunctionBlocks().getCount(), 0u);
    ASSERT_EQ(subMqttFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}
