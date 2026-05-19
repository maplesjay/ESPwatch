#include "CLOCK.h"
#include <Arduino.h>
#include <time.h>
#include "UI/ui.h"
#include <sys/time.h> // 底层时间设置
#include <esp_sntp.h> // NTP 状态检查
#define TFT_BL 1                
bool isScreenSleeping = false;  
volatile bool ntpSyncCompleted = false;
extern SemaphoreHandle_t xGuiSemaphore;
// 初始化 5 个闹钟档案槽位
AlarmData myAlarms[5] = {
    {0, 0, false}, {0, 0, false}, {0, 0, false}, {0, 0, false}, {0, 0, false}
};
static const char* ntpServer = "ntp.aliyun.com"; 
static const long  gmtOffset_sec = 8 * 3600;     
static const int   daylightOffset_sec = 0;       

static TimeUpdateCallback uiUpdateCb = nullptr; 
static unsigned long lastTimeUpdate = 0;        

int timerCountdownSec = -1;   // 剩余秒数
int totalStartSec = 0;        // 总秒数
bool isTimerRunning = false;  // 定时器是否正在运行的开关

//  回调函数
void CLOCKUSER_SetCallback(TimeUpdateCallback cb) {
    uiUpdateCb = cb;
}
void timeSyncCallback(struct timeval *tv) {
    ntpSyncCompleted = true; // 告诉主循环，时间已经拿到
}
//定时器读秒
void CLOCKUSER_StartTimer(int h, int m, int s) {
    totalStartSec = (h * 3600) + (m * 60) + s;
    timerCountdownSec = totalStartSec;
    isTimerRunning = true; 
    
    // 刚启动时，把进度条拉满，按钮文字重置为 STOP
        lv_arc_set_value(ui_TimerArc, 100); 
        lv_label_set_text(ui_LabelTimerStop, "STOP"); 
}
//  网络对表
void CLOCKUSER_Sync() {
    //Serial.println("Time 开始通过网络强制对表...");
    // 回调timeSyncCallback
    sntp_set_time_sync_notification_cb(timeSyncCallback); 
    
    // 强制立即覆盖时间
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

//  状态机
void CLOCKUSER_Loop() {
    uint32_t inactive_time = lv_disp_get_inactive_time(NULL);

    if (inactive_time > 10000 && !isScreenSleeping) {
        digitalWrite(TFT_BL, LOW); // 关灯
        isScreenSleeping = true;
        Serial.println(" 发呆 10 秒，进入息屏模式");
    } 
    else if (inactive_time < 500 && isScreenSleeping) {
        digitalWrite(TFT_BL, HIGH); // 开灯
        isScreenSleeping = false;
        Serial.println(" 检测到触摸，屏幕唤醒");
    }
    // 检查是否过了1秒
    if (millis() - lastTimeUpdate > 1000) {
        lastTimeUpdate = millis();
        if (isTimerRunning && timerCountdownSec > 0) {
            timerCountdownSec--; // 剩余时间减 1 秒

            // 更新大号数字显示
            int h = timerCountdownSec / 3600;
            int m = (timerCountdownSec % 3600) / 60;
            int s = timerCountdownSec % 60;
            char buf[12];
            sprintf(buf, "%02d:%02d:%02d", h, m, s);

            // 更新 Arc 进度条百分比
            int percent = (timerCountdownSec * 100) / totalStartSec;
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
                lv_label_set_text(ui_Label33, buf); 
                lv_arc_set_value(ui_TimerArc, percent);
                xSemaphoreGive(xGuiSemaphore);
            }
            
        } 
        // 当倒计时刚好到 0 时
        else if (isTimerRunning && timerCountdownSec == 0) {
            isTimerRunning = false;
            timerCountdownSec = -1;
            Serial.println(" 定时器时间到！");

            // 呼出全局闹钟弹窗
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(20)) == pdTRUE) {
                // 呼出全局闹钟弹窗
                lv_obj_clear_flag(ui_uiPopupAlert, LV_OBJ_FLAG_HIDDEN);

                // 恢复定时器界面的默认状态
                lv_arc_set_value(ui_TimerArc, 100);
                lv_label_set_text(ui_Label33, "00:00:00");
                lv_label_set_text(ui_LabelTimerStop, "STOP");

                // 隐藏倒计时界面，切回设置滚轮界面
                lv_obj_add_flag(ui_TimerRunGroup, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_TimerSetGroup, LV_OBJ_FLAG_HIDDEN);
                xSemaphoreGive(xGuiSemaphore);
            }
        }

        // 空指针直接返回
        if (uiUpdateCb == nullptr) return; 

        struct tm timeinfo;
        char timeString[16];

        // 获取真实时间
        if (getLocalTime(&timeinfo, 0)) {
            // 真实时间
            sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            for (int i = 0; i < 5; i++) {
                if (myAlarms[i].isActive) {
                    //一分钟只弹一次
                    if (timeinfo.tm_hour == myAlarms[i].hour && 
                        timeinfo.tm_min == myAlarms[i].minute && 
                        timeinfo.tm_sec == 0) {
                        
                        Serial.println("闹钟触发");
                        // 呼出全屏响铃弹窗
                        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
                            lv_obj_clear_flag(ui_uiPopupAlert, LV_OBJ_FLAG_HIDDEN);
                            xSemaphoreGive(xGuiSemaphore);
                        }
                    }
                }
            }
        
        } else {
            // 没拿时间，开机虚拟时间
            unsigned long currentSecs = millis() / 1000;
            int h = (currentSecs / 3600) % 24;
            int m = (currentSecs / 60) % 60;
            sprintf(timeString, "%02d:%02d", h, m);
        }

        // 把算好的 timeString 传给前厅 UI
        uiUpdateCb(timeString);
    }
}

