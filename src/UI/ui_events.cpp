// SquareLine Studio version: SquareLine Studio 1.6.0
// LVGL version: 8.3.11
// Project name: ESP32

#include "ui.h"
#include "ui_events.h"
#include "../GETWEATHER/weather.h"     
#include "../WIFIUSER/WIFIUSER.h"
#include "../CLOCK/CLOCK.h"
#include "../HEALTH/HEALTH.h"
#include "../AICHAT/AICHAT.h"                                                                                                                                                                                                                       
char current_ssid[64] = "";
extern bool isScanning;
extern WeatherData getWeatherData(String queryCity);
extern unsigned long switchOpenTime;
extern SemaphoreHandle_t xGuiSemaphore;

//计算器变量
String calcInput = "0";      
float calcFirstNum = 0;                                                                                                 
char calcOp = ' ';           
bool isNewInput = true;


void onSyncTimeClicked(lv_event_t * e)
{
	// Your code here
    if (WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(ui_time, "No WiFi"); 
        return; 
    }

    // 强行刷新提示文字
    lv_label_set_text(ui_time, "Syncing...");
    
    // 呼叫底层去对表
    CLOCKUSER_Sync();
}
// 当底层每秒算好时间后，会自动跳到这里执行
void onTimeUpdated(const char* timeStr) {
    // 把底层传过来的字符串贴到 LVGL 标签上
    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
    lv_label_set_text(ui_time, timeStr);
    xSemaphoreGive(xGuiSemaphore);
    }
}


void onAlarmSwitchToggled(lv_event_t * e)
{
	// Your code here
lv_obj_t* alarmCards[5] = {ui_AlarmCard1, ui_AlarmCard2, ui_AlarmCard3, ui_AlarmCard4, ui_AlarmCard5};
lv_obj_t* alarmLabels[5] = {ui_AlarmTime1, ui_AlarmTime2, ui_AlarmTime3, ui_AlarmTime4, ui_AlarmTime5};
lv_obj_t* alarmSwitches[5] = {ui_AlarmSwitch1, ui_AlarmSwitch2, ui_AlarmSwitch3, ui_AlarmSwitch4, ui_AlarmSwitch5};
    // 抓取是哪个开关被拨动了
    lv_obj_t * clicked_switch = lv_event_get_target(e);
    // 找到这个开关属于哪张卡片
    lv_obj_t * parent_card = lv_obj_get_parent(clicked_switch);

    // 遍历数组，确认是第几个闹钟
    for(int i = 0; i < 5; i++) {
        if(alarmCards[i] == parent_card) {
            // 获取当前开关是处于 ON 还是 OFF 状态
            bool is_on = lv_obj_has_state(clicked_switch, LV_STATE_CHECKED);
            
            //同步修改底层 CLOCK 模块里的档案 
            myAlarms[i].isActive = is_on;
            
            Serial.printf("闹钟开关改变: 槽位 %d 已 %s\n", i, is_on ? "开启" : "关闭");
            break; // 修改完毕，退出循环
        }
    }
}


void onDeleteClicked(lv_event_t * e)
{
	// Your code here
lv_obj_t* alarmCards[5] = {ui_AlarmCard1, ui_AlarmCard2, ui_AlarmCard3, ui_AlarmCard4, ui_AlarmCard5};
lv_obj_t* alarmLabels[5] = {ui_AlarmTime1, ui_AlarmTime2, ui_AlarmTime3, ui_AlarmTime4, ui_AlarmTime5};
lv_obj_t* alarmSwitches[5] = {ui_AlarmSwitch1, ui_AlarmSwitch2, ui_AlarmSwitch3, ui_AlarmSwitch4, ui_AlarmSwitch5};
    //抓取是谁删除被点了
    lv_obj_t * clicked_btn = lv_event_get_target(e);
    // 找到它所在的卡片
    lv_obj_t * parent_card = lv_obj_get_parent(clicked_btn);

    // 遍历数组，确认是第几个闹钟
    for(int i = 0; i < 5; i++) {
        if(alarmCards[i] == parent_card) {
            // 擦除底层的闹钟记录 
            myAlarms[i].isActive = false; // 关掉开关
            myAlarms[i].hour = 0;         // 时间清零
            myAlarms[i].minute = 0;       
            
            // 卡片被隐藏
            lv_obj_add_flag(alarmCards[i], LV_OBJ_FLAG_HIDDEN);
            
            Serial.printf("删除闹钟成功: 槽位 %d\n", i);
            break; 
        }
    }
}

