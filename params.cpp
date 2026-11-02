/*
 * 本文件属于 ESP32C3-TX 项目
 * (GPLv3 License Header Omitted for Brevity)
 */

#include "params.h"

// ==================== 集合管理函数 ====================

// 清空所有参数并重置计数器
void ParamCollection::clearParams() {
    for (int i = 0; i < paramCount; i++) {
        params[i].id = 0;
        params[i].parentFolder = 0;
        params[i].type = static_cast<crsfValueType>(0);
        params[i].hidden = false;
        params[i].currentVal = 0;
        memset(params[i].label, 0, sizeof(params[i].label)); 
        // 仅清除已使用的选项槽位，避免不必要的内存操作
        for (int j = 0; j < params[i].choicesCount; j++) {
            memset(params[i].choices[j].text, 0, sizeof(params[i].choices[j].text)); 
        }
        params[i].choicesCount = 0;
    }
    paramCount = 0;
}

// 获取或分配参数槽位
// 若 ID 已存在则返回现有索引；若为新 ID 则分配新槽位并初始化
// 返回: 内部数组索引，或 -1（超出容量）
int ParamCollection::getParamSlot(uint8_t id) {
    // 1. 线性搜索是否已存在该 ID（支持幂等调用）
    for (int i = 0; i < paramCount; i++) {
        if (params[i].id == id) return i;
    }
    
    // 2. 分配新槽位
    if (paramCount < CRSF_MAX_PARAMS) {
        int freshSlot = paramCount;
        params[freshSlot].id = id;
        // 零初始化所有字段，防止残留数据导致 UI 异常
        params[freshSlot].parentFolder = 0;
        params[freshSlot].type = static_cast<crsfValueType>(0);
        params[freshSlot].hidden = false;
        params[freshSlot].currentVal = 0;
        params[freshSlot].minVal = 0;
        params[freshSlot].maxVal = 0;
        params[freshSlot].precision = 0;
        params[freshSlot].step = 0;
        params[freshSlot].timeout = 0;
        memset(params[freshSlot].label, 0, sizeof(params[freshSlot].label));
        memset(params[freshSlot].valueString, 0, sizeof(params[freshSlot].valueString));
        memset(params[freshSlot].units, 0, sizeof(params[freshSlot].units));
        memset(params[freshSlot].choices, 0, sizeof(params[freshSlot].choices));
        params[freshSlot].choicesCount = 0;
        paramCount++;
        return freshSlot;
    } 
    else {
      Serial.println("Exceeded max parameter count. Cannot allocate free slot.");
    }
    return -1; // 超出安全跟踪内存空间
}

// ==================== 菜单导航驱动函数 ====================
// 更新 selectedParam 并返回导航方向指示（供 UI 动画使用）
// 返回值: 0=无移动, 1=向上滚动, 2=向下滚动, 3=向左滑入, 4=向右滑出

// 同文件夹内下一项（循环到首项）
uint8_t ParamCollection::nextInFolder() {
    uint8_t parentFolder = params[selectedParam].parentFolder;

    // 向后搜索同级可见项
    for (int8_t i = selectedParam + 1; i < paramCount; i++) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 1; // 向下移动 → 内容向上滚动
        }
    }
    // 到达末尾未找到 → 循环回到文件夹开头
    for (int8_t i = 0; i < selectedParam; i++) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 2; // 循环跳转 → 内容向下滚动
        }
    }
    return 0; // 未找到（异常情况），不更新选中项
}

// 同文件夹内上一项（循环到末项）
uint8_t ParamCollection::prevInFolder() {
    uint8_t parentFolder = params[selectedParam].parentFolder;

    // 向前搜索同级可见项
    for (int8_t i = selectedParam - 1; i >= 0; i--) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 2; // 向上移动 → 内容向下滚动
        }
    }
    // 到达开头未找到 → 循环回到文件夹末尾
    for (int8_t i = paramCount - 1; i > selectedParam ; i--) {
        if (params[i].parentFolder == parentFolder && !params[i].hidden) {
            selectedParam = i;
            return 1; // 循环跳转 → 内容向上滚动
        }
    }
    return 0; // 未找到（异常情况）
}

// 进入子文件夹（仅当当前项为 FOLDER 类型时生效）
uint8_t ParamCollection::enterFolder() {
    if (params[selectedParam].type != CRSF_FOLDER) return 0;
    // 假设子项紧跟在父文件夹之后排列
    for (uint8_t i = selectedParam + 1; i < paramCount; i++) {
        if (params[i].parentFolder == selectedParam && !params[i].hidden) {
            selectedParam = i;
            return 3; // 进入子级 → 内容向左滑入
        }
    }
    return 0; // 空文件夹或未找到
}

