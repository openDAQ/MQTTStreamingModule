#include "MqttAsyncClientWrapper.h"
#include "mqtt_streaming_client_module/mqtt_receiver_fb_impl.h"
#include "test_daq_test_helper.h"
#include "test_data.h"
#include "timestampConverter.h"
#include <cmath>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/reader_factory.h>
#include <sstream>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

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

namespace daq::modules::mqtt_streaming_client_module
{
class MqttJsonFbHelper
{
public:
    std::unique_ptr<MqttReceiverFbImpl> obj;

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
        obj = std::make_unique<MqttReceiverFbImpl>(NullContext(), nullptr, fbType, "localId", nullptr, config);
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
    std::vector<std::pair<double, uint64_t>>
    transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonConfigTemplate, const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, std::pair<double, uint64_t>>(data, jsonConfigTemplate, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    std::vector<double> transferDataWithoutDomain(const std::vector<std::pair<vT, tsT>>& data,
                                                  const std::string& jsonConfigTemplate,
                                                  const std::string& jsonDataTemplate)
    {
        return transferData<vT, tsT, double>(data, jsonConfigTemplate, jsonDataTemplate);
    }

    template <typename vT, typename tsT>
    bool compareData(const std::vector<std::pair<vT, tsT>>& data0, const std::vector<std::pair<double, uint64_t>>& data1, bool compareTs = true)
    {
        if (data0.size() != data1.size())
            return false;
        for (std::size_t i = 0; i < data0.size(); ++i)
        {
            const auto& [value0, ts0] = data0[i];
            const auto& [value1, ts1] = data1[i];
            if (!almostEqual(static_cast<double>(value0), static_cast<double>(value1)))
                return false;
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
    bool compareData(const std::vector<std::pair<vT, tsT>>& data0, const std::vector<double>& data1)
    {
        if (data0.size() != data1.size())
            return false;
        for (std::size_t i = 0; i < data0.size(); ++i)
        {
            const auto& [value0, ts0] = data0[i];
            const auto& value1 = data1[i];
            if (!almostEqual(static_cast<double>(value0), static_cast<double>(value1)))
                return false;
        }
        return true;
    }

    std::vector<std::pair<double, uint64_t>> read(const StreamReaderPtr& reader, bool withDomain = true)
    {
        std::vector<std::pair<double, uint64_t>> result;
        while (!reader.getEmpty())
        {
            daq::SizeT cnt = 1;
            std::pair<double, uint64_t> dataToReceiveEntry;
            if (withDomain)
                reader.readWithDomain(&dataToReceiveEntry.first, &dataToReceiveEntry.second, &cnt);
            else
                reader.read(&dataToReceiveEntry.first, &cnt);
            if (cnt == 1)
                result.push_back(dataToReceiveEntry);
        }
        return result;
    }

private:
    template <typename vT, typename tsT, typename returnT> std::vector<returnT> transferData(const std::vector<std::pair<vT, tsT>>& data, const std::string& jsonConfigTemplate, const std::string& jsonDataTemplate)
    {
        const auto topic = buildTopicName();
        const auto jsonConfig = replacePlaceholder(jsonConfigTemplate, "<placeholder_topic>", topic);
        CreateJsonFB(jsonConfig);

        auto signalList = getSignals();
        auto reader = daq::StreamReader(signalList[0]);

        auto msgs = replacePlaceholders(data, jsonDataTemplate);
        for (const auto& str : msgs)
        {
            onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
        }

        std::vector<returnT> dataToReceive;
        while (!reader.getEmpty())
        {
            daq::SizeT cnt = 1;
            returnT dataToReceiveEntry;
            if constexpr (std::is_same_v<returnT, double>)
            {
                reader.read(&dataToReceiveEntry, &cnt);
            }
            else if constexpr (std::is_same_v<returnT, std::pair<double, uint64_t>>)
            {
                reader.readWithDomain(&dataToReceiveEntry.first, &dataToReceiveEntry.second, &cnt);
            }
            if (cnt == 1)
                dataToReceive.push_back(dataToReceiveEntry);
        }
        return dataToReceive;
    }
};

class MqttJsonFbTest : public testing::Test, public DaqTestHelper, public MqttJsonFbHelper
{
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
class MqttJsonFbIntDataPTest : public ::testing::TestWithParam<std::vector<std::pair<int, uint64_t>>>,
                               public DaqTestHelper,
                               public MqttJsonFbHelper
{
};
class MqttJsonFbStringTsPTest : public ::testing::TestWithParam<std::vector<std::pair<double, std::string>>>,
                                public DaqTestHelper,
                                public MqttJsonFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_client_module

TEST_F(MqttJsonFbTest, DefaultConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
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
    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, VALID_JSON_0);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));

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
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(jsonFb.getName(), JSON_FB_NAME);
    auto fbs = device.getFunctionBlocks();
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
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME));
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
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttJsonFbTest, CreationWithCustomConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, String(VALID_JSON_0));
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttJsonFbRightJsonConfigPTest, CheckNumberOfSignal)
{
    auto [configStr, signalCnt] = GetParam();
    StartUp();

    auto configString = String(configStr);

    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));
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

    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));
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

    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, configString);
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = device.addFunctionBlock(JSON_FB_NAME, config));
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

    std::vector<std::pair<StreamReaderPtr, SignalPtr>> readers;
    const std::vector<std::string> originalNames{"temperature", "humi", "pressure"};
    std::vector<std::string> names;
    for (const auto& name : originalNames)
        names.emplace_back(MqttReceiverFbImpl::buildSignalNameFromTopic(topic, name));

    for (const auto& name : names)
    {
        for (const auto& signal : signalList)
        {
            if (signal.getName().toStdString() == name)
            {
                readers.emplace_back(std::pair<StreamReaderPtr, SignalPtr>(daq::StreamReader(signal), signal));
                break;
            }
        }
    }
    ASSERT_EQ(readers.size(), 3u);

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
        dataToReceive[i] = read(reader, signal.getDomainSignal().assigned());
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

    auto reader = daq::StreamReader(getSignals()[0]);

    for (int cnt = 0; cnt < DATA_DOUBLE_INT_0.size(); ++cnt)
    {
        auto str = VALID_JSON_DATA_1;
        str = replacePlaceholder(str, "<placeholder_value>", DATA_DOUBLE_INT_0[cnt].first);
        onSignalsMessage({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0});
    }
    std::vector<std::pair<double, uint64_t>> dataToReceive = read(reader, false);
    ASSERT_EQ(dataToReceive.size(), 0);
}

