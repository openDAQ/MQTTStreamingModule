#include "MqttAsyncClientWrapper.h"
#include "mqtt_streaming_helper/timer.h"
#include "mqtt_streaming_module/mqtt_publisher_fb_impl.h"
#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>
#include <iomanip>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

template <typename T>
class SignalHelper
{
public:
    ModulePtr module;

    SignalConfigPtr signal0;
    SignalConfigPtr signal1;
    SignalConfigPtr signalWithoutDomain;
    ContextPtr context;
    int cnt = 0;

    SignalHelper()
    {
        // Create domain signal
        auto logger = Logger();
        context = Context(Scheduler(logger), logger, TypeManager(), nullptr, nullptr);
        auto domainSignalDescriptorBuilder = DataDescriptorBuilder().setRule(LinearDataRule(1, 3)).setTickResolution(Ratio(1, 1000));

        // Create signal with descriptor
        signal0 = createSignal(domainSignalDescriptorBuilder);
        signal1 = createSignal(domainSignalDescriptorBuilder);
        signalWithoutDomain = createSignal(DataDescriptorBuilderPtr());
        // Create module
        createModule(&module, context);
    }

    SignalConfigPtr createSignal(const DataDescriptorBuilderPtr domainDescriptorBuilder)
    {
        SignalConfigPtr signal;
        DataDescriptorPtr domainSD;
        SignalConfigPtr dSignal;
        if (domainDescriptorBuilder.assigned())
        {
            domainSD = domainDescriptorBuilder.setUnit(Unit("s", -1, "seconds", "time"))
                           .setSampleType(SampleType::UInt64)
                           .setOrigin("1970-01-01T00:00:00")
                           .setReferenceDomainInfo(ReferenceDomainInfoBuilder().setReferenceDomainOffset(100).build())
                           .build();
            dSignal = SignalWithDescriptor(context, domainSD, nullptr, std::string("DomainSignal") + std::to_string(cnt));
        }

        auto SD = DataDescriptorBuilder().setSampleType(SampleTypeFromType<T>::SampleType).build();
        signal = SignalWithDescriptor(context, SD, nullptr, std::string("Signal") + std::to_string(cnt));
        if (dSignal.assigned())
            signal.setDomainSignal(dSignal);
        cnt++;
        return signal;
    }

    std::vector<std::pair<T, uint64_t>> generateTestData(size_t sampleCount) const
    {
        std::vector<std::pair<T, uint64_t>> data;
        for (size_t i = 0; i < sampleCount; i++)
        {
            uint64_t ts = DataPacket(signal0.getDomainSignal().getDescriptor(), 1, i).getLastValue();
            data.push_back({generateData(i), ts});
        }
        return data;
    }

    void send(const std::vector<std::pair<T, uint64_t>>& data) const
    {
        for (size_t i = 0; i < data.size(); i++)
        {
            auto sendPacket = [this](SignalConfigPtr signal, T data, DataPacketPtr domainPacket)
            {
                auto dataPacket = DataPacketWithDomain(domainPacket, signal0.getDescriptor(), 1);
                copyData(dataPacket, data);
                signal.sendPacket(dataPacket);
            };
            auto domainPacket = DataPacket(signal0.getDomainSignal().getDescriptor(), 1, i);
            memcpy(domainPacket.getData(), &(data[i].second), sizeof(uint64_t));
            SignalConfigPtr dSignal = signal0.getDomainSignal();
            dSignal.sendPacket(domainPacket);
            sendPacket(signal0, data[i].first, domainPacket);
            sendPacket(signal1, data[i].first, domainPacket);
        }
    }

protected:
    bool checkType(SampleType type) const
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
    }
    bool copyData(T& destination, const DataPacketPtr source)
    {
        const auto dataType = source.getDataDescriptor().getSampleType();
        if (checkType(dataType) && getSampleSize(dataType) != sizeof(destination))
            return false;
        if constexpr (std::is_same_v<T, std::string>)
        {
            return false;
        }
        else
        {
            memcpy(&destination, source.getData(), sizeof(destination));
        }
        return true;
    }

    bool copyData(DataPacketPtr destination, const T& source) const
    {
        const auto dataType = destination.getDataDescriptor().getSampleType();
        if (checkType(dataType) && getSampleSize(dataType) != sizeof(source))
            return false;
        if constexpr (std::is_same_v<T, std::string>)
        {
            return false;
        }
        else
        {
            memcpy(destination.getData(), &source, sizeof(source));
        }
        return true;
    }

    T generateData(size_t i) const
    {
        T sampleData;
        if constexpr (std::is_fundamental_v<T>)
        {
            if (i == 0)
            {
                sampleData = std::numeric_limits<T>::min();
            }
            else if (i == 1)
            {
                sampleData = std::numeric_limits<T>::max();
            }
            else if (i == 2)
            {
                sampleData = std::numeric_limits<T>::lowest();
            }
            else
            {
                if constexpr (std::is_integral_v<T>)
                {
                    sampleData = static_cast<T>(i);
                    if constexpr (std::is_signed_v<T>)
                    {
                        sampleData *= ((i % 2 == 0) ? 1 : -1);
                    }
                }
                else if constexpr (std::is_floating_point_v<T>)
                {
                    sampleData = static_cast<T>(i) * 1.1 * ((i % 2 == 0) ? 1 : -1);
                }
            }
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            sampleData = std::string("String data for a sample ") + std::to_string(i);
        }
        return sampleData;
    }
};

