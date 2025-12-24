#pragma once
#include <cstdint>
#include <string>
#include <vector>

inline const std::string VALID_JSON_1_TOPIC_0 = R"json({
    "openDAQ/RefDev0/IO/AI/RefCh0/Sig/AI0": [
        {
            "AI0": {
                "Value": "value",
                "Timestamp": "timestamp",
                "Unit": [
                    "V",
                    "volts",
                    "voltage"
                ]
            }
        },
        {
            "AI1": {
                "Value": "value1"
            }
        },
        {
            "AI2": {
                "Value": "value2",
                "Timestamp": "",
                "Unit": [
                    "W"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_1_TOPIC_1 = R"json({
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
                "Timestamp":"ts",
                "Unit":[
                    "%"
                ]
            }
        },
        {
            "tds":{
                "Value":"tds_value",
                "Timestamp":"ts",
                "Unit":[
                    "ppm", "parts per million", "Total dissolved solids"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_3_TOPIC_2 = R"json({
    "/mirip/UNet3AC2/sensor/data0":[
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
                "Timestamp":"ts",
                "Unit":[
                    "%"
                ]
            }
        },
        {
            "tds":{
                "Value":"tds_value",
                "Timestamp":"ts",
                "Unit":[
                    "ppm", "parts per million", "Total dissolved solids"
                ]
            }
        }
    ],
    "/mirip/UNet3AC2/sensor/data1":[
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
                "Timestamp":"ts",
                "Unit":[
                    "%"
                ]
            }
        }
    ],
    "/mirip/UNet3AC2/sensor/data2":[
        {
            "temp":{
                "Value":"temp",
                "Timestamp":"ts",
                "Unit":[
                    "°C"
                ]
            }
        }
    ]
}
)json";



// inline const std::string VALID_JSON_1_TOPIC_3 = R"json({
//     "/mirip/UNet3AC2/sensor/data":[
//         {
//             "temp":{
//                 "Value":"temp",
//                 "Timestamp":"ts",
//                 "Unit":[
//                     "°C"
//                 ]
//             }
//         }
//     ]
// }
// )json";

// inline const std::string VALID_JSON_1_TOPIC_4 = R"json({
//     "/mirip/UNet3AC2/+/data0":[
//     ]
// }
// )json";

// inline const std::string VALID_JSON_1_TOPIC_5 = R"json({
//     "/mirip/UNet3AC2/+/data0":[
//         {
//             "temp":{
//             }
//         }
//     ]
// }
// )json";

inline const std::string WILDCARD_JSON_0 = R"json({
    "/mirip/UNet3AC2/+/data0":[
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
                "Timestamp":"ts",
                "Unit":[
                    "%"
                ]
            }
        },
        {
            "tds":{
                "Value":"tds_value",
                "Timestamp":"ts",
                "Unit":[
                    "ppm", "parts per million", "Total dissolved solids"
                ]
            }
        }
    ]
}
)json";

inline const std::string WILDCARD_JSON_1 = R"json({
    "/mirip/#":[
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
                "Timestamp":"ts",
                "Unit":[
                    "%"
                ]
            }
        }
    ]
}
)json";

inline const std::string INVALID_JSON_1 = R"json({
    "/mirip/UNet3AC2/test/data0":[
        {
            "temp": 0
        }
    ]
}
)json";

inline const std::string INVALID_JSON_3 = R"json({
    "/mirip/UNet3AC2/test/data0": "invalid_value"
}
)json";

inline const std::string VALID_JSON_CONFIG_0 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"temp",
                "Timestamp":"ts",
                "Unit":[
                    "°C"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_DATA_0 = R"json({
    "temp": <placeholder_value>, "ts": <placeholder_ts>
}
)json";

inline const std::string VALID_JSON_CONFIG_1 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"temp",
                "Unit":[
                    "°C"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_DATA_1 = R"json({
    "temp": <placeholder_value>
}
)json";

inline const std::string VALID_JSON_CONFIG_2 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"temperature",
                "Timestamp":"timestamp",
                "Unit":[
                    "°C"
                ]
            }
        },
        {
            "humi":{
                "Value":"humi",
                "Timestamp":"timestamp"
            }
        },
        {
            "pressure":{
                "Value":"pr"
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_DATA_2 = R"json({
    "timestamp": <placeholder_ts>, "temperature": <placeholder_temperature>, "humi": <placeholder_humi>, "pr": <placeholder_pr>
}
)json";

inline const std::string MISSING_FIELD_JSON_DATA_2 = R"json({
    "timestamp": <placeholder_ts>, "temperature": <placeholder_temperature>, "pr": <placeholder_pr>
}
)json";

