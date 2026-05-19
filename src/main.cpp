#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Wire.h>
#include "cst816t.h"
#include "UI/ui.h"
#include "GETWEATHER/weather.h"
#include "WIFIUSER/WIFIUSER.h"
#include "CLOCK/CLOCK.h"
#include "HEALTH/HEALTH.h"
#include "AICHAT/AICHAT.h"
#include "COMPASS/COMPASS.h"

// 引脚（触摸）
#define TP_SDA  18
#define TP_SCL  8
#define TP_RST  39   
#define TP_IRQ  40   

cst816t touch(Wire, TP_RST, TP_IRQ);

extern void onWifiScanFinished(int n);
extern void onWifiConnectFinished(bool success);
extern void onTimeUpdated(const char* timeStr);

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 280

TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

// 两把互斥锁
SemaphoreHandle_t xGuiSemaphore;
SemaphoreHandle_t xI2CSemaphore; 

// 帧率刷新
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    
    lv_disp_flush_ready(disp);
}

// 读触摸
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    // 触摸屏也挂在 I2C 
    if (xI2CSemaphore != NULL && xSemaphoreTake(xI2CSemaphore, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (touch.available()) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch.x;
            data->point.y = touch.y;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
        xSemaphoreGive(xI2CSemaphore); // 读完马上还锁
    }
}

// UI任务优先级 5
void Task_UI(void *pvParameters) {
    while (1) {
        // 尝试获取互斥锁，最多等待 10 毫秒
        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_timer_handler(); // 只有拿到锁，才能刷新屏幕
            xSemaphoreGive(xGuiSemaphore); // 刷新完立刻释放锁
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // 出让 5ms CPU，防止看门狗复位
    }
}

// 传感器任务优先级 3
void Task_Sensors(void *pvParameters) {
    static unsigned long lastSyncCheck = 0;
    while (1) {
        CLOCKUSER_Loop();
        HEALTH_Loop();
        COMPASS_Loop();

        // 每秒检查一次是否拿到了网络时间，拿到了就会自动刷新日历
        if (millis() - lastSyncCheck > 1000) {
            lastSyncCheck = millis();
            CLOCKUSER_CheckAndRefresh(); 
        }
        
        vTaskDelay(pdMS_TO_TICKS(20)); // 休眠 20ms，让出 CPU
    }
}


// 网络任务 优先级 2
void Task_NetworkAI(void *pvParameters) {
    while (1) {
        WIFIUSER_Loop();
        AICHAT_Loop(); 
        
        vTaskDelay(pdMS_TO_TICKS(30)); // 休眠 30ms
    }
}

void setup() {
    Serial.begin(115200);

    // 把锁造出
    Serial.println("正在创建全局互斥锁...");
    xGuiSemaphore = xSemaphoreCreateMutex();
    xI2CSemaphore = xSemaphoreCreateMutex();
    if (xGuiSemaphore == NULL || xI2CSemaphore == NULL) {
        Serial.println("严重错误：互斥锁创建失败！系统挂起！");
        while(1); 
    }

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); 
    
    // 等待电脑串口
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000) {
        delay(10);
    }
    
    // 开机画面
    tft.init();
    tft.setRotation(0);
    
    Serial.println("填充红色...");
    tft.fillScreen(TFT_RED);   
    delay(2000);
    
    Serial.println("填充蓝色...");
    tft.fillScreen(TFT_BLUE);  
    delay(2000);
    
    tft.fillScreen(TFT_BLACK);
    Serial.println("TFT裸屏测试完成,开始初始化LVGL...");
    
    // 初始化LVGL 
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // 初始化触摸+心率
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setClock(100000);
    touch.begin(mode_touch);
    Serial.println("触摸初始化完成");
    HEALTH_Init();

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // 创建UI
    ui_init();
    CLOCKUSER_repire();
    WIFIUSER_Init();
    
    // 闹钟窗口弹出
    lv_obj_set_parent(ui_uiPopupAlert, lv_layer_top());
    lv_obj_align(ui_uiPopupAlert, LV_ALIGN_CENTER, 0, 0);
    CLOCKUSER_SetOfflineDate(2024, 5, 20); 
    CLOCKUSER_SetupCalendarToToday(); 
    
    // 绑定回调函数
    WIFIUSER_SetScanCallback(onWifiScanFinished);
    WIFIUSER_SetConnectCallback(onWifiConnectFinished);
    CLOCKUSER_SetCallback(onTimeUpdated);
    
    lv_dropdown_set_options(ui_DropdownWifi, "Turn ON WiFi first");

    // 初始化语音和指南针
    AICHAT_Init();
    COMPASS_Init();
    Serial.println("基础外设 Setup 完成");

    // RTOS 启动 (去掉了原来底部的锁创建)
    Serial.println("正在将任务 FreeRTOS ");
    
    xTaskCreatePinnedToCore(Task_UI, "UI_Task", 10000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors_Task", 8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(Task_NetworkAI, "NetAI_Task", 32768, NULL, 2, NULL, 0); 

    Serial.println("RTOS 启动成功！");
}

void loop() {
    // 【修复 4】不要用 vTaskDelete 杀进程，改成永久休眠，防止看门狗咬人！
    vTaskDelay(portMAX_DELAY); 
}