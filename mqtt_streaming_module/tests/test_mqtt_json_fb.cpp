#include "MqttAsyncClientWrapper.h"
#include "Timer.h"
#include "mqtt_streaming_module/mqtt_json_receiver_fb_impl.h"
#include "test_daq_test_helper.h"
#include "test_data.h"
#include "timestampConverter.h"
#include <cmath>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

template <typename T>
struct is_pair : std::false_type {};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type {};

bool almostEqual(double a, double b, double relEpsilon = 1e-9, double absEpsilon = 1e-12)
{
    return std::fabs(a - b) <= std::max(absEpsilon, relEpsilon * std::max(std::fabs(a), std::fabs(b)));
}

std::string doubleToString(double value, int precision = 12)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

namespace daq::modules::mqtt_streaming_module
{
class MqttJsonFbHelper
{
public:
    std::unique_ptr<MqttJsonReceiverFbImpl> obj;

    void onSignalsMessage(const mqtt::MqttMessage& msg)
    {
        mqtt::MqttAsyncClient unused;
        obj->onSignalsMessage(unused, msg);
    }

    void CreateJsonFB(const std::string& jsonConfig)
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_SIGNAL_LIST, String("")));
        const auto fbType = FunctionBlockType(JSON_FB_NAME, JSON_FB_NAME, "", config);
        config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, jsonConfig);
        obj = std::make_unique<MqttJsonReceiverFbImpl>(NullContext(), nullptr, fbType, "localId", nullptr, config);
    }

    auto getSignals()
    {
        auto signalList = List<ISignal>();
        obj->getSignals(&signalList);
        return signalList;
    }

    std::string buildTopicName()
    {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    std::string buildClientId()
    {
        return std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "_ClientId";
    }

    template <typename T> std::string replacePlaceholder(const std::string& jsonTemplate, const std::string& ph, const T& value)
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
            else
            {
                replacement = std::to_string(value);
            }
            result.replace(pos, ph.length(), replacement);
        }
        return result;
    }

    template <typename vT, typename tsT> std::vector<std::string> replacePlaceholders(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonTemplate)
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

    template <typename vT, typename tsT>
    std::vector<std::pair<vT, uint64_t>>
    transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonConfigTemplate, const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, std::pair<vT, uint64_t>>(data, jsonConfigTemplate, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    std::vector<vT> transferDataWithoutDomain(const std::vector<std::pair<vT, tsT>>& data,
                                                  const std::string& jsonConfigTemplate,
                                                  const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, vT>(data, jsonConfigTemplate, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    bool compareData(const std::vector<std::pair<vT, tsT>>& data0, const std::vector<std::pair<vT, uint64_t>>& data1, bool compareTs = true)
    {
        if (data0.size() != data1.size())
            return false;
        for (std::size_t i = 0; i < data0.size(); ++i)
        {
            const auto& [value0, ts0] = data0[i];
            const auto& [value1, ts1] = data1[i];
            if constexpr (std::is_same_v<vT, double>)
            {
                if (!almostEqual(static_cast<double>(value0), static_cast<double>(value1)))
                    return false;
            }
            else
            {
                if (value0 != value1)
                    return false;
            }
            if (compareTs)
            {
                if constexpr (std::is_same_v<tsT, uint64_t>)
                {
                    if (mqtt::utils::numericToMicroseconds(ts0) != ts1 && ts1 != 0)
                        return false;
                }
                else if constexpr (std::is_same_v<tsT, std::string>)
                {
                    if (mqtt::utils::toUnixTicks(ts0) != ts1 && ts1 != 0)
                        return false;
                }
                else
                {
                    return false;
                }
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
            const auto& [value0, ts0] = data0[i];
            const auto& value1 = data1[i];
            if constexpr (std::is_same_v<vT, double>)
            {
                if (!almostEqual(static_cast<double>(value0), static_cast<double>(value1)))
                    return false;
            }
            else
            {
                if (value0 != value1)
                    return false;
            }
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
        if (checkType(dataType) && getSampleSize(dataType) != sizeof(destination))
            return false;
        if constexpr (std::is_same_v<T, std::string>)
        {
            destination = std::string(static_cast<char*>(source.getData()), source.getDataSize());
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

        auto timer = Timer(timeoutMs);
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
                const auto dataType = dataPacket.getDataDescriptor().getSampleType();
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

private:
    template <typename vT, typename tsT, typename returnT> std::vector<returnT> transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonConfigTemplate, const std::string& jsonDataTemplate)
    {
        const auto topic = buildTopicName();
        const auto jsonConfig = replacePlaceholder(jsonConfigTemplate, "<placeholder_topic>", topic);
        CreateJsonFB(jsonConfig);

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

class MqttJsonFbTest : public testing::Test, public DaqTestHelper, public MqttJsonFbHelper
{
};

class MqttJsonFbCommunicationTest : public testing::Test, public DaqTestHelper, public MqttJsonFbHelper
{
public:
    using data_set_t = std::vector<std::pair<double, uint64_t>>;

    struct Result
    {
        bool mqttFbProblem = false;
        bool publishingProblem = false;
        data_set_t dataReceived;
    };

    Result processTransfer(const InstancePtr& instance, const std::string& url, const uint16_t port, const std::string& topic_postfix, const data_set_t& dataSet)
    {
        Result result;
        const std::string topic = buildTopicName() + topic_postfix;
        const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_0, "<placeholder_topic>", topic);
        FunctionBlockPtr rootMqttFb;
        try
        {
            rootMqttFb = instance.addFunctionBlock(ROOT_FB_NAME, DaqMqttFbConfig(url, port));
        }
        catch (...)
        {
            result.mqttFbProblem = true;
            return result;
        }

        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, jsonConfig);
        auto singal = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config).getSignals()[0];
        auto reader = daq::PacketReader(singal);

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
        result.dataReceived = read<std::pair<double, uint64_t>>(reader, singal, 2000);
        return result;
    };
};

class MqttJsonFbRightJsonConfigPTest : public ::testing::TestWithParam<std::pair<std::string, int>>,
                                       public DaqTestHelper,
                                       public MqttJsonFbHelper
{
};
class MqttJsonFbWrongJsonConfigPTest : public ::testing::TestWithParam<std::string>, public DaqTestHelper, public MqttJsonFbHelper
{
};
class MqttJsonFbDoubleDataPTest : public ::testing::TestWithParam<std::vector<std::pair<double, uint64_t>>>,
                                  public DaqTestHelper,
                                  public MqttJsonFbHelper
{
};
class MqttJsonFbIntDataPTest : public ::testing::TestWithParam<std::vector<std::pair<int64_t, uint64_t>>>,
                               public DaqTestHelper,
                               public MqttJsonFbHelper
{
};
class MqttJsonFbStringDataPTest : public ::testing::TestWithParam<std::vector<std::pair<std::string, uint64_t>>>,
                                  public DaqTestHelper,
                                  public MqttJsonFbHelper
{
};
class MqttJsonFbStringTsPTest : public ::testing::TestWithParam<std::vector<std::pair<double, std::string>>>,
                                public DaqTestHelper,
                                public MqttJsonFbHelper
{
};
class MqttJsonFbUnitPTest : public ::testing::TestWithParam<std::pair<std::string, std::vector<std::string>>>,
                                public DaqTestHelper,
                                public MqttJsonFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttJsonFbTest, DefaultConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = rootMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(JSON_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 1u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SIGNAL_LIST));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SIGNAL_LIST).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtr<IString>().getLength(), 0u);
}

