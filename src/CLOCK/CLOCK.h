#ifndef CLOCK_H
#define CLOCK_H
//  闹钟数据结构
struct AlarmData {
    int hour;
    int minute;
    bool isActive;
};

// 闹钟数组
extern AlarmData myAlarms[5];
//定时器
extern int timerCountdownSec; 
extern int totalStartSec;
extern bool isTimerRunning;

void CLOCKUSER_StartTimer(int h, int m, int s);
// 定义时间更新的回调函数类型
typedef void (*TimeUpdateCallback)(const char* timeStr);

//设置回调函数
void CLOCKUSER_SetCallback(TimeUpdateCallback cb);

//  同步 NTP 时间
void CLOCKUSER_Sync();

// 状态机
void CLOCKUSER_Loop();

//闹钟页面修复
void CLOCKUSER_repire();
//日历设置
void CLOCKUSER_SetOfflineDate(int year, int month, int day);
void CLOCKUSER_SetupCalendarToToday();
void CLOCKUSER_CheckAndRefresh(); 
#endif