void onAddOkClicked(lv_event_t * e)
{
	// Your code here
    lv_obj_t* alarmCards[5] = {ui_AlarmCard1, ui_AlarmCard2, ui_AlarmCard3, ui_AlarmCard4, ui_AlarmCard5};
lv_obj_t* alarmLabels[5] = {ui_AlarmTime1, ui_AlarmTime2, ui_AlarmTime3, ui_AlarmTime4, ui_AlarmTime5};
lv_obj_t* alarmSwitches[5] = {ui_AlarmSwitch1, ui_AlarmSwitch2, ui_AlarmSwitch3, ui_AlarmSwitch4, ui_AlarmSwitch5};
    // 获取滚轮选中的时和分
    int selectedHour = lv_roller_get_selected(ui_Roller4);
    int selectedMin = lv_roller_get_selected(ui_Roller5);

    for(int i = 0; i < 5; i++) {
        // 寻找隐藏状态
        if(lv_obj_has_flag(alarmCards[i], LV_OBJ_FLAG_HIDDEN)) {
            
            // 写入CLOCK.h 里的数组
            myAlarms[i].hour = selectedHour;
            myAlarms[i].minute = selectedMin;
            myAlarms[i].isActive = true;
            // 把开关开开
            lv_obj_add_state(alarmSwitches[i], LV_STATE_CHECKED);
            
            // 改写卡片上的时间文字
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d", selectedHour, selectedMin);
            lv_label_set_text(alarmLabels[i], timeStr);
            
            // 解除隐藏
            lv_obj_clear_flag(alarmCards[i], LV_OBJ_FLAG_HIDDEN);
            
            Serial.printf("添加闹钟成功: 槽位 %d, 时间 %02d:%02d\n", i, selectedHour, selectedMin);
            break; // 填进去一个就马上退出循环
        }
    }
    
    //把添加时间的弹窗收起来
    lv_obj_add_flag(ui_PopupAdd, LV_OBJ_FLAG_HIDDEN);
}



void onWifiSwitchToggle(lv_event_t * e)
{
	// Your code here
lv_obj_t * switch_obj = lv_event_get_target(e);
    
    if (lv_obj_has_state(switch_obj, LV_STATE_CHECKED)) {
        Serial.println("WiFi 开关已打开，正在初始化射频模块...");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(); 
        
        //给射频模块 100 毫秒的唤醒和准备时间
        vTaskDelay(pdMS_TO_TICKS(100));
        
        lv_dropdown_clear_options(ui_DropdownWifi);
        lv_dropdown_add_option(ui_DropdownWifi, "Please click REFRESH", 0);
        Serial.println("WiFi 初始化完成，可以扫描了。");
    } else {
        Serial.println("WiFi 已关闭");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        lv_dropdown_clear_options(ui_DropdownWifi);
        lv_dropdown_add_option(ui_DropdownWifi, "WiFi is OFF", 0);
        isScanning = false; 
    }
}

void WifiDropdownChanged(lv_event_t * e)
{
	// Your code here
    lv_dropdown_get_selected_str(ui_DropdownWifi, current_ssid, sizeof(current_ssid));
}

void PasswordBtnClicked(lv_event_t * e)
{
	// Your code here

    lv_dropdown_get_selected_str(ui_DropdownWifi, current_ssid, sizeof(current_ssid));
    lv_label_set_text(ui_Label9, current_ssid);
    lv_textarea_set_text(ui_TextArea1, "");
}