void CLOCKUSER_repire()
{
    // 闹钟分钟修复
    lv_obj_set_style_text_line_space(ui_Roller5, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(ui_Roller5, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_top(ui_Roller5, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_bottom(ui_Roller5, 0, LV_PART_SELECTED);

    // 闹钟小时修复
    lv_obj_set_style_text_line_space(ui_Roller4, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(ui_Roller4, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_top(ui_Roller4, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_bottom(ui_Roller4, 0, LV_PART_SELECTED);
    // 定时器修复分钟
    lv_obj_set_style_text_line_space(ui_Roller2, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(ui_Roller2, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_top(ui_Roller2, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_bottom(ui_Roller2, 0, LV_PART_SELECTED);
    // 定时器修复秒数
    lv_obj_set_style_text_line_space(ui_Roller3, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(ui_Roller3, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_top(ui_Roller3, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_bottom(ui_Roller3, 0, LV_PART_SELECTED);
}
// 离线强制设置系统日期

void CLOCKUSER_SetOfflineDate(int year, int month, int day) {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    time_t timeSinceEpoch = mktime(&t);
    struct timeval now = { .tv_sec = timeSinceEpoch, .tv_usec = 0 };
    settimeofday(&now, NULL);
}


// 检查联网状态并自动刷新日历
void CLOCKUSER_CheckAndRefresh() {
if (ntpSyncCompleted) {
        Serial.println(" 接收到 NTP 回调时间已校准，正在刷新日历...");
        
        CLOCKUSER_SetupCalendarToToday(); // 刷新日历
        
        ntpSyncCompleted = false; // 归零，防止无限刷
        }
}

// 将日历控件同步为当前日期
void CLOCKUSER_SetupCalendarToToday() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        int year = timeinfo.tm_year + 1900;
        int month = timeinfo.tm_mon + 1;
        int day = timeinfo.tm_mday;
        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_calendar_set_today_date(ui_Calendar1, year, month, day);
            lv_calendar_set_showed_date(ui_Calendar1, year, month);
            xSemaphoreGive(xGuiSemaphore);
        }
    }
}