namespace daq::modules::mqtt_streaming_module
{
class MqttPublisherFbHelper : public DaqTestHelper
{
public:
    daq::FunctionBlockPtr fb;
    std::unique_ptr<MqttAsyncClientWrapper> subscriber;

    void CreatePublisherFB()
    {
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_PUB_QOS, 2);
        fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config);
    }

    void CreatePublisherFB(bool multiTopic,
                           bool sharedTs,
                           bool groupV,
                           bool useSignalNames,
                           const std::string& topicName,
                           size_t valuePackSize = 0,
                           int qos = 2,
                           uint32_t readPeriod = 20)
    {
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, multiTopic ? 1 : 0);
        config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, sharedTs ? True : False);
        config.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES, groupV ? True : False);
        config.setPropertyValue(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, useSignalNames ? True : False);
        config.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, valuePackSize);
        config.setPropertyValue(PROPERTY_NAME_PUB_QOS, qos);
        config.setPropertyValue(PROPERTY_NAME_PUB_READ_PERIOD, readPeriod);
        config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME, topicName);
        fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config);
    }

    bool CreateSubscriber()
    {
        subscriber = std::make_unique<MqttAsyncClientWrapper>(buildClientId("_subscriberId"));
        return subscriber->connect("127.0.0.1");
    }

    std::string buildClientId(const std::string& postfix = "")
    {
        auto str = std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + postfix;
        for (auto& ch : str)
        {
            if (ch == '/')
                ch = '_';
        }
        return str;
    }

    template <typename T>
    std::vector<std::string> expectedMsgsForSingle(const std::string& signalName, std::vector<std::pair<T, uint64_t>> data)
    {
        std::vector<std::string> msgs;
        const std::string PUBLISHER_SINGLE_MSG = "{<placeholder_signal> : <placeholder_value>, \"timestamp\": <placeholder_ts>}";

        std::vector<std::string> messages;
        {
            for (const auto& [value, ts] : data)
            {
                auto msg = PUBLISHER_SINGLE_MSG;
                msg = replacePlaceholder(msg, "<placeholder_signal>", signalName);
                msg = replacePlaceholder(msg, "<placeholder_value>", value);
                msg = replacePlaceholder(msg, "<placeholder_ts>", ts * 1000);
                messages.push_back(std::move(msg));
            }
        }
        return messages;
    }

    template <typename T>
    std::vector<std::string>
    expectedMsgsForSingleGroupValues(const std::string& signalName, std::vector<std::pair<T, uint64_t>> data, size_t packSize)
    {
        std::vector<std::string> msgs;
        const std::string PUBLISHER_SINGLE_MSG = "{<placeholder_signal> : <placeholder_value>, \"timestamp\": <placeholder_ts>}";

        std::vector<std::string> messages;
        {
            for (size_t i = 0; i < data.size(); i += packSize)
            {
                auto msg = PUBLISHER_SINGLE_MSG;
                msg = replacePlaceholder(msg, "<placeholder_signal>", signalName);
                std::vector<T> value;
                std::vector<uint64_t> ts;
                for (size_t j = i, cnt = 0; j < data.size() && cnt < packSize; ++j, ++cnt)
                {
                    value.push_back(data[j].first);
                    ts.push_back(data[j].second * 1000);
                }
                msg = replacePlaceholder(msg, "<placeholder_value>", value);
                msg = replacePlaceholder(msg, "<placeholder_ts>", ts);
                messages.push_back(std::move(msg));
            }
        }
        return messages;
    }

    template <typename T>
    std::vector<std::string>
    expectedMsgsForSharedTs(const std::string& signalName0, const std::string& signalName1, std::vector<std::pair<T, uint64_t>> data)
    {
        std::vector<std::string> msgs;
        const std::string PUBLISHER_SINGLE_MSG = "{<placeholder_signal0> : <placeholder_value0>, <placeholder_signal1> : "
                                                 "<placeholder_value1>, \"timestamp\" : <placeholder_ts>}";

        std::vector<std::string> messages;
        {
            for (const auto& [value, ts] : data)
            {
                auto msg = PUBLISHER_SINGLE_MSG;
                msg = replacePlaceholder(msg, "<placeholder_signal0>", signalName0);
                msg = replacePlaceholder(msg, "<placeholder_signal1>", signalName1);
                msg = replacePlaceholder(msg, "<placeholder_value0>", value);
                msg = replacePlaceholder(msg, "<placeholder_value1>", value);
                msg = replacePlaceholder(msg, "<placeholder_ts>", ts * 1000);
                messages.push_back(std::move(msg));
            }
        }
        return messages;
    }

    template <typename T>
    std::vector<std::string>
    expectedMsgsForMultimsg(const std::string& signalName0, const std::string& signalName1, std::vector<std::pair<T, uint64_t>> data)
    {
        std::vector<std::string> msgs;
        const std::string PUBLISHER_SINGLE_MSG = "[{<placeholder_signal0> : <placeholder_value0>, \"timestamp\": <placeholder_ts0>}, "
                                                 "{<placeholder_signal1> : <placeholder_value1>, \"timestamp\": <placeholder_ts1>}]";

        std::vector<std::string> messages;
        {
            for (const auto& [value, ts] : data)
            {
                auto msg = PUBLISHER_SINGLE_MSG;
                msg = replacePlaceholder(msg, "<placeholder_signal0>", signalName0);
                msg = replacePlaceholder(msg, "<placeholder_signal1>", signalName1);
                msg = replacePlaceholder(msg, "<placeholder_value0>", value);
                msg = replacePlaceholder(msg, "<placeholder_value1>", value);
                msg = replacePlaceholder(msg, "<placeholder_ts0>", ts * 1000);
                msg = replacePlaceholder(msg, "<placeholder_ts1>", ts * 1000);
                messages.push_back(std::move(msg));
            }
        }
        return messages;
    }

    template <typename vT>
    static std::string replacePlaceholder(const std::string& jsonTemplate, const std::string& ph, const vT& value)
    {
        std::string result = jsonTemplate;
        size_t pos = result.find(ph);
        if (pos != std::string::npos)
        {
            std::string replacement;
            if constexpr (std::is_same_v<vT, std::string>)
            {
                replacement = '"' + value + '"';
            }
            else if constexpr (std::is_same_v<vT, double> || std::is_same_v<vT, float>)
            {
                replacement = doubleToString(value, 6);
            }
            else
            {
                replacement = std::to_string(value);
            }
            result.replace(pos, ph.length(), replacement);
        }
        return result;
    }

    template <typename vT>
    static std::string replacePlaceholder(const std::string& jsonTemplate, const std::string& ph, const std::vector<vT>& values)
    {
        std::string result = jsonTemplate;
        size_t pos = result.find(ph);
        if (pos != std::string::npos)
        {
            std::string replacement("[");

            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    replacement += ", ";
                if constexpr (std::is_same_v<vT, std::string>)
                {
                    replacement += '"' + values[i] + '"';
                }
                else if constexpr (std::is_same_v<vT, double> || std::is_same_v<vT, float>)
                {
                    replacement += doubleToString(values[i], 6);
                }
                else
                {
                    replacement += std::to_string(values[i]);
                }
            }
            replacement += "]";

            result.replace(pos, ph.length(), replacement);
        }
        return result;
    }
    template <typename T>
    bool transfer(const std::string& topic,
                  const std::vector<std::string>& messages,
                  SignalHelper<T>& helper,
                  const std::vector<std::pair<T, uint64_t>>& data)
    {
        std::promise<bool> receivedPromise;
        auto receivedFuture = receivedPromise.get_future();
        std::atomic<bool> done{false};
        subscriber->expectMsgs(topic, messages, receivedPromise, done);

        helper::utils::Timer receiveTimer(5000);
        bool ok = subscriber->subscribe(topic, 2);
        if (!ok)
            return false;
        helper.send(data);
        auto status = receivedFuture.wait_for(receiveTimer.remain());
        subscriber = nullptr;
        ok = (status == std::future_status::ready) && receivedFuture.get();
        return ok;
    }

    std::string buildTopicName(const std::string& postfix = "")
    {
        return std::string("test/topic/publisherFB/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + postfix;
    }

private:
    static std::string doubleToString(double value, int precision = 12)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    }
};

