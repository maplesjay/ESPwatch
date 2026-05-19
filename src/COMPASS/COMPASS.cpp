#include "COMPASS.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include "UI/ui.h"

extern SemaphoreHandle_t xGuiSemaphore;

// ID
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

// 滤波
float smoothHeading = 0.0;
const float filterAlpha = 0.15; // 滤波强度

void COMPASS_Init() {
    Serial.println(" 正在初始化 HMC5883L 指南针...");

    // 检查开i2c
    if (!mag.begin()) {
        Serial.println(" 找不到");
        return;
    }
    Serial.println(" 指南针初始化成功");
}

void COMPASS_Loop() {
static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 40) return;
    lastUpdate = millis();

    sensors_event_t event;
    mag.getEvent(&event);

    // 干扰校准
    float offsetX = 3; 
    float offsetY = -13.5; 
    
    float correctedX = event.magnetic.x - offsetX;
    float correctedY = event.magnetic.y - offsetY;

    // 重新计算带偏移补偿弧度
    float heading = atan2(correctedY, correctedX);

    heading += M_PI;

    // 修正弧度
    if (heading < 0) {
        heading += 2 * M_PI;
    } else if (heading > 2 * M_PI) {
        heading -= 2 * M_PI;
    }

    // 将弧度转换 0~360 度
    float headingDegrees = heading * 180 / M_PI;

    // 平滑滤波  LVGL旋转
    float diff = headingDegrees - smoothHeading;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    smoothHeading += diff * filterAlpha;
    
    if (smoothHeading >= 360) smoothHeading -= 360;
    if (smoothHeading < 0) smoothHeading += 360;

    if (ui_Image10) {
        // 最多等 10 毫秒，等不到就算了，大不了错过这一帧（40ms 后还会再算）
        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_img_set_angle(ui_Image10, (int32_t)(smoothHeading * 10));
            xSemaphoreGive(xGuiSemaphore); // 必须还锁！
        }
    }
    //查看原始数据
    //Serial.printf("X: %.2f, Y: %.2f\n", event.magnetic.x, event.magnetic.y);
}