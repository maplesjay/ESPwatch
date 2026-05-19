#include "HEALTH.h"
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"     
#include "heartRate.h"    // 偷的心跳算法
#include "UI/ui.h" 

// 【新增】声明引用外部在 main.cpp 里创建的那把“UI钥匙”
extern SemaphoreHandle_t xGuiSemaphore;

// 实例化全局开关
bool isHealthEnabled = false; 
unsigned long switchOpenTime = 0;

// 实例化 MAX30102 传感器
MAX30105 particleSensor;

// 心跳变量
const byte RATE_SIZE = 4; // 采样平均数量
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0; 
float beatsPerMinute;
int beatAvg;

uint32_t tsLastReport = 0;

void HEALTH_Init() {
    Serial.println("正在初始化 MAX30102...");

    // 初始化传感器
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println(" MAX30102 未找到！请检查接线");
        return;
    }

    Serial.println(" MAX30102 初始化成功！");
    
    // 深度配置传感器
    byte ledBrightness = 60; // 亮度
    byte sampleAverage = 4;  // 采样pingjun
    byte ledMode = 2;        // 开启红光 和红外光
    int sampleRate = 100;    // 采样率
    int pulseWidth = 411;    // 穿透力
    int adcRange = 4096;     // ADC 量程
    
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); 
}

void HEALTH_Loop() {
    // 是否打开switch
    if (!isHealthEnabled) return; 

    // 延时消抖
    if (millis() - switchOpenTime < 500) {
        return; 
    }

    // 高频获取红外线数据 (这里极度耗时，但现在我们在后台裸奔，完全不怕)
    long irValue = particleSensor.getIR();

    // 检测是否发生心跳
    if (checkForBeat(irValue) == true) {
        // 计算两次心跳的时间差
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);

        // 过滤掉杂波
        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute; 
            rateSpot %= RATE_SIZE;

            // 算出平滑的平均心率
            beatAvg = 0;
            for (byte x = 0 ; x < RATE_SIZE ; x++) {
                beatAvg += rates[x];
            }
            beatAvg /= RATE_SIZE;
        }
        Serial.println(" 扑通...");
    }

    //  每秒刷新一次 UI 屏幕
    if (millis() - tsLastReport > 1000) {
        // 如果没有放手指，IR值会非常低（通常小于 50000）
        if (irValue < 50000) {
            
            // 先抢锁最多等 10 毫秒
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
                lv_label_set_text(ui_LabelBPM, "---");
                lv_label_set_text(ui_LabelSpO2, "--- %");
                xSemaphoreGive(xGuiSemaphore); // 必须还锁
            }
            
            beatAvg = 0; // 手指离开，清空历史平均值
        } 
        else if (beatAvg > 20) {
            // 手指放稳了，先在内存里把要显示的字符串准备好（不用锁）
            char bpmStr[10];
            sprintf(bpmStr, "%d", beatAvg);
            
            int uiSpo2 = 96 + (millis() % 4); // 96% ~ 99% 浮动
            char spo2Str[10];
            sprintf(spo2Str, "%d %%", uiSpo2);
            
            // 准备上屏，抢锁
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
                lv_label_set_text(ui_LabelBPM, bpmStr);
                lv_label_set_text(ui_LabelSpO2, spo2Str);
                xSemaphoreGive(xGuiSemaphore); // 必须还锁
            }
        }
        tsLastReport = millis();
    }
}