class MqttPublisherFbTest : public testing::Test, public MqttPublisherFbHelper
{
};

using H = std::variant<SignalHelper<double>,
                       SignalHelper<float>,
                       SignalHelper<int64_t>,
                       SignalHelper<uint64_t>,
                       SignalHelper<int32_t>,
                       SignalHelper<uint32_t>,
                       SignalHelper<int16_t>,
                       SignalHelper<uint16_t>,
                       SignalHelper<int8_t>,
                       SignalHelper<uint8_t>>;

template <typename Helper>
struct HelperValueType
{
    using type = void;
};

template <typename T>
struct HelperValueType<SignalHelper<T>>
{
    using type = T;
};

// clang-format off
template<typename T> struct TypeName { static std::string Get() { return "unknown"; } };
template<> struct TypeName<float> { static std::string Get() { return "float"; } };
template<> struct TypeName<double> { static std::string Get() { return "double"; } };
template<> struct TypeName<int64_t> { static std::string Get() { return "int64_t"; } };
template<> struct TypeName<uint64_t> { static std::string Get() { return "uint64_t"; } };
template<> struct TypeName<int32_t> { static std::string Get() { return "int32_t"; } };
template<> struct TypeName<uint32_t> { static std::string Get() { return "uint32_t"; } };
template<> struct TypeName<int16_t> { static std::string Get() { return "int16_t"; } };
template<> struct TypeName<uint16_t> { static std::string Get() { return "uint16_t"; } };
template<> struct TypeName<int8_t> { static std::string Get() { return "int8_t"; } };
template<> struct TypeName<uint8_t> { static std::string Get() { return "uint8_t"; } };
template<> struct TypeName<std::string> { static std::string Get() { return "string"; } };
// clang-format on

