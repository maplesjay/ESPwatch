#include "AICHAT.h"
#include <Arduino.h>
#include "../UI/ui.h" 
#include <WiFi.h>           
#include <HTTPClient.h>     
#include <ArduinoJson.h>
#include <Maples-project-1_inferencing.h>   

// 声明外部
extern SemaphoreHandle_t xGuiSemaphore;

//串口引脚
#define ASR_RX_PIN 16 
#define ASR_TX_PIN 17 

// deepseek api
const char* api_key = "YOUR_API_KEY_HERE"; // 请在此填入你自己的火山引擎 DeepSeek
const char* api_url = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";

//全局变量
AIState currentState = STATE_IDLE;
//检查时间
unsigned long stateTimer = 0;      
unsigned long lastWifiCheckTime = 0; 

String targetAIText = "";          
String currentDisplayText = "";    // 聊天记录本
int textIndex = 0;                 

const int TYPE_SPEED = 60;         // 打字速度

bool isInitialState = true;        //  标志，ture清空聊天框，false聊天框有东西

//  UTF-8 字符长度检测器汉语必备
int getUtf8CharLength(unsigned char byte) {
    if ((byte & 0x80) == 0x00) return 1; 
    if ((byte & 0xE0) == 0xC0) return 2; 
    if ((byte & 0xF0) == 0xE0) return 3; 
    if ((byte & 0xF8) == 0xF0) return 4; 
    return 1; 
}

//  动态刷新初始画面的网络状态
void updateNetworkUI() {
    if (!isInitialState) return; 

    if (WiFi.status() == WL_CONNECTED) {
        currentDisplayText = "请使用AI\n";
    } else {
        currentDisplayText = "请链接网络\n";
    }
    
    // 因为这个函数在 setup() 时也会被调用 锁还没创建
    if (xGuiSemaphore != NULL) {
        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_label_set_text(ui_LabelAIText, currentDisplayText.c_str());
            xSemaphoreGive(xGuiSemaphore);
        }
    } else {
        // 如果锁还没创建（开机阶段），说明 RTOS 还没启动，直接裸奔安全修改
        lv_label_set_text(ui_LabelAIText, currentDisplayText.c_str());
    }
}

//  初始化 ASRPRO 串口
void AICHAT_Init() {
    Serial.println("正在初始化 ASRPRO 语音串口...");
    Serial1.begin(9600, SERIAL_8N1, ASR_RX_PIN, ASR_TX_PIN);
    
    // 初始化时系统还没开始跑多线程，直接操作安全
    lv_obj_clear_flag(ui_LabelAIText, LV_OBJ_FLAG_HIDDEN);
    isInitialState = true;
    updateNetworkUI(); // 开机先检查一次网络，显示初始提示
    
    Serial.println(" ASRPRO 准备就绪！等待语音指令...");
}

//  向云端发起网络请求 (底层数据交互，不用锁)
String askRealAI(String question) {
    if (WiFi.status() != WL_CONNECTED) {
        return "哎呀，手表的 WiFi 好像断开了，请检查网络！";
    }

    Serial.println(" 正在连接云端大模型...");
    HTTPClient http;
    http.begin(api_url);
    http.setTimeout(10000); // 10秒超时，在后台随便卡，不影响前台动画！
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + api_key);

    JsonDocument doc;
    doc["model"] = "ep-20260428204656-lp6td"; 

    JsonObject sysMsg = doc["messages"].add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "你是一个智能手表语音助手回答必须简短幽默字数控制在60字以内。";
    
    JsonObject userMsg = doc["messages"].add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = question;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    String answer = "抱歉，我的大脑突然短路了。";

    if (httpResponseCode == 200) {
        String response = http.getString();
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response);
        
        if (!error && !respDoc["choices"].isNull()) {
            answer = respDoc["choices"][0]["message"]["content"].as<String>();
            Serial.println(" 成功获取云端回复");
        } else {
            Serial.println(" JSON 解析失败");
        }
    } else {
        Serial.printf(" API 请求失败，错误码: %d\n", httpResponseCode);
        Serial.println(http.getString()); 
    }

    http.end();
    return answer;
}

