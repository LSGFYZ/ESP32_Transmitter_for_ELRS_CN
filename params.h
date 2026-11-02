/*
 * 本文件属于 ESP32C3-TX 项目
 * (GPLv3 License Header Omitted for Brevity)
 */
 
#ifndef PARAMS_H
#define PARAMS_H

#include <Arduino.h>

// ==================== CRSF 参数系统常量定义 ====================
#define CRSF_MAX_PARAMS                 32   // 最大参数数量（受限于 ELRS 协议单帧载荷）
#define CRSF_MAX_STRING_LEN             32   // 标签/单位/选项文本最大长度
#define CRSF_MAX_PARAM_DATA_LEN         256  // 值字符串缓冲区大小（用于 INFO/COMMAND 类型）

// ==================== ELRS 命令执行状态机步骤 ====================
// 用于 CRSF_COMMAND 类型参数的 step 字段，跟踪命令生命周期
#define ELRS_COMMAND_IDLE               0x00 // 空闲：命令未激活
#define ELRS_COMMAND_CLICK              0x01 // 点击：用户触发了命令
#define ELRS_COMMAND_EXECUTING          0x02 // 执行中：等待高频头响应
#define ELRS_COMMAND_ASKCONFIRM         0x03 // 请求确认：需要用户二次确认
#define ELRS_COMMAND_CONFIRMED          0x04 // 已确认：用户确认执行
#define ELRS_COMMAND_CANCEL             0x05 // 已取消：用户取消操作
#define ELRS_COMMAND_QUERY              0x06 // 查询：主动查询命令状态

// ==================== 选项结构体（TEXT_SELECTION 类型的可选项）====================
struct crsfOption {
    char text[CRSF_MAX_STRING_LEN]; // 选项显示文本（如 "Off", "On", "250Hz"）
};

// ==================== CRSF 参数值类型枚举 ====================
// 与 ELRS/OpenTX CRSF 协议规范完全对齐
typedef enum
{
    CRSF_UINT8 = 0,           // 无符号8位整数
    CRSF_INT8 = 1,            // 有符号8位整数
    CRSF_UINT16 = 2,          // 无符号16位整数
    CRSF_INT16 = 3,           // 有符号16位整数
    CRSF_UINT32 = 4,          // 无符号32位整数
    CRSF_INT32 = 5,           // 有符号32位整数
    CRSF_UINT64 = 6,          // 无符号64位整数
    CRSF_INT64 = 7,           // 有符号64位整数
    CRSF_FLOAT = 8,           // 浮点数（通过 precision 字段确定小数位）
    CRSF_TEXT_SELECTION = 9,  // 下拉选择（选项存储在 choices[] 数组）
    CRSF_STRING = 10,         // 字符串输入
    CRSF_FOLDER = 11,         // 文件夹（菜单分组容器）
    CRSF_INFO = 12,           // 只读信息文本
    CRSF_COMMAND = 13,        // 可执行命令（带状态机）
    CRSF_VTX = 15,            // VTX 图传专用参数
    CRSF_OUT_OF_RANGE = 127,  // 越界/无效标记
} crsfValueType;

// ==================== 单个参数节点结构体 ====================
// 统一表示所有类型的菜单项，兼容 ELRS Lua 参数和本地手台设置
struct crsfParameter {
    uint8_t id;                       // 参数索引号（从0开始递增）
    uint8_t parentFolder;             // 父级文件夹 ID（0 = 根目录）
    crsfValueType type;               // 参数数据类型
    bool hidden;                      // 是否隐藏（从 ELRS 协议 type 字段高位提取）
    int32_t currentVal;               // 当前生效值（选择型为选项索引，数值型为实际值）
    int32_t minVal;                   // 最小值（数值型范围下限 / STRING 最小长度）
    int32_t maxVal;                   // 最大值（数值型范围上限 / STRING 最大长度）
    int32_t precision;                // 小数精度（仅 FLOAT 类型有效，表示10的幂次除数）
    uint8_t step;                     // 调整步长（数值型）/ 命令状态（COMMAND 类型）
    uint32_t timeout;                 // 命令超时时间(ms)（仅 COMMAND 类型）
   
    char label[CRSF_MAX_STRING_LEN];       // 参数标签名（同时作为 NVS 存储键名）
    char valueString[CRSF_MAX_PARAM_DATA_LEN]; // 值字符串（INFO/COMMAND 的描述文本）
    char units[CRSF_MAX_STRING_LEN];       // 单位后缀（如 "ms", "%", "dBm"）
    crsfOption choices[CRSF_MAX_PARAMS];   // 可选项数组（TEXT_SELECTION 类型使用）
    uint8_t choicesCount;                  // 已成功解析的选项数量
};

// ==================== 参数集合管理类 ====================
// 封装参数数组，提供导航、编辑、查找等高级操作
class ParamCollection {
public:
    // --- 公共属性 ---
    uint8_t paramCount = 0;      // 当前已加载的参数总数
    uint8_t selectedParam = 0;   // 当前光标选中的参数索引
    int32_t editValue = 0;       // 编辑模式下的临时值（确认前不写入 currentVal）

    // 检查当前选中项是否为活跃的命令弹窗
    bool popupActive() const { 
        return (params[selectedParam].type == CRSF_COMMAND && 
                params[selectedParam].step > ELRS_COMMAND_IDLE); 
    }

    // --- 运算符重载：支持数组式访问 ---
    crsfParameter& operator[](int index) {
        return params[index]; 
    }
    const crsfParameter& operator[](int index) const {
        return params[index];
    }

    // --- 查找 ---
    // 按标签名搜索参数，返回索引或 -1
    int findParamByLabel(const char searchString[]) const;

    // --- 菜单导航（返回动画方向 0-4，0=无移动）---
    uint8_t nextInFolder();   // 同文件夹内下一项
    uint8_t prevInFolder();   // 同文件夹内上一项
    uint8_t enterFolder();    // 进入子文件夹
    uint8_t exitFolder();     // 返回上级文件夹

    // --- 参数编辑（返回动画方向 0-4，0=无变化）---
    uint8_t editParamDec();   // 减小编辑值
    uint8_t editParamInc();   // 增大编辑值
    void    editParam();      // 将 currentVal 复制到 editValue（进入编辑模式时调用）

    // --- 安全访问器 ---
    const crsfParameter& getCurrentParam() const;    // 获取当前选中参数
    const crsfParameter& getParam(const uint8_t index) const; // 按索引安全获取参数

    // --- 内存管理 ---
    int getParamSlot(uint8_t id);   // 分配新参数槽位，返回内部索引或 -1
    void clearParams();             // 清空所有参数

private:
    crsfParameter params[CRSF_MAX_PARAMS]; // 固定大小参数数组（避免动态分配）
};

#endif // PARAMS_H