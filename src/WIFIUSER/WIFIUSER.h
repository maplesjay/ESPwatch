#ifndef WIFIUSER_H
#define WIFIUSER_H

// 提供给外部（UI事件和main）调用的接口
void WIFIUSER_Init();
void WIFIUSER_Loop();  // 放在 main.cpp 的 loop 中运行

void WIFIUSER_TurnOn();
void WIFIUSER_TurnOff();
void WIFIUSER_StartScan();
void WIFIUSER_StartConnect(const char* ssid, const char* password);
void WIFIUSER_CancelConnect();

// 定义回调函数类型，用于把底层结果通知给UI层
typedef void (*WifiScanCallback)(int count);
typedef void (*WifiConnectCallback)(bool success);

// 注册回调的接口
void WIFIUSER_SetScanCallback(WifiScanCallback cb);
void WIFIUSER_SetConnectCallback(WifiConnectCallback cb);

#endif