void wifirefresh(lv_event_t * e)
{
	// Your code here
// 检查开关是否打开
    if (WiFi.getMode() != WIFI_STA) {
        Serial.println(" 错误：请先打开 WiFi 开关！");
        return; 
    }
    
    // 正在扫描，直接忽略这次点击
    if (isScanning) {
        Serial.println(" 正在扫描中，请勿重复点击...");
        return; 
    }
    
    Serial.println("开始异步扫描 WiFi...");
    lv_dropdown_clear_options(ui_DropdownWifi);
    lv_dropdown_add_option(ui_DropdownWifi, "Scanning...", 0);
    
    // 清理上次扫描内存
    WiFi.scanDelete(); 
    vTaskDelay(pdMS_TO_TICKS(10)); // 稍微缓冲一下
    
    // 启动新的异步扫描
    WIFIUSER_StartScan();
}

void WifiConnectAction(lv_event_t * e)
{
	// Your code here
    const char * pwd = lv_textarea_get_text(ui_TextArea1);
    //先用代码主动切换到“连接中”的屏幕
    lv_disp_load_scr(ui_Screen5); 
    delay(50);
    
    WIFIUSER_StartConnect(current_ssid, pwd); // 调用连接接口
}

void WifiCancelConnect(lv_event_t * e)
{
	// Your code here
    WIFIUSER_CancelConnect();
    lv_textarea_set_text(ui_TextArea1, "");
    lv_disp_load_scr(ui_Screen4);
}
// 当底层扫描完成时，会调用这里
void onWifiScanFinished(int n) {
    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (n == WIFI_SCAN_FAILED) {
        Serial.println("扫描失败");
        lv_dropdown_set_options(ui_DropdownWifi, "Scan Failed");
        return;
    }
    if (n == 0) {
        Serial.println("扫描完成但没有找到任何 WiFi");
        lv_dropdown_set_options(ui_DropdownWifi, "No WiFi found");
        return;
    }
    Serial.printf("扫描完成，底层共找到 %d 个网络\n", n);
    
    String options = "";
    for (int i = 0; i < n; ++i) {
        options += WiFi.SSID(i);
        if (i < n - 1) options += "\n";
    }
    lv_dropdown_set_options(ui_DropdownWifi, options.c_str());
    lv_dropdown_get_selected_str(ui_DropdownWifi, current_ssid, sizeof(current_ssid));
    xSemaphoreGive(xGuiSemaphore);
    }
}

// 当底层连接出结果时，会调用这里
void onWifiConnectFinished(bool success) {
    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (success) {
        CLOCKUSER_Sync();
        lv_disp_load_scr(ui_Screen7); // 成功页
    } else {
        lv_disp_load_scr(ui_Screen6); // 失败页
    }
    xSemaphoreGive(xGuiSemaphore);
    }
}



void CheckWeather(lv_event_t * e)
{
	// Your code here 获取用户在键盘上输入的城市名字
    const char * city_input = lv_textarea_get_text(ui_CITY);
    
    if (strlen(city_input) == 0) {
        Serial.println("城市名不能为空！");
        return; // 如果没输入就点查询，直接退出
    }

    Serial.printf("准备查询城市: %s\n", city_input);
    lv_disp_load_scr(ui_Screen5);
    
    
    // 稍微延时一下，确保用户能看到加载页
    delay(100);
    //  调用 API 查询天气 (将 char* 转为String)
    WeatherData data = getWeatherData(String(city_input));

    //  判断结果并刷新 UI
    if (data.success) {
        // 更新文字 Label
        lv_label_set_text(ui_Label21, data.city_name.c_str());
        
        String tempStr = String(data.temperature) + "度";
        lv_label_set_text(ui_Label22, tempStr.c_str());
        
        lv_label_set_text(ui_Label23, data.weather_info.c_str());

        // 更新天气图标
        //updateWeatherIcon(data.weather_info);

        // 全部数据刷新完毕后，跳转到结果页面 Screen10
        lv_disp_load_scr(ui_Screen10);
        
    } else {
        // 查询失败
        lv_textarea_set_text(ui_CITY, "查询失败,请重试");
    }
}