std::string ParamNameGenerator(const testing::TestParamInfo<H>& info)
{
    return std::visit(
        [](auto& h)
        {
            using T = typename HelperValueType<std::decay_t<decltype(h)>>::type;
            std::string name = "Type_" + TypeName<T>::Get();
            return name;
        },
        info.param);
}
class MqttPublisherFbPTest : public ::testing::TestWithParam<H>, public MqttPublisherFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttPublisherFbTest, DefaultConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = rootMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(PUB_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 8u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_TOPIC_MODE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_MODE).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE).asPtr<IInteger>(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_SHARED_TS));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_SHARED_TS).asPtr<IBoolean>(), False);
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_GROUP_VALUES));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES).asPtr<IBoolean>(), False);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES).asPtr<IBoolean>(), False);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).asPtr<IInteger>(), DEFAULT_PUB_PACK_SIZE);
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_QOS));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_QOS).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_QOS).asPtr<IInteger>(), DEFAULT_PUB_QOS);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_READ_PERIOD));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_READ_PERIOD).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_READ_PERIOD).asPtr<IInteger>(), DEFAULT_PUB_READ_PERIOD);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_TOPIC_NAME));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_NAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME).asPtr<IString>().toStdString(), "");
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_NAME).getVisible());
}

TEST_F(MqttPublisherFbTest, PropertyVisibility)
{
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt = MqttPublisherFbImpl::CreateType();
    daq::PropertyObjectPtr defaultConfig = fbt.createDefaultConfig();

    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Multi topic
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 0); // Set to Single topic
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES, True);
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Multi topic
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Single topic
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES, True);
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 0); // Set to Single topic
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Multi topic
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 0); // Set to Single topic
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_NAME).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Multi topic
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_NAME).getVisible());
}

