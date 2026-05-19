#ifndef HEALTH_H
#define HEALTH_H

#include <lvgl.h>

// 声明全局开关，让其他文件（比如 ui_events）能操控它
extern bool isHealthEnabled; 

void HEALTH_Init();
void HEALTH_Loop();

#endif