TEST_F(MqttJsonFbTest, Config)
{
    StartUp();
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, VALID_JSON_0);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));

    const auto allProperties = jsonFb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(jsonFb.hasProperty(propName));
        ASSERT_EQ(jsonFb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttJsonFbTest, Creation)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(jsonFb.getName(), JSON_FB_NAME);
    auto fbs = rootMqttFb.getFunctionBlocks();
    bool contain = false;
    daq::GenericFunctionBlockPtr<daq::IFunctionBlock> fbFromList;
    for (const auto& fb : fbs)
    {
        contain = (fb.getName() == JSON_FB_NAME);
        if (contain)
        {
            fbFromList = fb;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(fbFromList.assigned());
    ASSERT_EQ(fbFromList.getName(), jsonFb.getName());
    ASSERT_TRUE(fbFromList == jsonFb);
}

TEST_F(MqttJsonFbTest, CreationWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME));
    auto signals = jsonFb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttJsonFbTest, CreationWithPartialConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = PropertyObject();
    config.addProperty(StringProperty(PROPERTY_NAME_SIGNAL_LIST, String(VALID_JSON_0)));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttJsonFbTest, CreationWithCustomConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, String(VALID_JSON_0));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttJsonFbRightJsonConfigPTest, CheckNumberOfSignal)
{
    auto [configStr, signalCnt] = GetParam();
    StartUp();

    auto configString = String(configStr);

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    auto signals = jsonFb.getSignals();
    ASSERT_EQ(signals.getCount(), signalCnt);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

INSTANTIATE_TEST_SUITE_P(SignalNumbersTest,
                         MqttJsonFbRightJsonConfigPTest,
                         ::testing::Values(std::make_pair(VALID_JSON_0, 4),
                                           std::make_pair(VALID_JSON_1, 3),
                                           std::make_pair(VALID_JSON_2, 6)));

TEST_F(MqttJsonFbTest, SignalListWithWildcard)
{
    StartUp();

    auto configString = String(WILDCARD_JSON_0);

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    auto signals = jsonFb.getSignals();
    ASSERT_EQ(signals.getCount(), 1u);
    // TODO : check status to Warning when wildcard is used
    // ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
    //           Enumeration("ComponentStatusType", "Warning", daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttJsonFbWrongJsonConfigPTest, WrongJsonConfig)
{
    auto configStr = GetParam();
    StartUp();

    auto configString = String(configStr);

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    auto signals = jsonFb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
    // TODO : check status to Error when config is invalid
    // ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
    //           Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
}

INSTANTIATE_TEST_SUITE_P(WrongJsonConfigTest,
                         MqttJsonFbWrongJsonConfigPTest,
                         ::testing::Values(INVALID_JSON_0, INVALID_JSON_1, INVALID_JSON_2, INVALID_JSON_3));

TEST_P(MqttJsonFbDoubleDataPTest, DataTransferOneSignalDouble)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_CONFIG_0, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

TEST_P(MqttJsonFbDoubleDataPTest, DataTransferOneSignalDoubleWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_CONFIG_1, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalDouble,
                         MqttJsonFbDoubleDataPTest,
                         ::testing::Values(DATA_DOUBLE_INT_0, DATA_DOUBLE_INT_1, DATA_DOUBLE_INT_2));

TEST_P(MqttJsonFbIntDataPTest, DataTransferOneSignalInt)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_CONFIG_0, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

TEST_P(MqttJsonFbIntDataPTest, DataTransferOneSignalIntWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_CONFIG_1, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalInt,
                         MqttJsonFbIntDataPTest,
                         ::testing::Values(DATA_INT_INT_0, DATA_INT_INT_1, DATA_INT_INT_2));

TEST_P(MqttJsonFbStringDataPTest, DataTransferOneSignalString)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_CONFIG_0, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

TEST_P(MqttJsonFbStringDataPTest, DataTransferOneSignalStringWithoutDomain)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferDataWithoutDomain(dataToSend, VALID_JSON_CONFIG_1, VALID_JSON_DATA_1);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalString,
                         MqttJsonFbStringDataPTest,
                         ::testing::Values(DATA_STR_INT_0, DATA_STR_INT_1));

TEST_P(MqttJsonFbStringTsPTest, DataTransferOneSignalIntDomainString)
{
    const auto dataToSend = GetParam();
    auto dataToReceive = transferData(dataToSend, VALID_JSON_CONFIG_0, VALID_JSON_DATA_0);
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(dataToSend, dataToReceive));
}