TEST_F(MqttPublisherFbTest, Config)
{
    StartUp();
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1);
    config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, 3);
    config.setPropertyValue(PROPERTY_NAME_PUB_QOS, 2);
    config.setPropertyValue(PROPERTY_NAME_PUB_READ_PERIOD, 100);
    config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME, buildTopicName());
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));
    const auto allProperties = fb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(fb.hasProperty(propName));
        ASSERT_EQ(fb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
    MqttPublisherFbImpl* ptr = reinterpret_cast<MqttPublisherFbImpl*>(fb.getObject());
    ASSERT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->getFbConfig().topicMode, TopicMode::Multi);
    EXPECT_TRUE(ptr->getFbConfig().sharedTs);
    EXPECT_TRUE(ptr->getFbConfig().groupValues);
    EXPECT_TRUE(ptr->getFbConfig().useSignalNames);
    EXPECT_EQ(ptr->getFbConfig().groupValuesPackSize, 3);
    EXPECT_EQ(ptr->getFbConfig().qos, 2);
    EXPECT_EQ(ptr->getFbConfig().periodMs, 100);
}

TEST_F(MqttPublisherFbTest, Creation)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, TwoFbCreation)
{
    StartUp();
    {
        daq::FunctionBlockPtr fb;
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME));
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                  EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                          static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                          daqInstance.getContext().getTypeManager()));
    }
    {
        daq::FunctionBlockPtr fb;
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME));
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                  EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                          static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                          daqInstance.getContext().getTypeManager()));
    }
    auto fbs = rootMqttFb.getFunctionBlocks();
    ASSERT_EQ(fbs.getCount(), 2u);
}

TEST_F(MqttPublisherFbTest, CreationWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME));
    auto signals = fb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, CreationWithPartialConfig)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    config.addProperty(BoolProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, True));
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, ConnectToPort)
{
    StartUp();
    StatusHelper<MqttPublisherFbImpl::SignalStatus>::addTypesToTypeManager(MQTT_PUB_FB_SIG_STATUS_TYPE,
                                                                           MQTT_PUB_FB_SIG_STATUS_NAME,
                                                                           MqttPublisherFbImpl::signalStatusMap,
                                                                           rootMqttFb.getContext().getTypeManager());
    StatusHelper<MqttPublisherFbImpl::PublishingStatus>::addTypesToTypeManager(MQTT_PUB_FB_PUB_STATUS_TYPE,
                                                                               MQTT_PUB_FB_PUB_STATUS_NAME,
                                                                               MqttPublisherFbImpl::publishingStatusMap,
                                                                               rootMqttFb.getContext().getTypeManager());
    const auto sigStValid = EnumerationWithIntValue(MQTT_PUB_FB_SIG_STATUS_TYPE,
                                                    static_cast<Int>(MqttPublisherFbImpl::SignalStatus::Valid),
                                                    daqInstance.getContext().getTypeManager());
    const auto sigStInvalid = EnumerationWithIntValue(MQTT_PUB_FB_SIG_STATUS_TYPE,
                                                      static_cast<Int>(MqttPublisherFbImpl::SignalStatus::Invalid),
                                                      daqInstance.getContext().getTypeManager());
    const auto sigStNotConnected = EnumerationWithIntValue(MQTT_PUB_FB_SIG_STATUS_TYPE,
                                                           static_cast<Int>(MqttPublisherFbImpl::SignalStatus::NotConnected),
                                                           daqInstance.getContext().getTypeManager());
    const auto comStOk = Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager());
    const auto comStError = Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager());

    {
        daq::FunctionBlockPtr fb;
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME));
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStNotConnected);
        auto help = SignalHelper<double>();

        ASSERT_EQ(fb.getInputPorts().getCount(), 1u);
        fb.getInputPorts()[0].connect(help.signal0);
        ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);

        fb.getInputPorts()[1].connect(help.signal0);
        ASSERT_EQ(fb.getInputPorts().getCount(), 3u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
        // disconnection
        fb.getInputPorts()[1].disconnect();
        ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
        // connection without a domain signal
        fb.getInputPorts()[1].connect(help.signalWithoutDomain);
        ASSERT_EQ(fb.getInputPorts().getCount(), 3u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
        // disconnection
        fb.getInputPorts()[1].disconnect();
        ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
        fb.getInputPorts()[0].disconnect();
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStNotConnected);
    }

    {
        daq::FunctionBlockPtr fb;
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
        auto help = SignalHelper<double>();

        auto signal0 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(2, 3)).setTickResolution(Ratio(1, 1000)));
        auto signal1 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(1, 3)).setTickResolution(Ratio(1, 500)));

        fb.getInputPorts()[0].connect(signal0);
        fb.getInputPorts()[1].connect(signal1);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
    }

    {
        daq::FunctionBlockPtr fb;
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
        auto help = SignalHelper<double>();

        auto signal0 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(1, 3)).setTickResolution(Ratio(1, 1000)));
        auto signal1 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(1, 3)).setTickResolution(Ratio(1, 500)));

        fb.getInputPorts()[0].connect(signal0);
        fb.getInputPorts()[1].connect(signal1);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStError);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStInvalid);
        fb.getInputPorts()[1].disconnect();
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
    }

    {
        daq::FunctionBlockPtr fb;
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
        auto help = SignalHelper<double>();

        auto signal0 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(2, 3)).setTickResolution(Ratio(1, 1000)));
        auto signal1 = help.createSignal(DataDescriptorBuilder().setRule(LinearDataRule(1, 3)).setTickResolution(Ratio(1, 1000)));

        fb.getInputPorts()[0].connect(signal0);
        fb.getInputPorts()[1].connect(signal1);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStError);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStInvalid);
        fb.getInputPorts()[1].disconnect();
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), comStOk);
        ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SIG_STATUS_NAME), sigStValid);
    }
}


