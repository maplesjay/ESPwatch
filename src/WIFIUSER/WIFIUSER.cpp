#include "WIFIUSER.h"
#include <Arduino.h>
#include <WiFi.h>

// 内部状态变量
//标志位
bool isScanning = false;
static bool isConnecting = false;

static unsigned long connectStartTime = 0;
const unsigned long TIMEOUT_MS = 10000;

// 回调函数指针
static WifiScanCallback scanCb = nullptr;
static WifiConnectCallback connectCb = nullptr;
void WIFIUSER_SetScanCallback(WifiScanCallback cb) { scanCb = cb; }
void WIFIUSER_SetConnectCallback(WifiConnectCallback cb) { connectCb = cb; }

void WIFIUSER_Init() {
    // 初始状态下不开启WiFi
    WiFi.mode(WIFI_OFF);
}

void WIFIUSER_TurnOn() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

void WIFIUSER_TurnOff() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    isScanning = false;
}

void WIFIUSER_StartScan() {
    if (WiFi.getMode() == WIFI_STA) {
        WiFi.scanNetworks(true); // 异步扫描
        isScanning = true;
    }
}

void WIFIUSER_StartConnect(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(10);
    WiFi.begin(ssid, password);
    isConnecting = true;
    connectStartTime = millis();
}

void WIFIUSER_CancelConnect() {
    isConnecting = false;
    WiFi.disconnect();
}

// 核心状态机，处理异步任务
void WIFIUSER_Loop() {
    // 处理扫描
    if (isScanning) {
        int n = WiFi.scanComplete();
        if (n >= 0 || n == WIFI_SCAN_FAILED) {
            isScanning = false;
            if (scanCb) scanCb(n); // 触发回调通知UI
            if (n >= 0) WiFi.scanDelete();
        }
    }


    // 处理连接
    if (isConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            isConnecting = false;
            if (connectCb) connectCb(true); // 连接成功回调
        } else if (millis() - connectStartTime > TIMEOUT_MS) {
            isConnecting = false;
            WiFi.disconnect();
            if (connectCb) connectCb(false); // 连接失败回调
        }
    }
}