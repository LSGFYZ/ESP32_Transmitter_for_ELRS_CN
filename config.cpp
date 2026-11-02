/*
 * 本文件属于 ESP32C3-TX 项目
 *
 * ESP32C3-TX 是自由软件：您可以根据自由软件基金会发布的 GNU 通用公共许可证
 * （第3版或您选择的任何更高版本）的条款重新分发和/或修改它。
 *
 * ESP32C3-TX 的发布是希望它能有用，但不提供任何担保；
 * 甚至没有适销性或特定用途适用性的默示担保。
 * 有关更多详细信息，请参阅 GNU 通用公共许可证。
 * <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <Preferences.h>  // ESP32 NVS 封装库，用于键值对持久化存储
#include <vector>         // 用于传递选项列表
#include "config.h"       // 包含 Handset 类定义及全局设置结构体

// 使用静态局部实例避免全局命名空间污染
// NVS 分区名: "settings"，最大键名长度15字符
static Preferences configPrefs;

// ==================== 加载全局设置（构建菜单 + 读取NVS）====================
// initialize=true: 清空菜单并完整重建（首次启动/重置时）
// initialize=false: 仅从NVS刷新值到已有菜单项（保存后回读时）
void Handset::loadGlobalSettings(bool initialize) {
    if (initialize) { handsetSettingsMenu.clearParams(); }
    int idx = 0; 

    // 索引0固定为菜单标题栏
    addInfo(idx, idx, "Radio Settings", "");
    idx++;

    // 以只读模式打开 NVS "settings" 命名空间
    configPrefs.begin("settings", true);

    // --- 独立设置项 ---
    
    // 摇杆模式 (Mode1-4)，默认 Mode2
    globalSettings.channelMode = configPrefs.getUChar("Stick Mode", 2);
    addChoiceParam(idx, 0, "Stick Mode", {"","Mode 1","Mode 2","Mode 3","Mode 4"}, globalSettings.channelMode, 1, 4);
    idx++;

    // ADC 滤波器开关，默认关闭
    globalSettings.adcFilter = configPrefs.getUChar("ADC Filter", 0);
    addChoiceParam(idx, 0, "ADC Filter", {"Off","On"}, globalSettings.adcFilter, 0, 1);
    idx++;

    // 摇杆死区，默认0，范围0-200，步长10
    globalSettings.deadband = configPrefs.getUShort("Deadband", 0);
    addNumericParam(idx, 0, CRSF_UINT16, "Deadband", globalSettings.deadband, 0, 200, 10, " / 2048");
    idx++;

    // 长按判定时间(ms)，默认AUX_LONG_PRESS宏定义值
    globalSettings.auxLongPress = configPrefs.getUShort("Long Press", AUX_LONG_PRESS);
    addNumericParam(idx, 0, CRSF_UINT16, "Long Press", globalSettings.auxLongPress, 300, 5000, 100, "ms");
    idx++;

    // 电池电压校准系数，默认VOLTAGE_SCALE宏定义值(454)
    globalSettings.voltageScale = configPrefs.getUShort("Volt Scale", VOLTAGE_SCALE);
    addNumericParam(idx, 0, CRSF_UINT16, "Volt Scale", globalSettings.voltageScale, 100, 2000, 5, "");
    idx++;

    // --- 四通道动态子菜单构建 ---
    // 每个通道生成一个文件夹，内含反向/行程/曲线等子项
    const char* folderLabels[4] = {"Ail/Roll", "Ele/Pitch", "Throttle", "Rud/Yaw"};
    const char* shortLabels[4]  = {"Ail", "Ele", "Thr", "Yaw"};
    char keyBuf[15]; // NVS键名缓冲（NVS键名最长15字符）
    
    for (uint8_t ch = 0; ch < 4; ch++) {
        int currentFolderID = idx; // 记录当前通道文件夹的索引作为父级ID

        addFolder(idx, 0, folderLabels[ch]);
        idx++;

        // 油门通道专属设置：中点居中开关
        if (ch == THR) {
            globalSettings.throttleCentered = configPrefs.getBool("Center Throt", false);
            addChoiceParam(idx, currentFolderID, "Center Throt", {"No","Yes"}, globalSettings.throttleCentered, 0, 1);
            idx++;
        }

        // 通道反向
        snprintf(keyBuf, sizeof(keyBuf), "%s Reverse", shortLabels[ch]);
        globalSettings.reverse[ch] = configPrefs.getBool(keyBuf, false);
        addChoiceParam(idx, currentFolderID, keyBuf, {"Normal","Reverse"}, globalSettings.reverse[ch], 0, 1);
        idx++;

        // 通道行程限制(%)，默认100%
        snprintf(keyBuf, sizeof(keyBuf), "%s Limit", shortLabels[ch]);
        globalSettings.limit[ch] = configPrefs.getUChar(keyBuf, 100);
        addNumericParam(idx, currentFolderID, CRSF_UINT8, keyBuf, globalSettings.limit[ch], 10, 100, 10, "%");
        idx++;

        // 通道指数曲线(Expo)，默认0%线性
        snprintf(keyBuf, sizeof(keyBuf), "%s Expo", shortLabels[ch]);
        globalSettings.expo[ch] = configPrefs.getUChar(keyBuf, 0);
        addNumericParam(idx, currentFolderID, CRSF_UINT8, keyBuf, globalSettings.expo[ch], 0, 100, 5, "%");
        idx++;    

        // 预留：后续可为所有通道添加更多数组属性设置
        // 注意：组合后的标签名必须 < 15字符（NVS键名限制）
    }

    configPrefs.end(); // 关闭NVS命名空间释放资源
}

// ==================== 保存当前编辑的设置到NVS ====================
void Handset::saveGlobalSettings() {
    // 以读写模式打开 NVS
    configPrefs.begin("settings", false);

    // 根据当前参数的数据类型选择对应的NVS写入方法
    switch (handsetSettingsMenu.getCurrentParam().type) {
        case CRSF_UINT8:
        case CRSF_INT8:
        case CRSF_TEXT_SELECTION:
            // ⚠️ 使用参数标签(label)作为NVS键名
            configPrefs.putUChar(handsetSettingsMenu.getCurrentParam().label, handsetSettingsMenu.editValue);
            loadGlobalSettings(false); // 回读到内存并刷新菜单显示值
            break;
        case CRSF_UINT16:
        case CRSF_INT16:
            configPrefs.putUShort(handsetSettingsMenu.getCurrentParam().label, handsetSettingsMenu.editValue);
            loadGlobalSettings(false);
            break;
        default:
            break; // 不支持的类型静默忽略
    }
    configPrefs.end();
}

// ==================== 执行本地设置命令（预留接口）====================
void Handset::executeSettingsCommand() {
    switch (handsetSettingsMenu.selectedParam) {
        case 99:
            // TODO: 执行命令99（例如：恢复出厂设置）
            break;
    }
}

// 取消本地设置命令（预留接口）
void Handset::cancelSettingsCommand() {
    // TODO: 取消操作逻辑
}

// ==================== 菜单项构建辅助函数 ====================
// 所有函数均通过 getParamSlot(idx) 检查边界安全性

// 添加文件夹类型菜单项
bool Handset::addFolder(int idx, int parent, const char* label) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_FOLDER;
        strcpy(handsetSettingsMenu[idx].label, label);
        return true;
    }
    else return false;
}

// 添加只读信息字符串菜单项
bool Handset::addInfo(int idx, int parent, const char* label, const char* valueString) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_INFO;
        strcpy(handsetSettingsMenu[idx].label, label);
        strcpy(handsetSettingsMenu[idx].valueString, valueString);
        return true;
    }
    else return false;
}

// 添加下拉选择型菜单项
// choices: 选项文本向量 | currVal: 当前选中索引 | min/max: 可选范围
bool Handset::addChoiceParam(int idx, int parent, const char* label, 
                              const std::vector<const char*>& choices, 
                              int currVal, int min, int max) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = CRSF_TEXT_SELECTION;
        strcpy(handsetSettingsMenu[idx].label, label);
        // 逐个复制选项文本到参数结构体
        for (int i = 0; i < choices.size(); i++) {            
            strcpy(handsetSettingsMenu[idx].choices[i].text, choices[i]);
        }
        handsetSettingsMenu[idx].minVal = min;
        handsetSettingsMenu[idx].maxVal = max;
        handsetSettingsMenu[idx].choicesCount = (max - min) + 1;
        handsetSettingsMenu[idx].currentVal = currVal;        
        return true;
    }
    else return false;
}

// 添加数值型菜单项（支持UINT8/UINT16/INT8/INT16）
bool Handset::addNumericParam(int idx, int parent, crsfValueType type, const char* label, 
                               int currVal, int min, int max, int step, const char* units) {
    if (handsetSettingsMenu.getParamSlot(idx) != -1) {
        handsetSettingsMenu[idx].parentFolder = parent;
        handsetSettingsMenu[idx].type = type;
        strcpy(handsetSettingsMenu[idx].label, label);
        handsetSettingsMenu[idx].minVal = min;
        handsetSettingsMenu[idx].maxVal = max;
        handsetSettingsMenu[idx].step = step;
        strcpy(handsetSettingsMenu[idx].units, units);   
        handsetSettingsMenu[idx].currentVal = currVal;     
        return true;
    }
    else return false;
}