void onTimerStartClicked(lv_event_t * e)
{
	// Your code here
    //三个滚轮里读取当前的数值
    int h = lv_roller_get_selected(ui_Roller1);
    int m = lv_roller_get_selected(ui_Roller2);
    int s = lv_roller_get_selected(ui_Roller3);
    
    //如果时间全是 0，直接 return 退出，不启动也不切画面
    if(h == 0 && m == 0 && s == 0) {
        return; 
    }

    // 开始倒数
    CLOCKUSER_StartTimer(h, m, s);
    // UI 界面切换

    lv_obj_add_flag(ui_TimerSetGroup, LV_OBJ_FLAG_HIDDEN);
    
    //显示出
    lv_obj_clear_flag(ui_TimerRunGroup, LV_OBJ_FLAG_HIDDEN);

    Serial.printf("定时器成功启动: %02d:%02d:%02d\n", h, m, s);
}

void onTimerBackClicked(lv_event_t * e)
{
	// Your code here
// 隐藏倒计时进度条界面
    lv_obj_add_flag(ui_TimerRunGroup, LV_OBJ_FLAG_HIDDEN);
    
    // 重新显示滚轮设置界面
    lv_obj_clear_flag(ui_TimerSetGroup, LV_OBJ_FLAG_HIDDEN);
    
    Serial.println("已切回设置界面，倒计时在后台继续流逝");
}

void onTimerStopClicked(lv_event_t * e)
{
	// Your code here
    // 防呆
    if(timerCountdownSec <= 0) return;

    if (isTimerRunning) {
        // 在跑 CONTINUE
        isTimerRunning = false;
        lv_label_set_text(ui_LabelTimerStop, "CONTINUE");
    } else {
        // STOP
        isTimerRunning = true;
        lv_label_set_text(ui_LabelTimerStop, "STOP");
    }
}

void onTimerResetClicked(lv_event_t * e) {
    // 从底层拿回当初设定的总时间
    timerCountdownSec = totalStartSec;
    
    // 习惯上重置后会进入等待状态
    isTimerRunning = false; 

    // 刷新 UI 的大字时间
    int h = timerCountdownSec / 3600;
    int m = (timerCountdownSec % 3600) / 60;
    int s = timerCountdownSec % 60;
    char buf[12];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    lv_label_set_text(ui_Label33, buf);

    // 圆环进度条直接拉满 100%
    lv_arc_set_value(ui_TimerArc, 100);

    // 因为进入了暂停状态，所以按钮文字变成 CONTINUE
    lv_label_set_text(ui_LabelTimerStop, "CONTINUE"); 
}


void onTimerCheckClicked(lv_event_t * e)
{
	// Your code here
    if (timerCountdownSec <= 0) {
        Serial.println("后台无任务，拒绝查看");
        return; 
    }

    // 只有后台有任务时，才切换 UI 进行查看
    lv_obj_add_flag(ui_TimerSetGroup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_TimerRunGroup, LV_OBJ_FLAG_HIDDEN);
    
    // 校准 STOP/CONTINUE文字
    if(isTimerRunning) {
        lv_label_set_text(ui_LabelTimerStop, "STOP");
    } else {
        lv_label_set_text(ui_LabelTimerStop, "CONTINUE");
    }

    Serial.println("已切回倒计时监控界面");
}

String formatFloat(float num) {
    if (num == (int)num) {
        return String((int)num); // 完美的整数
    }
    String str = String(num, 5); // 小数最多保留5位
    while(str.endsWith("0")) str.remove(str.length() - 1);
    if(str.endsWith(".")) str.remove(str.length() - 1);
    return str;
}