TEST_F(MqttPublisherFbTest, WrongConfig)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME, String("/test/#"));
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(PUB_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Invalid),
                                      daqInstance.getContext().getTypeManager()));

    fb.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, False);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));

    fb.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
    fb.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME, String("/test/+/test"));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Invalid),
                                      daqInstance.getContext().getTypeManager()));

    fb.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_NAME, String("/test/value/test"));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
              EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                      static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                      daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttPublisherFbPTest, TransferSingle)
{
    const size_t sampleCnt = 15;
    H param = GetParam();
    std::visit(
        [&](auto& help)
        {
            StartUp();

            ASSERT_NO_THROW(CreatePublisherFB());

            fb.getInputPorts()[0].connect(help.signal0);
            ASSERT_TRUE(CreateSubscriber());

            const auto data = help.generateTestData(sampleCnt);
            const std::vector<std::string> messages = expectedMsgsForSingle(help.signal0.getGlobalId().toStdString(), data);
            const std::string topic = help.signal0.getGlobalId().toStdString();
            auto ok = transfer(topic, messages, help, data);
            ASSERT_TRUE(ok);
            ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                      Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                              daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_PUB_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_PUB_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::PublishingStatus::Ok),
                                              daqInstance.getContext().getTypeManager()));
        },
        param);
}

TEST_P(MqttPublisherFbPTest, TransferSingleGroupValues)
{
    constexpr size_t sampleCnt = 15;
    constexpr size_t packSize = 3;
    H param = GetParam();
    std::visit(
        [&](auto& help)
        {
            StartUp();

            ASSERT_NO_THROW(CreatePublisherFB(false, false, true, false, buildTopicName(), packSize));

            fb.getInputPorts()[0].connect(help.signal0);
            ASSERT_TRUE(CreateSubscriber());

            const auto data = help.generateTestData(sampleCnt);
            const std::vector<std::string> messages =
                expectedMsgsForSingleGroupValues(help.signal0.getGlobalId().toStdString(), data, packSize);
            const std::string topic = help.signal0.getGlobalId().toStdString();
            auto ok = transfer(topic, messages, help, data);
            ASSERT_TRUE(ok);
            ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                      Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                              daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_PUB_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_PUB_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::PublishingStatus::Ok),
                                              daqInstance.getContext().getTypeManager()));
        },
        param);
}

TEST_P(MqttPublisherFbPTest, TransferSharedTs)
{
    constexpr size_t sampleCnt = 15;
    H param = GetParam();
    std::visit(
        [&](auto& help)
        {
            StartUp();
            const std::string topic = buildTopicName();
            ASSERT_NO_THROW(CreatePublisherFB(true, true, false, false, topic));

            fb.getInputPorts()[0].connect(help.signal0);
            fb.getInputPorts()[1].connect(help.signal1);
            ASSERT_TRUE(CreateSubscriber());

            const auto data = help.generateTestData(sampleCnt);
            const std::vector<std::string> messages =
                expectedMsgsForSharedTs(help.signal0.getGlobalId().toStdString(), help.signal1.getGlobalId().toStdString(), data);
            auto ok = transfer(topic, messages, help, data);
            ASSERT_TRUE(ok);
            ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                      Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                              daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_PUB_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_PUB_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::PublishingStatus::Ok),
                                              daqInstance.getContext().getTypeManager()));
        },
        param);
}