inline const std::string VALID_JSON_CONFIG_3 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"value",
                "Unit":[
                    "rpm"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_CONFIG_4 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"value",
                "Unit":[
                    "rpm",
                    "rotations per minute"
                ]
            }
        }
    ]
}
)json";

inline const std::string VALID_JSON_CONFIG_5 = R"json({
    <placeholder_topic>:[
        {
            "temperature":{
                "Value":"value",
                "Unit":[
                    "rpm",
                    "rotations per minute",
                    "rotational speed"
                ]
            }
        }
    ]
}
)json";

inline const std::vector<std::pair<double, uint64_t>> DATA_DOUBLE_INT_0 = {{23.50000001, 1761567115},
                                                                           {-0.00000005583, 1761567116},
                                                                           {19.84916651651, 1761567117},
                                                                           {-30.28078754, 1761567118},
                                                                           {50.11245348484001, 1761567119}};
inline const std::vector<std::pair<double, uint64_t>> DATA_DOUBLE_INT_1 = {{3.5, 1761567115000},
                                                                           {5.0, 1761567116000},
                                                                           {-9.8, 1761567117000},
                                                                           {0.2, 1761567118000},
                                                                           {0.1, 1761567119000}};
inline const std::vector<std::pair<double, uint64_t>> DATA_DOUBLE_INT_2 = {{223.5, 1761567115000000},
                                                                           {-345.0, 1761567116000000},
                                                                           {519.8, 1761567117000000},
                                                                           {380.2, 1761567118000000},
                                                                           {570.1, 1761567119000000}};

inline const std::vector<std::pair<int64_t, uint64_t>> DATA_INT_INT_0 = {{2307, 1761567115},
                                                                         {4500, 1761567116},
                                                                         {198, 1761567117},
                                                                         {380, 1761567118},
                                                                         {500, 1761567119}};
inline const std::vector<std::pair<int64_t, uint64_t>> DATA_INT_INT_1 = {{3, 1761567115000},
                                                                         {55358, 1761567116000},
                                                                         {9525, 1761567117000},
                                                                         {-454, 1761567118000},
                                                                         {454490, 1761567119000}};
inline const std::vector<std::pair<int64_t, uint64_t>> DATA_INT_INT_2 = {{-7223, 1761567115000000},
                                                                         {-3475, 1761567116000000},
                                                                         {5719, 1761567117000000},
                                                                         {380, 1761567118000000},
                                                                         {5, 1761567119000000}};
inline const std::vector<std::pair<std::string, uint64_t>> DATA_STR_INT_0 = {{"very cold", 1761567115},
                                                                             {"cold", 1761567116},
                                                                             {"normal", 1761567117},
                                                                             {"hot", 1761567118},
                                                                             {"very hot", 1761567119}};
inline const std::vector<std::pair<std::string, uint64_t>> DATA_STR_INT_1 = {{u8"очень холодно", 1761567115000},
                                                                             {u8"холодно", 1761567116000},
                                                                             {u8"нормально", 1761567117000},
                                                                             {u8"жарко", 1761567118000},
                                                                             {u8"очень жарко", 1761567119000}};
inline const std::vector<std::pair<double, std::string>> DATA_DOUBLE_STR_0 = {{23070.008, "2025-10-27T12:45:15Z"},
                                                                              {4500.4883, " 2025-10-27T12:45:16Z"},
                                                                              {198.0000052, "2025-10-27T12:45:17Z "},
                                                                              {380.3484, " 2025-10-27T12:45:18Z "},
                                                                              {500.0, "2025-10-27T12:45:19Z"}};
inline const std::vector<std::pair<double, std::string>> DATA_DOUBLE_STR_1 = {{3.454, "2025-10-27 12:45:15"},
                                                                              {55358.4, " 2025-10-27 12:45:16 "},
                                                                              {9525.5, " 2025-10-27 12:45:17 "},
                                                                              {-454.9, "2025-10-27 12:45:18 "},
                                                                              {454490.44, " 2025-10-27 12:45:19"}};
inline const std::vector<std::pair<double, std::string>> DATA_DOUBLE_STR_2 = {{-7223.45, " 2025-10-27 12:45:15.001"},
                                                                              {-3475.5, "2025-10-27 12:45:15.002 "},
                                                                              {5719.9, " 2025-10-27 12:45:15.003 "},
                                                                              {380.1, "  2025-10-27 12:45:15.004  "},
                                                                              {5, " 2025-10-27 12:45:15.005"},
                                                                              {50, " 2025-10-27T12:45:15.005Z"},
                                                                              {50.5, " 2025-10-27 12:45:15.005Z"},
                                                                              {51, "2025-10-27T12:45:15.005"}};