TEST_F(MqttJsonFbTest, DataTransferMissingFieldSeveralSignals)
{
    const auto topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_2, "<placeholder_topic>", topic);
    CreateJsonFB(jsonConfig);

    auto signalList = getSignals();
    ASSERT_EQ(signalList.getCount(), 3u);

    std::vector<std::pair<StreamReaderPtr, SignalPtr>> readers;
    const std::vector<std::string> originalNames{"temperature", "humi", "pressure"};
    std::vector<std::string> names;
    for (const auto& name : originalNames)
        names.emplace_back(MqttReceiverFbImpl::buildSignalNameFromTopic(topic, name));

    for (const auto& name : names)
    {
        for (const auto& signal : signalList)
        {
            if (signal.getName().toStdString() == name)
            {
                readers.emplace_back(std::pair<StreamReaderPtr, SignalPtr>(daq::StreamReader(signal), signal));
                break;
            }
        }
    }
    ASSERT_EQ(readers.size(), 3u);

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
        dataToReceive[i] = read(reader, signal.getDomainSignal().assigned());
    }
    EXPECT_EQ(DATA_DOUBLE_INT_0.size(), dataToReceive[0].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_0, dataToReceive[0]));

    EXPECT_EQ(0u, dataToReceive[1].size());

    EXPECT_EQ(DATA_DOUBLE_INT_2.size(), dataToReceive[2].size());
    EXPECT_TRUE(compareData(DATA_DOUBLE_INT_2, dataToReceive[2], false));
}

TEST_F(MqttJsonFbTest, FullDataTransfer)
{
    const std::string topic = buildTopicName();
    const auto jsonConfig = replacePlaceholder(VALID_JSON_CONFIG_0, "<placeholder_topic>", topic);

    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1", DaqMqttDeviceConfig(100));

    auto config = device.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, jsonConfig);
    auto singal = device.addFunctionBlock(JSON_FB_NAME, config).getSignals()[0];

    auto reader = daq::StreamReader(singal);

    MqttAsyncClientWrapper publisher(std::make_shared<mqtt::MqttAsyncClient>(), buildClientId());
    ASSERT_TRUE(publisher.connect("127.0.0.1"));

    for (const auto& [value, ts] : DATA_DOUBLE_INT_0)
    {
        auto str = VALID_JSON_DATA_0;
        str = replacePlaceholder(str, "<placeholder_ts>", ts);
        str = replacePlaceholder(str, "<placeholder_value>", value);
        ASSERT_TRUE(publisher.publishMsg({topic, std::vector<uint8_t>(str.begin(), str.end()), 1, 0}));
    }

    const std::vector<std::pair<double, uint64_t>> dataToReceive = read(reader, true);

    ASSERT_EQ(DATA_DOUBLE_INT_0.size(), dataToReceive.size());
    ASSERT_TRUE(compareData(DATA_DOUBLE_INT_0, dataToReceive));
}
