#ifndef AICHAT_H
#define AICHAT_H

#include <Arduino.h>

// 状态机定义
enum AIState {
    STATE_IDLE,       // 待机中
    STATE_THINKING,   // 思考中
    STATE_TYPING      // 打字输出中
};

// 暴露外部接口
void AICHAT_Init();
void AICHAT_Loop();
void AICHAT_ResetChat(); // 暴露给 UI 按钮使用的清空函数

#endif