TEST_P(MqttPublisherFbPTest, TransferMultimessage)
{
    constexpr size_t sampleCnt = 15;
    H param = GetParam();
    std::visit(
        [&](auto& help)
        {
            StartUp();
            const std::string topic = buildTopicName();
            ASSERT_NO_THROW(CreatePublisherFB(true, false, false, false, topic));

            fb.getInputPorts()[0].connect(help.signal0);
            fb.getInputPorts()[1].connect(help.signal1);
            ASSERT_TRUE(CreateSubscriber());

            const auto data = help.generateTestData(sampleCnt);
            const std::vector<std::string> messages =
                expectedMsgsForMultimsg(help.signal0.getGlobalId().toStdString(), help.signal1.getGlobalId().toStdString(), data);
            auto ok = transfer(topic, messages, help, data);
            ASSERT_TRUE(ok);
            ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                      Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_SET_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_SET_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::SettingStatus::Valid),
                                              daqInstance.getContext().getTypeManager()));
            ASSERT_EQ(fb.getStatusContainer().getStatus(MQTT_PUB_FB_PUB_STATUS_NAME),
                      EnumerationWithIntValue(MQTT_PUB_FB_PUB_STATUS_TYPE,
                                              static_cast<Int>(MqttPublisherFbImpl::PublishingStatus::Ok),
                                              daqInstance.getContext().getTypeManager()));
        },
        param);
}

INSTANTIATE_TEST_SUITE_P(MyParams,
                         MqttPublisherFbPTest,
                         ::testing::Values(H{SignalHelper<double>{}},
                                           H{SignalHelper<float>{}},
                                           H{SignalHelper<int64_t>{}},
                                           H{SignalHelper<uint64_t>{}},
                                           H{SignalHelper<int32_t>{}},
                                           H{SignalHelper<uint32_t>{}},
                                           H{SignalHelper<int16_t>{}},
                                           H{SignalHelper<uint16_t>{}},
                                           H{SignalHelper<int8_t>{}},
                                           H{SignalHelper<uint8_t>{}}),
                         ParamNameGenerator);