void onCalcButtonClicked(lv_event_t * e)
{
	// Your code here
lv_obj_t * label = lv_event_get_target(e);
    const char * txt = lv_label_get_text(label);

    // 退出键与基础控制键
    if (strcmp(txt, "R") == 0) {
        calcInput = "0";
        calcFirstNum = 0;
        calcOp = ' ';
        isNewInput = true;
        lv_label_set_text(ui_Label45, "0");
        lv_scr_load_anim(ui_Screen8, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        return; 
    }

    if (strcmp(txt, "C") == 0) {
        calcInput = "0";
        calcFirstNum = 0;
        calcOp = ' ';
        isNewInput = true;
        
    } else if (strcmp(txt, "<-") == 0) {
        if (calcInput.length() > 1) calcInput.remove(calcInput.length() - 1); 
        else { calcInput = "0"; isNewInput = true; }

    } else if (strcmp(txt, "%") == 0) {
        float currentNum = calcInput.toFloat();
        calcInput = formatFloat(currentNum / 100.0);
        isNewInput = true; 

    } else if (strcmp(txt, ".") == 0) {
        if (isNewInput) { calcInput = "0."; isNewInput = false; } 
        else if (calcInput.indexOf('.') == -1) calcInput += ".";
        
    //核心计算：等于号
    } else if (strcmp(txt, "=") == 0) {
        if (calcOp != ' ') {
            float secondNum = calcInput.toFloat();
            float result = 0;
            
            if (calcOp == '+') result = calcFirstNum + secondNum;
            else if (calcOp == '-') result = calcFirstNum - secondNum;
            else if (calcOp == 'X') result = calcFirstNum * secondNum; 
            else if (calcOp == '/') {
                if (secondNum != 0) result = calcFirstNum / secondNum;
                else {
                    calcInput = "Error"; 
                    lv_label_set_text(ui_Label45, calcInput.c_str());
                    calcOp = ' ';
                    isNewInput = true;
                    return;
                }
            }

            calcInput = formatFloat(result); // 算完后覆盖当前屏幕文字
            calcOp = ' ';                    // 清空符号大脑
            isNewInput = true; 
        }
        

    // 加减乘除运算符

    } else if (strcmp(txt, "+") == 0 || strcmp(txt, "-") == 0 || 
               strcmp(txt, "X") == 0 || strcmp(txt, "/") == 0) {
        
        
        if (calcOp != ' ' && !isNewInput) {
            float secondNum = calcInput.toFloat();
            if (calcOp == '+') calcFirstNum = calcFirstNum + secondNum;
            else if (calcOp == '-') calcFirstNum = calcFirstNum - secondNum;
            else if (calcOp == 'X') calcFirstNum = calcFirstNum * secondNum; 
            else if (calcOp == '/') {
                if (secondNum != 0) calcFirstNum = calcFirstNum / secondNum;
            }
            calcInput = formatFloat(calcFirstNum);
        } else {
            calcFirstNum = calcInput.toFloat(); 
        }

        calcOp = txt[0];                    
        isNewInput = true;                  
        

    //  数字输入

    } else {
        if (isNewInput) {
            calcInput = txt; 
            isNewInput = false;
        } else {
            if (calcInput == "0") calcInput = txt; 
            else calcInput += txt;
        }
    }

    String screenText = "";
    
    if (calcOp == ' ') {
        // 大脑里没有符号（刚开机、刚按了归零、或者刚按了等于号算完）
        // 屏幕就只显示当前的独立数字
        screenText = calcInput;
    } else {
        // 大脑里记了符号，咱们就把前面的数字和符号一起显示在屏幕上
        String firstStr = formatFloat(calcFirstNum);
        
        if (isNewInput) {
            // 还没敲第二个数字，屏幕显示： "12 +"
            screenText = firstStr + " " + calcOp;
        } else {
            // 正在敲第二个数字，屏幕显示： "12 + 5"
            screenText = firstStr + " " + calcOp + " " + calcInput;
        }
    }

    // 将拼装好的完美画面，刷新到屏幕上
    lv_label_set_text(ui_Label45, screenText.c_str());
}

void onHealthSwitchChanged(lv_event_t * e)
{
	// Your code here
lv_obj_t * obj = lv_event_get_target(e);
    
    if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        isHealthEnabled = true;
        switchOpenTime = millis(); // 记录按下的瞬间
        Serial.println(" 健康检测：已开启 (为了防冲突等待0.5秒后亮灯启动)");
    } else {
        isHealthEnabled = false;
        Serial.println(" 健康检测：已关闭");
        lv_label_set_text(ui_LabelBPM, "---");
        lv_label_set_text(ui_LabelSpO2, "--- %");
    }
}


void onNewChatClicked(lv_event_t * e)
{
	// Your code here
   AICHAT_ResetChat();
}
