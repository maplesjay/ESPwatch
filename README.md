ESP32-S3 AI SmartWatch 

基于 ESP32-S3 开发的工业级架构智能手表。采用 FreeRTOS 纯异步多线程架构，融合 LVGL 丝滑 UI，支持 Edge Impulse 本地神经网络唤醒与 DeepSeek 云端大模型对话。


Features
基于 FreeRTOS 划分 UI、传感器、网络/AI 三大独立任务，UI 帧率不受网络请求和本地推理阻塞
ASRPRO 独立硬件离线极速识别
Edge Impulse 本地神经网络
接入 DeepSeek 大语言模型，实现打字机效果的流式对话
基于 LVGL 构建，支持全功能计算器、天气预报、多组闹钟与动态倒计时
集成 MAX30102 心率血氧检测、HMC5883L 电子罗盘


主控 MCU ESP32-S3 N16R8 
屏幕  240*280 LCD (带 CST816T 触摸)  TFT_eSPI 驱动 
麦克风  INMP441  I2S 数字麦克风 
传感器  MAX30102, HMC5883L  挂载于 I2C 总线 
语音模块 ASRPRO  通过 UART 串口通信 



本项目使用 VSCode + PlatformIO开发。

目前还未进行低功耗按键语音输入，神经网络还未完全进行低功耗我的构想是将ui中设置一个按键，按住开启识别，成功后关闭识别，随后调用ai，这是我想的低功耗方法，希望指正与指导
Here is the English translation for your GitHub README. I have polished the technical terminology to make it sound highly professional and standard for the open-source hardware community!

---

 ESP32-S3 AI SmartWatch

An industrial-grade smartwatch built on the ESP32-S3. It utilizes a pure asynchronous multi-threaded architecture based on FreeRTOS, integrated with a silky-smooth LVGL UI, and supports local neural network wake-up via Edge Impulse alongside cloud-based large language model conversations via DeepSeek.

 Features

 **FreeRTOS Architecture:** Divided into three independent tasks (UI, Sensors, Network/AI) ensuring the UI frame rate is never blocked by network requests or local AI inference.
 **Offline Voice Recognition:** Lightning-fast offline speech recognition using the ASRPRO independent hardware module.
 **Edge AI:** Local neural network integration powered by Edge Impulse.
 **Cloud LLM Integration:** Connected to the DeepSeek Large Language Model for streaming conversations with a typewriter effect.
 **Rich UI Interactions:** Built with LVGL, featuring a fully functional calculator, weather forecasts, multiple alarms, and dynamic countdown timers.
 **Sensor Fusion:** Integrated MAX30102 heart rate & blood oxygen sensor and HMC5883L electronic compass.

 Hardware Bill of Materials (BOM)

* **Main MCU:** ESP32-S3 N16R8
* **Display:** 240x280 LCD (with CST816T capacitive touch), driven by TFT_eSPI
* **Microphone:** INMP441 I2S digital microphone
* **Sensors:** MAX30102, HMC5883L (mounted on the I2C bus)
* **Voice Module:** ASRPRO (communicates via UART)

 Development Environment

This project is developed using **VSCode + PlatformIO**.

 Current Status & Future Plans

Currently, the low-power push-to-talk voice input hasn't been implemented yet, and the neural network hasn't been fully optimized for low power consumption. My current concept is to add a button in the UI: hold it down to start voice recognition, turn off the recognition/microphone once successful, and then trigger the cloud AI. This is my proposed approach to achieving low power consumption. I would greatly appreciate any corrections, guidance, or feedback on this idea!