// 退出当前文件夹
uint8_t ParamCollection::exitFolder() {
    if (params[selectedParam].parentFolder == 0) {
        // 已在根目录：按左键回到菜单顶部
        if (selectedParam > 0) {
            selectedParam = 0;
            return 1;
        } else {
            return 0; // 已在顶部，无操作
        }
    } else {
        // 跳转到父文件夹节点
        selectedParam = params[selectedParam].parentFolder;
        return 4;  // 返回上级 → 内容向右滑出
    }
}

// ==================== 安全访问器 ====================

const crsfParameter& ParamCollection::getCurrentParam() const {
    return getParam(selectedParam);
}

// 带越界保护的参数获取
// 越界时返回静态占位对象而非崩溃，显示 "Loading..."
const crsfParameter& ParamCollection::getParam(const uint8_t index) const {
    if (index >= 0 && index < paramCount) {
        return params[index]; // 直接返回数组元素引用
    } else {
        static crsfParameter notFound;
        notFound.id = 0;
        notFound.parentFolder = 0;
        notFound.type = CRSF_INFO;
        strcpy(notFound.label, "Loading...");
        return notFound;
    }
}

// 按标签名前缀搜索参数
// ⚠️ 注意：使用 strncmp 进行前缀匹配，非精确匹配
// 例如搜索 "Packet" 会匹配 "Packet Rate"
int ParamCollection::findParamByLabel(const char searchString[]) const {
    for (uint8_t i = 1; i < paramCount; i++) { // 从1开始，跳过索引0的标题项
        if (strncmp(params[i].label, searchString, strlen(searchString)) == 0 ) {
            return i;
        }
    }
    return -1; // 未找到
}

// ==================== 参数编辑函数 ====================
// 修改 editValue 并返回动画方向
// 返回值: 0=值未变(触达边界), 1=上, 2=下, 3=左, 4=右

uint8_t ParamCollection::editParamDec() { 
    uint8_t direction = 0;
    int32_t prevValue = editValue;
    switch(params[selectedParam].type) {
        case CRSF_UINT8: case CRSF_INT8:
        case CRSF_UINT16: case CRSF_INT16:
        case CRSF_UINT32: case CRSF_INT32:
        case CRSF_UINT64: case CRSF_INT64:
        case CRSF_FLOAT:
            // 数值型：按 step 递减，step<=1 时最小步长为1
            editValue -= params[selectedParam].step > 1 ? params[selectedParam].step : 1;
            direction = 2; // 数值减小 → 向下箭头动画
            if (editValue < params[selectedParam].minVal) 
                editValue = params[selectedParam].minVal; // 下限钳位
            break;
        case CRSF_TEXT_SELECTION:
            editValue--;
            direction = 3; // 选项左移 → 向左箭头动画
            if (editValue < params[selectedParam].minVal) 
                editValue = params[selectedParam].minVal;
            break;
        case CRSF_STRING:
            // TODO: 字符串编辑暂未实现
            break;
        default:
            break;   
    }
    // 若值未变化（触达边界），返回0禁止无效动画
    direction = (prevValue == editValue) ? 0 : direction;
    return direction;
}

uint8_t ParamCollection::editParamInc() {
    uint8_t direction = 0;
    int32_t prevValue = editValue;
    switch(params[selectedParam].type) {
        case CRSF_UINT8: case CRSF_INT8:
        case CRSF_UINT16: case CRSF_INT16:
        case CRSF_UINT32: case CRSF_INT32:
        case CRSF_UINT64: case CRSF_INT64:
        case CRSF_FLOAT:
            editValue += params[selectedParam].step > 1 ? params[selectedParam].step : 1;
            direction = 1; // 数值增大 → 向上箭头动画
            if (editValue > params[selectedParam].maxVal) 
                editValue = params[selectedParam].maxVal; // 上限钳位
            break;
        case CRSF_TEXT_SELECTION:
            editValue++;
            direction = 4; // 选项右移 → 向右箭头动画
            if (editValue > params[selectedParam].maxVal) 
                editValue = params[selectedParam].maxVal;
            break;
        case CRSF_STRING:
            // TODO: 字符串编辑暂未实现
            break;
        default:
            break;   
    }
    direction = (prevValue == editValue) ? 0 : direction;
    return direction;
}

// 进入编辑模式：将当前生效值复制到临时编辑缓冲区
void ParamCollection::editParam() {
    editValue = params[selectedParam].currentVal;
}