#pragma once
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
#include <stdint.h>
#include <string.h>
#include "params.h"   // 引入参数集合类，用于存储 ELRS Lua 菜单树

class CRSF;           // 前向声明：避免循环引用（ELRS 依赖 CRSF，CRSF 也关联 ELRS）

// #define ELRSDEBUG    // [调试] 取消注释启用 ELRS 协议调试输出

// ==================== UI 标签常量（OLED 菜单显示用）====================
#define LABEL_PACKET_RATE               "Packet Rate"     // 数据包速率
#define LABEL_TX_POWER_STRING           "TX Power"        // 发射功率
#define LABEL_MAX_POWER                 "Max Power"       // 最大功率
#define LABEL_DYNAMIC_POWER             "Dynamic"         // 动态功率
#define LABEL_BIND_TX                   "Bind"            // 对频
#define LABEL_ENABLE_WIFI               "Enable WiFi"     // 启用 WiFi

// ==================== 输入阈值常量 ====================
#define STICK_POS_HIGH                  3000  // 摇杆高位阈值（用于菜单导航确认）
#define STICK_POS_LOW                   1000  // 摇杆低位阈值
#define BUTTON_PRESS_DURATION           100   // 按键按下持续时间判定(ms)

// ==================== CRSF 包大小常量（与 crsf.h 保持一致）====================
#define CRSF_MAX_PACKET_SIZE            64    // 最大包大小
#define CRSF_PAYLOAD_SIZE_MAX           60    // 最大载荷大小
#define CRSF_PACKET_LENGTH              22    // 通道数据包长度
#define CRSF_PACKET_SIZE                26    // 通道数据包总大小
#define CRSF_FRAME_LENGTH               24    // 帧长度 = type + payload + crc
#define CRSF_CMD_PACKET_SIZE            8     // 命令包大小
#define CRSF_TLM_PACKET_SIZE            10    // 遥测包大小
#define CRSF_MAX_STRING_LEN             32    // 字符串最大长度
#define CRSF_SUBTYPE_OPENTX_SYNC        0x10  // OpenTX 同步子类型
#define CRSF_LOAD_QUEUE_MAX_ITEMS       64    // 参数加载队列最大容量（预期约21-22个参数，留余量）

// ==================== ELRS (CRSF) 帧类型定义 ====================
#define ELRS_CHANNELS                   0x16  // 通道数据
#define ELRS_LINK_STATS                 0x14  // 链路统计
#define ELRS_DEVICE_PING                0x28  // 设备探测
#define ELRS_DEVICE_INFO                0x29  // 设备信息响应
#define ELRS_SETTINGS_RESPONSE          0x2B  // 设置读取响应（Lua 参数数据）
#define ELRS_SETTINGS_READ              0x2C  // 设置读取请求
#define ELRS_HEARTBEAT                  0x2D  // 心跳
#define ELRS_STATUS                     0x2E  // ELRS 状态信息
#define ELRS_RADIO_ID                   0x3A  // 遥控器 ID / 同步包

// ==================== ELRS 命令执行步骤状态机 ====================
// 用于跟踪命令类参数（如 Bind、WiFi）的执行进度
#define ELRS_COMMAND_IDLE               0x00  // 空闲，等待触发
#define ELRS_COMMAND_CLICK              0x01  // 用户点击了命令
#define ELRS_COMMAND_EXECUTING          0x02  // 命令正在执行中
#define ELRS_COMMAND_ASKCONFIRM         0x03  // 等待用户二次确认
#define ELRS_COMMAND_CONFIRMED          0x04  // 用户已确认
#define ELRS_COMMAND_CANCEL             0x05  // 用户取消了命令
#define ELRS_COMMAND_QUERY              0x06  // 查询命令当前状态

// ==================== 设备地址定义 ====================
#define HANDSET_ADDRESS                 0xEA  // 手台地址
#define MODULE_ADDRESS                  0xEE  // ELRS 高频头地址
#define LUA_SCRIPT_ADDRESS              0xEF  // Lua 脚本地址
#define BROADCAST_ADDRESS               0x00  // 广播地址
#define SYNC_BYTE                       0xC8  // CRSF 同步字节


