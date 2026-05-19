#include "weather.h"

// API 配置
String weather_url = "https://api.seniverse.com/v3/weather/now.json";
String key = "S_kaflFYkOe4yXrvo";
String language = "zh-Hans";
String unit = "c";


WeatherData getWeatherData(String queryCity) {
    WeatherData data;
    data.success = false;
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi 未连接");
        return data;
    }
    
    HTTPClient http;
    String requestUrl = weather_url + "?key=" + key + "&location=" + queryCity 
                      + "&language=" + language + "&unit=" + unit;
    http.begin(requestUrl);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String response = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            data.temperature = doc["results"][0]["now"]["temperature"].as<int>();
            data.weather_info = doc["results"][0]["now"]["text"].as<String>();
            data.city_name = doc["results"][0]["location"]["name"].as<String>();
            data.success = true;
            
            Serial.printf("温度: %d°C, 天气: %s\n", data.temperature, data.weather_info.c_str());
        }
    }
    
    http.end();
    return data;
}