INSTANTIATE_TEST_SUITE_P(DataTransferOneSignalInt,
                         MqttJsonFbStringTsPTest,
                         ::testing::Values(DATA_DOUBLE_STR_0, DATA_DOUBLE_STR_1, DATA_DOUBLE_STR_2));

TEST_F(MqttJsonFbTest, DataTransferSeveralSignals)
{
    const auto topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_2, "<placeholder_topic>", topic);
    CreateJsonFB(jsonConfig);

    auto signalList = getSignals();
    ASSERT_EQ(signalList.getCount(), 3u);

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

    EXPECT_EQ(DATA_DOUBLE_INT_1.size(), dataToReceive[1].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_1, dataToReceive[1]));

    EXPECT_EQ(DATA_DOUBLE_INT_2.size(), dataToReceive[2].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_2, dataToReceive[2], false));
}

TEST_F(MqttJsonFbTest, DataTransferMissingFieldOneSignal)
{
    const auto topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_0, "<placeholder_topic>", topic);
    CreateJsonFB(jsonConfig);

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
}

TEST_F(MqttJsonFbTest, DataTransferMissingFieldSeveralSignals)
{
    const auto topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_2, "<placeholder_topic>", topic);
    CreateJsonFB(jsonConfig);

    auto signalList = getSignals();
    ASSERT_EQ(signalList.getCount(), 3u);

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

    EXPECT_EQ(0u, dataToReceive[1].size());

    EXPECT_EQ(DATA_DOUBLE_INT_2.size(), dataToReceive[2].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_2, dataToReceive[2], false));
}

