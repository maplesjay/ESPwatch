#ifndef WEATHER_H
#define WEATHER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
// 配置常量（声明为 extern）
extern String weather_url;
extern String key;
extern String language;
extern String unit;

// 天气数据结构
struct WeatherData {
    int temperature;
    String weather_info;
    String city_name;
    bool success;
};
WeatherData getWeatherData(String queryCity);


#endif