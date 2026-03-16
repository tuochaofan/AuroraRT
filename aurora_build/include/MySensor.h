#ifndef MYSENSOR_H
#define MYSENSOR_H

#include <cstdint>
#include <string>
#include <vector>

struct MySensor {
    float temperature;
    float humidity;
    int32_t pressure;
    bool is_active;
    std::string sensor_id;
    std::vector<float> readings;
    std::vector<std::string> tags;
};

#endif // MYSENSOR_H