// ==================== LIFO 栈：参数加载任务队列 ====================
// 用于跟踪待查询的参数索引，后进先出确保子菜单优先加载
struct LoadStack {
    uint8_t items[CRSF_LOAD_QUEUE_MAX_ITEMS]; // 栈数组
    int8_t top = -1;                          // 栈顶指针，-1 表示空栈

    // 压栈：将参数索引加入待加载队列
    bool push(uint8_t paramIndex) {
        if (top >= CRSF_LOAD_QUEUE_MAX_ITEMS - 1 || paramIndex < 0) return false;
        // 去重检查：避免同一参数被重复加入队列
        for (int i = 0; i <= top; i++) {
            if (items[i] == paramIndex) return true; // 已存在则直接返回成功
        }
        items[++top] = paramIndex; // 先递增再赋值
        return true;
    }

    // 弹栈：取出最近加入的参数索引
    int16_t pop() {
        if (top < 0) return -1; // 栈空返回 -1
        return items[top--];    // 先取值再递减
    }

    // 检查栈是否为空
    bool isEmpty() {
        return top == -1;
    }

    // 清空栈
    void clear() {
        top = -1;
    }
};


// ==================== ELRS 运行状态结构体 ====================
// 存储从 ELRS_STATUS (0x2E) 包解析出的实时状态
struct crsfElrsStatus {
    uint8_t  packetsBad;                        // 坏包计数
    uint16_t packetsGood;                       // 好包计数
    bool     isConnected;                       // 接收机是否已连接
    bool     modelMismatch;                     // 模型匹配失败标志
    bool     isArmed;                           // 飞行器是否已解锁
    char     statusMessage[CRSF_MAX_STRING_LEN];// 状态消息文本
};

// ==================== ELRS 模块信息结构体 ====================
// 存储从 DEVICE_INFO (0x29) 包解析出的模块硬件/固件信息及完整参数树
struct crsfModule {
    char name[CRSF_MAX_STRING_LEN];             // 模块名称（如 "HappyModel EP2"）
    uint8_t serialNumber[4];                    // 序列号
    uint8_t hwVersion[4];                       // 硬件版本号
    uint8_t swVersion[4];                       // 固件版本号
    uint8_t paramCount;                         // 参数总数
    uint8_t protocolVersion;                    // CRSF 协议版本
    bool paramsLoaded;                          // 参数树是否已完整加载

    ParamCollection params;                     // 参数集合对象：存储完整的 Lua 菜单树
};


// ==================== ELRS 高级交互类 ====================
class ELRS {
public:
    // ---- 公开属性 ----
    // 检查 ELRS 是否就绪（参数加载完成且连接正常）
    bool ready() const { return connectionState == ELRS_READY; }
    // 检查是否有弹窗消息需要显示（如命令执行反馈）
    bool popup() const { return txModule.params.popupActive(); }
    bool showMenu = false;                      // OLED 菜单显示开关
    crsfModule txModule;                        // 高频头模块信息实例
    crsfElrsStatus elrsStatus;                  // ELRS 运行状态实例

    // ---- 公开方法 ----
    ELRS(CRSF& crsfInstance);                   // 构造函数：传入 CRSF 实例引用以访问底层通信
    void update(bool menuActive);               // 主驱动函数：在 loop() 中周期调用，驱动状态机
    void editParamSave();                       // 保存当前编辑中的参数值到高频头
    void executeCommand();                      // 执行或确认当前选中的命令参数
    void cancelCommand();                       // 取消当前正在执行的命令

private:
    // ---- 私有属性 ----
    CRSF& crsf;                                 // CRSF 底层通信实例引用
    
    // ELRS 连接状态机枚举
    enum crsfConnectState {
        ELRS_BOOT_DELAY,                        // 开机延迟：等待模块上电稳定
        ELRS_PINGING,                           // 探测阶段：发送 PING 等待设备响应
        ELRS_LOAD_PARAMS,                       // 参数加载阶段：逐个查询并缓存 Lua 菜单
        ELRS_READY                              // 就绪：参数加载完成，可正常交互
    };
    crsfConnectState connectionState = ELRS_BOOT_DELAY; // 当前连接状态
    
