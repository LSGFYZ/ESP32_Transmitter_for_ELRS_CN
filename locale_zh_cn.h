#pragma once
#include <string.h>

// ESP32C3-TX 简体中文显示字典。
// 左侧必须保留源码/ELRS返回的原始英文；右侧只影响屏幕显示，不影响 NVS 键名和 CRSF 协议。
struct UiTranslation { const char* source; const char* chinese; };
static const UiTranslation UI_TRANSLATIONS[] = {
    {"Radio Settings", "遥控器设置"}, {"Radio", "无线电"}, {"radio", "无线电"},
    {"TX Module", "发射模块"}, {"Stick Mode", "摇杆模式"},
    {"ADC Filter", "摇杆滤波"}, {"Deadband", "中位死区"},
    {"Long Press", "长按时间"}, {"Volt Scale", "电压校准"},
    {"Ail/Roll", "副翼/横滚"}, {"Ele/Pitch", "升降/俯仰"},
    {"Throttle", "油门"}, {"Rud/Yaw", "方向/偏航"},
    {"Center Throt", "油门居中"}, {"Ail Reverse", "副翼反向"},
    {"Ail Limit", "副翼行程"}, {"Ail Expo", "副翼指数"},
    {"Ele Reverse", "升降反向"}, {"Ele Limit", "升降行程"},
    {"Ele Expo", "升降指数"}, {"Thr Reverse", "油门反向"},
    {"Thr Limit", "油门行程"}, {"Thr Expo", "油门指数"},
    {"Yaw Reverse", "方向反向"}, {"Yaw Limit", "方向行程"},
    {"Yaw Expo", "方向指数"}, {"Normal", "正常"}, {"Reverse", "反向"},
    {"Mode 1", "模式 1"}, {"Mode 2", "模式 2"},
    {"Mode 3", "模式 3"}, {"Mode 4", "模式 4"},
    {"Off", "关闭"}, {"On", "开启"}, {"No", "否"}, {"Yes", "是"},
    {"Channel Output", "通道输出"}, {"BLE Joystick", "蓝牙摇杆"},
    {"ON", "开启"}, {"OFF", "关闭"}, {"Packet rate ???", "包速率：未知"},
    {"TX power ???", "发射功率：未知"}, {"Loading Params...", "正在读取参数…"},
    {"No TX Module", "未检测到发射模块"}, {"CALIBRATION", "摇杆校准"},
    {"Starting...", "正在开始…"}, {"Center Sticks ", "摇杆回中 "},
    {"Move Sticks ", "活动摇杆 "}, {"Complete", "校准完成"},
    {"[Running...]", "[正在执行…]"}, {"[Confirm?]", "[确认执行？]"},
    {"Armed", "已解锁"}, {"Disarmed", "已锁定"},
    {"Link Statistics", "链路统计"}, {"RSSI", "信号强度"},
    {"Model Finder", "模型查找"}, {"Running", "运行中"},
    {"Confirm", "确认"}, {"Cancel", "取消"}, {"Back", "返回"},
    {"Save", "保存"}, {"Exit", "退出"}, {"Reset", "重置"},
    {"Packet Rate", "包速率"}, {"ELRS Config", "ELRS配置"},
    {"Power", "功率"}, {"Model", "模型"}, {"Bind", "对频"},{"Max Power", "最大功率"}, 
    {"WiFi Connectivity", "Wi-Fi连接"}, {"WiFi", "Wi-Fi"},
    {"Telem Ratio", "遥测比例"}, {"Serial Protocol", "串口协议"},
    {"Switch Mode", "开关模式"}, {"Packet Rate", "包速率"},
    {"Dynamic Power", "动态功率"}, {"Model Match", "模型匹配"},
    {"Telemetry", "遥测"}, {"Connected", "已连接"}, {"Disconnected", "未连接"},{"Link Mode", "连接模式"}, 
    {"Starting", "启动中"}, {"Waiting", "等待中"}, {"Loading", "加载中"},
    {"Standard", "标准"}, {"Dynamic", "动态"}, {"Wide", "宽"},{"Hybrid", "混合"},
    {"25Hz", "25Hz"}, {"50Hz", "50Hz"}, {"100Hz", "100Hz"},
    {"150Hz", "150Hz"}, {"200Hz", "200Hz"}, {"250Hz", "250Hz"},
    {"333Hz", "333Hz"}, {"500Hz", "500Hz"}, {"DShot", "DShot"},
};

static inline const char* translateUiText(const char* text) {
    if (!text) return "";
    for (size_t i = 0; i < sizeof(UI_TRANSLATIONS) / sizeof(UI_TRANSLATIONS[0]); ++i) {
        if (strcmp(text, UI_TRANSLATIONS[i].source) == 0) return UI_TRANSLATIONS[i].chinese;
    }
    return text;
}
static inline bool isUiTextTranslated(const char* text) {
    return text && translateUiText(text) != text;
}