TEST_F(MqttPublisherFbTest, DISABLED_MultiReaderTest)
{
    constexpr auto samples = 15;
    constexpr size_t buffersSize = samples;
    constexpr auto signalNum = 2;
    constexpr auto timeoutS = 5;
    constexpr auto divider = 2;
    auto logger = Logger();
    auto context = Context(Scheduler(logger), logger, TypeManager(), nullptr, nullptr);
    auto domainSignalDescriptor0 = DataDescriptorBuilder()
                                       .setUnit(Unit("s", -1, "seconds", "time"))
                                       .setSampleType(SampleType::UInt64)
                                       .setRule(LinearDataRule(1, 4 / divider))
                                       .setOrigin("1970")
                                       .setTickResolution(Ratio(1, 1000 / divider))
                                       .setReferenceDomainInfo(ReferenceDomainInfoBuilder().setReferenceDomainOffset(100 / divider).build())
                                       .build();
    auto domainSignalDescriptor1 = DataDescriptorBuilder()
                                       .setUnit(Unit("s", -1, "seconds", "time"))
                                       .setSampleType(SampleType::UInt64)
                                       .setRule(LinearDataRule(1, 4))
                                       .setOrigin("1970")
                                       .setTickResolution(Ratio(1, 1000))
                                       .setReferenceDomainInfo(ReferenceDomainInfoBuilder().setReferenceDomainOffset(100).build())
                                       .build();
    auto domainSignal0 = SignalWithDescriptor(context, domainSignalDescriptor0, nullptr, "DomainSignal0");
    auto domainSignal1 = SignalWithDescriptor(context, domainSignalDescriptor1, nullptr, "DomainSignal1");

    auto signalDescriptor = DataDescriptorBuilder().setSampleType(SampleType::UInt64).build();
    auto signal0 = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal0");
    auto signal1 = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal1");

    signal0.setDomainSignal(domainSignal0);
    signal1.setDomainSignal(domainSignal1);

    auto readerBuilder = MultiReaderBuilder()
                             .setValueReadType(SampleType::UInt64)
                             .setInputPortNotificationMethod(PacketReadyNotification::SameThread)
                             .setDomainReadType(SampleType::UInt64);
    readerBuilder.addSignal(signal0);
    readerBuilder.addSignal(signal1);

    auto reader = readerBuilder.build();

    auto domainBuffers = std::vector<void*>(signalNum, nullptr);
    auto dataBuffers = std::vector<void*>(signalNum, nullptr);

    for (size_t i = 0; i < signalNum; ++i)
    {
        dataBuffers[i] = std::malloc(buffersSize * divider * getSampleSize(SampleType::UInt64));
        domainBuffers[i] = std::malloc(buffersSize * divider * getSampleSize(SampleType::UInt64));
    }

    auto sendPacket = [&signalDescriptor](SignalConfigPtr signal, uint64_t data, DataPacketPtr domainPacket)
    {
        auto dataPacket = DataPacketWithDomain(domainPacket, signalDescriptor, 1);
        memcpy(dataPacket.getData(), &data, sizeof(data));
        signal.sendPacket(dataPacket);
    };

    auto send = [&]()
    {
        std::cout << "---------------sending-data---------------" << std::endl;
        for (size_t i = 0; i < samples * divider; i++)
        {
            uint64_t ts0, ts1;
            if (i % divider == 0)
            {
                {
                    auto domainPacket = DataPacket(domainSignalDescriptor0, 1, i / divider);
                    domainSignal0.sendPacket(domainPacket);
                    sendPacket(signal0, (i / divider), domainPacket);
                    ts0 = domainPacket.getLastValue().getValue(0);
                }
                {
                    auto domainPacket = DataPacket(domainSignalDescriptor1, 1, i);
                    domainSignal1.sendPacket(domainPacket);
                    sendPacket(signal1, i, domainPacket);
                    ts1 = domainPacket.getLastValue().getValue(0);
                }
                std::cout << fmt::format("{{Signal0 | Signal1 | TS0 | TS1}} : {:^3} | {:^3} | {:^3} | {:^3}", i / divider, i, ts0, ts1)
                          << std::endl;
            }
            else
            {
                auto domainPacket = DataPacket(domainSignalDescriptor1, 1, i);
                domainSignal1.sendPacket(domainPacket);
                sendPacket(signal1, i, domainPacket);
                ts1 = domainPacket.getLastValue().getValue(0);
                std::cout << fmt::format("{{Signal0 | Signal1 | TS0 | TS1}} : {:^3} | {:^3} | {:^3} | {:^3}", "-", i, "-", ts1)
                          << std::endl;
            }
        }
        std::cout << "---------------sending-data-end---------------" << std::endl;
    };

    auto toString = [](const std::string& valueFieldName, void* data, SizeT offset) -> std::string
    { return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<uint64_t*>(data) + offset))); };

    SizeT sampleCnt = 0;
    auto finishTime = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutS);

    send();
    std::cout << "---------------reading-data---------------" << std::endl;
    while (sampleCnt < samples * divider && std::chrono::steady_clock::now() < finishTime)
    {
        auto dataAvailable = reader.getAvailableCount();
        auto count = std::min(SizeT{buffersSize}, dataAvailable);
        auto status = reader.readWithDomain(dataBuffers.data(), domainBuffers.data(), &count);
        if (status.getReadStatus() == ReadStatus::Ok && count > 0)
        {
            sampleCnt += count;
            for (SizeT cnt = 0; cnt < count; ++cnt)
            {
                std::vector<std::string> fields;
                for (size_t signalCnt = 0; signalCnt < signalNum; ++signalCnt)
                {
                    std::string valueFieldName = "Signal" + std::to_string(signalCnt);
                    fields.emplace_back(toString(valueFieldName, dataBuffers[signalCnt], cnt));
                }
                for (size_t signalCnt = 0; signalCnt < signalNum; ++signalCnt)
                {
                    std::string valueFieldName = "TS" + std::to_string(signalCnt);
                    fields.emplace_back(toString(valueFieldName, domainBuffers[signalCnt], cnt));
                }
                std::string msg;
                for (size_t i = 0; i < fields.size(); ++i)
                {
                    if (i > 0)
                        msg += ", ";
                    msg += fields[i];
                }
                std::cout << msg << std::endl;
            }
        }
    }
    std::cout << "---------------reading-data-end---------------" << std::endl;
    ASSERT_EQ(sampleCnt, samples * divider);
}