//  触发 UI 开始思考
void triggerUIThinking(String userSaid) {
    currentState = STATE_THINKING;
    
    if (!isInitialState && currentDisplayText.length() > 0) {
        currentDisplayText += "\n\n"; 
    }

    if (isInitialState) {
        currentDisplayText = ""; 
        isInitialState = false;
    }
    
    currentDisplayText += "你 " + userSaid + "\n";
    String thinkingStr = currentDisplayText + "AI 云端思考中...";
    
    // 上屏思考，抢锁
    if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(20)) == pdTRUE) {
        lv_label_set_text(ui_LabelAIText, thinkingStr.c_str()); 
        lv_obj_scroll_to_y(ui_PanelAIChat, 10000, LV_ANIM_ON);
        xSemaphoreGive(xGuiSemaphore);
    }
    
}

//  语音循环
void AICHAT_Loop() {
    
    if (currentState == STATE_IDLE && isInitialState) {
        if (millis() - lastWifiCheckTime > 1500) {
            lastWifiCheckTime = millis();
            updateNetworkUI();
        }
    }

    if (Serial1.available() > 0) {
        uint8_t command = Serial1.read(); 
        Serial.printf("\n 收到语音指令代号: 0x%02X\n", command);

        if (WiFi.status() != WL_CONNECTED) {
            String warnText = isInitialState ? "请链接网络\n" : currentDisplayText + "\n网络已断开";
            
            // 断网上屏，抢锁
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(20)) == pdTRUE) {
                lv_label_set_text(ui_LabelAIText, warnText.c_str());
                lv_obj_scroll_to_y(ui_PanelAIChat, 10000, LV_ANIM_ON);
                xSemaphoreGive(xGuiSemaphore);
            }
            return; 
        }

        String userSpoke = ""; 
        String prompt = "";    

        switch (command) {
            case 0x02: 
                userSpoke = "讲个冷笑话";
                prompt = "给我讲一个极其好笑的冷笑话。"; 
                break;
            case 0x01: 
                userSpoke = "今天天气怎么样";
                prompt = "今天郑州天气怎么样？穿什么衣服合适？"; 
                break;
            case 0x03: 
                userSpoke = "讲个故事";
                prompt = "给我科普一个非常冷门的宇宙小知识。"; 
                break;
        }

        if (prompt != "") {
            triggerUIThinking(userSpoke);
            
        
            String realAnswer = askRealAI(prompt);
            
            currentDisplayText += "AI "; 
            targetAIText = realAnswer;
            currentState = STATE_TYPING;
            textIndex = 0;
            stateTimer = millis();
            
            // 拿到云端回复，清除思考中，准备打字。抢锁
            if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(20)) == pdTRUE) {
                lv_label_set_text(ui_LabelAIText, currentDisplayText.c_str()); 
                xSemaphoreGive(xGuiSemaphore);
            }
        }
    }

    // 打字机动画流转 
    if (currentState == STATE_TYPING) {
        if (millis() - stateTimer > TYPE_SPEED) {
            stateTimer = millis();

            if (textIndex < targetAIText.length()) {
                unsigned char currentByte = targetAIText[textIndex];
                int charLen = getUtf8CharLength(currentByte);
                
                String nextChar = targetAIText.substring(textIndex, textIndex + charLen);
                currentDisplayText += nextChar;
                textIndex += charLen;

                // 打出一个字，抢锁
                if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
                    lv_label_set_text(ui_LabelAIText, currentDisplayText.c_str());
                    lv_obj_scroll_to_y(ui_PanelAIChat, 10000, LV_ANIM_ON);
                    xSemaphoreGive(xGuiSemaphore);
                }
                Serial.print(nextChar);
            } else {
                currentState = STATE_IDLE;
                Serial.println("\n[AI 输出完毕]");
            }
        }
    }
}

//  清空记忆功能
void AICHAT_ResetChat() {
    if (currentState != STATE_IDLE) {
        Serial.println(" AI 忙碌中，忽略重置请求");
        return; 
    }

    isInitialState = true;     
    currentDisplayText = "";   
    
    currentState = STATE_IDLE;
    textIndex = 0;
    targetAIText = "";

    updateNetworkUI(); // 这个函数内部自带锁

    // 让滚动条回顶，涉及 UI 更新，抢锁
        lv_obj_scroll_to_y(ui_PanelAIChat, 0, LV_ANIM_ON);

    Serial.println(" AI 记忆已清空，回到动态初始状态");
}