TEST_F(MqttJsonFbCommunicationTest, FullDataTransfer)
{
    const auto instance = Instance();
    const auto result = processTransfer(instance, "127.0.0.1", DEFAULT_PORT, "", DATA_DOUBLE_INT_0);

    ASSERT_FALSE(result.mqttFbProblem);
    ASSERT_FALSE(result.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), result.dataReceived.size());
    ASSERT_TRUE(compareData(DATA_DOUBLE_INT_0, result.dataReceived));
}

TEST_F(MqttJsonFbCommunicationTest, FullDataTransferFor2MqttFbs)
{
    const auto instance = Instance();
    const auto result0 = processTransfer(instance, "127.0.0.1", 1883, "0", DATA_DOUBLE_INT_0);
    const auto result1 = processTransfer(instance, "127.0.0.1", 1884, "1", DATA_DOUBLE_INT_1);

    ASSERT_FALSE(result0.mqttFbProblem);
    ASSERT_FALSE(result0.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), result0.dataReceived.size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_0, result0.dataReceived));

    ASSERT_FALSE(result1.mqttFbProblem);
    ASSERT_FALSE(result1.publishingProblem);
    EXPECT_EQ(DATA_DOUBLE_INT_1.size(), result1.dataReceived.size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_1, result1.dataReceived));
}

TEST_P(MqttJsonFbUnitPTest, SignalUnit)
{
    const auto [config, unitDetails] = GetParam();
    ASSERT_EQ(unitDetails.size(), 3u);
    const auto topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(config, "<placeholder_topic>", topic);
    CreateJsonFB(jsonConfig);

    auto signalList = getSignals();
    ASSERT_EQ(signalList.getCount(), 1u);
    ASSERT_TRUE(signalList[0].getDescriptor().assigned());
    ASSERT_TRUE(signalList[0].getDescriptor().getUnit().assigned());
    EXPECT_EQ(signalList[0].getDescriptor().getUnit().getSymbol(), unitDetails[0]);
    EXPECT_EQ(signalList[0].getDescriptor().getUnit().getName(), unitDetails[1]);
    EXPECT_EQ(signalList[0].getDescriptor().getUnit().getQuantity(), unitDetails[2]);
}

INSTANTIATE_TEST_SUITE_P(SignalUnit,
                         MqttJsonFbUnitPTest,
                         ::testing::Values(std::pair<std::string, std::vector<std::string>>{VALID_JSON_CONFIG_3, {"rpm", "", ""}},
                                           std::pair<std::string, std::vector<std::string>>{VALID_JSON_CONFIG_4, {"rpm", "rotations per minute", ""}},
                                           std::pair<std::string, std::vector<std::string>>{VALID_JSON_CONFIG_5, {"rpm", "rotations per minute", "rotational speed"}}));