    LoadStack loadQueue;                        // 参数加载任务栈
    int16_t activeExpectedParamIndex = -1;      // 当前正在等待响应的参数索引
    bool moduleInfoReceived = false;            // 是否已收到设备信息包
    uint32_t lastHandshakeTime = 0;             // 最后握手时间戳
    uint32_t lastParameterQueryTime = 0;        // 最后参数查询时间戳
    uint32_t nextPopupPollingTime = 0;          // 下次弹窗轮询时间（命令执行期间定期查状态）
    bool parameterDiscoveryActive = false;      // 参数发现过程是否正在进行
    int16_t currentSettingsIndex = -1;          // 当前正在处理的设置索引
    uint8_t currentChunk = 0;                   // 当前分块传输的块编号
    uint8_t chunksRemaining = 0;                // 剩余未接收的数据块数
    uint8_t settingAttemptsCounter = 0;         // 设置写入重试计数器
    uint8_t totalSettingsCount;                 // 参数总数

    uint32_t lastLinkStatsFrameTime = 0;        // 最后链路统计帧时间
    uint32_t lastLinkStatRequestTime = 0;       // 最后链路统计请求时间
    uint32_t lastSyncPacketDisplay = 0;         // 最后同步包显示更新时间

    // ---- 私有方法 ----
    void sendCommand(uint8_t command, uint8_t value);   // 通过 CRSF 层发送命令
    void pingDevices();                                 // 发送设备探测包
    void requestElrsStatus();                           // 请求 ELRS 运行状态
    void requestSetting(uint8_t settingIndex, uint8_t chunk); // 请求指定参数的指定数据块

    // 各类响应包解析函数
    void parseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length);    // 解析设备信息 (0x29)
    void parseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length);    // 解析 ELRS 状态 (0x2E)
    void parseParameterInfoPacket(uint8_t rxBuffer[], uint8_t length); // 解析参数元信息
    void parseParameter(uint8_t rxBuffer[], uint8_t length);           // 解析参数数据（支持分块重组）

    void loadAllParams(uint8_t totalCount);     // 启动全量参数加载流程
    void loadOneParam(uint8_t paramIndex);      // 加载单个参数
    void clearModule();                         // 清除模块信息和参数缓存（断连时调用）
};

/*
=====================================================================================
ELRS 3.6.0 参数树参考表（非代码，仅供开发对照）
格式: 父级ID - 索引 - 标签 | 类型及选项
=====================================================================================
Parent   Index    Label
0     -  0     -  HooJ  | Menu (ID: 0)                          ← 根菜单
0     -  1     -  Packet Rate | Choices(0-9): 50Hz~F1000        ← 数据包速率选择
0     -  2     -  Telem Ratio | Choices(0-9): Std~Race          ← 遥测比率
0     -  3     -  Switch Mode | Choices(0-1): Wide, Hybrid      ← 开关模式
0     -  4     -  Link Mode | Choices(0-1): Normal, MAVLink     ← 链路模式
0     -  5     -  Model Match | Choices(0-1): Off, On           ← 模型匹配
0     -  6     -  TX Power (100mW Dyn) | Menu (ID: 6)           ← 功率子菜单
6     -  7     -  Max Power | Choices(0-5): 10~500mW            ← 最大功率
6     -  8     -  Dynamic | Choices(0-5): Off/Dyn/AUX9~12       ← 动态功率
0     -  9     -  VTX Administrator | Menu (ID: 9)              ← 图传管理子菜单
9     -  10    -  Band | Choices(0-6): Off/A/B/E/F/R/L          ← 图传频段
9     -  11    -  Channel (1-8): 1                               ← 图传频道
9     -  12    -  Pwr Lvl | Choices(0-8): -/1~8                 ← 图传功率等级
9     -  13    -  Pitmode | Choices(0-21): Off/On/AUX↑↓...      ← 维修模式
9     -  14    -  Send VTx | Command (state IDLE)               ← 发送图传设置命令
0     -  15    -  WiFi Connectivity | Menu (ID: 15)             ← WiFi 子菜单
15    -  16    -  Enable WiFi | Command (state IDLE)            ← 启用高频头 WiFi
15    -  17    -  Enable Rx WiFi | Command (state IDLE)         ← 启用接收机 WiFi
0     -  18    -  BLE Joystick | Command (state IDLE)           ← 蓝牙手柄
0     -  19    -  Bind | Command (state IDLE)                   ← 对频
0     -  20    -  Bad/Good | Info String: 0/126                 ← 链路质量信息
0     -  21    -  3.6.0 ISM | Info String                       ← 固件版本信息
*/