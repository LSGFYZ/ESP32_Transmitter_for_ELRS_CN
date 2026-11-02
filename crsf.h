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

/*
 =======================================================================================================
 * CRSF 协议说明
 *
 * CRSF 协议使用单线半双工 UART 连接。
 * 主设备（手台）每 4ms 发送一帧，从设备（高频头）在两帧之间回复。
 *
 * 波特率: 420000 baud
 * 电平: 非反向 (not inverted)
 * 数据位: 8 Bit
 * 停止位: 1 Stop bit
 * 字节序: 大端模式 (Big endian)
 * ELRS 支持多种波特率: 115k, 400k, 921k, 1.87M, 3.75M
 * 115000 bit/s = 14400 byte/s
 * 420000 bit/s = 46667 byte/s (含停止位) = 每字节 21.43us
 * 最大帧大小: 64 字节
 * 64 字节帧 + 1 同步字节可在 1393 微秒内发送完毕
 *
 * CRSF_TIME_NEEDED_PER_FRAME_US 保守设置为 1500 微秒
 *
 * 每帧结构: <设备地址><帧长度><类型><载荷><CRC>
 *
 * 设备地址: (uint8_t)
 * 帧长度:   包含类型字段的字节数 (uint8_t)
 * 类型:     (uint8_t)
 * CRC:      (uint8_t)
 *
 */
//#define CRSFDEBUG // [调试] 取消注释启用 CRSF 协议调试输出

#define ELRS_PORT Serial0 // ELRS 模块使用的硬件串口（ESP32C3 的 Serial0）

// ==================== 基础配置 ====================
#define CRSF_MAX_CHANNEL        16      // CRSF 协议最大支持通道数
#define CRSF_FRAME_SIZE_MAX     64      // 单帧最大字节数
// 定义数字通道的输入限制（CRSF 标准范围）
#define CRSF_DIGITAL_CHANNEL_MIN 172    // 通道最小值（对应约 -100%）
#define CRSF_DIGITAL_CHANNEL_MAX 1811   // 通道最大值（对应约 +100%）

// ==================== CRSF 内部变量 ====================
#define CRSF_TIME_NEEDED_PER_FRAME_US   1100 // 每帧所需时间(us): 700ms发送 + 400ms预留临时请求
//#define SERIAL_BAUDRATE                 115200 // [备用] Arduino Nano 低波特率，TX模块会自动检测
//#define CRSF_TIME_BETWEEN_FRAMES_US     4000   // [备用] 4ms = 250Hz 刷新率
#define SERIAL_BAUDRATE                 921600 // 当前使用波特率: 921600
#define CRSF_TIME_BETWEEN_FRAMES_US     2000   // 当前帧间隔: 2000us = 500Hz 刷新率
#define CRSF_PAYLOAD_OFFSET             offsetof(crsfFrameDef_t, type) // 载荷在帧结构体中的偏移量
#define CRSF_MAX_PACKET_SIZE            64      // 最大包大小
#define CRSF_QUEUE_SIZE                 8       // 环形队列大小，必须为2的幂(2,4,8,16)以便用位运算快速取模
#define CRSF_MSP_RX_BUF_SIZE            128     // MSP 接收缓冲区大小
#define CRSF_MSP_TX_BUF_SIZE            128     // MSP 发送缓冲区大小
#define CRSF_PAYLOAD_SIZE_MAX           60      // 最大载荷大小（64 - 地址 - 长度 - 类型 - CRC）
#define CRSF_PACKET_LENGTH              22      // 标准通道数据包长度
#define CRSF_PACKET_SIZE                26      // 标准通道数据包总大小（含帧头帧尾）
#define CRSF_FRAME_LENGTH               24      // 帧长度字段值 = 类型 + 载荷 + CRC
#define CRSF_CMD_PACKET_SIZE            8       // 命令包大小
#define CRSF_TLM_PACKET_SIZE            10      // 遥测包大小
#define CRSF_MAX_PARAMS                 32      // ELRS 最大参数数量
#define CRSF_MAX_STRING_LEN             32      // 参数字符串最大长度
#define CRSF_MAX_PARAM_DATA_LEN         256     // 参数数据最大长度
#define CRSF_SUBTYPE_OPENTX_SYNC        0x10    // OpenTX 同步子类型标识

// ==================== ELRS (CRSF) 帧类型定义 ====================
#define ELRS_CHANNELS                   0x16    // 通道数据包
#define ELRS_LINK_STATS                 0x14    // 链路统计信息（RSSI/LQ/SNR等）
#define ELRS_DEVICE_PING                0x28    // 设备发现/心跳探测
#define ELRS_DEVICE_INFO                0x29    // 设备信息响应
#define ELRS_SETTINGS_RESPONSE          0x2B    // 设置读取响应（Lua菜单数据）
#define ELRS_SETTINGS_READ              0x2C    // 设置读取请求
#define ELRS_HEARTBEAT                  0x2D    // 心跳包
#define ELRS_STATUS                     0x2E    // 状态信息
#define ELRS_RADIO_ID                   0x3A    // 遥控器ID标识

// ==================== 设备地址定义 ====================
#define HANDSET_ADDRESS                 0xEA    // 手台（本机）地址
#define MODULE_ADDRESS                  0xEE    // ELRS 高频头模块地址
#define LUA_SCRIPT_ADDRESS              0xEF    // Lua 脚本地址
#define BROADCAST_ADDRESS               0x00    // 广播地址

// ==================== ELRS 命令码（适用于 Jumper Aion ELRS 3.6.0）====================
#define ELRS_PKT_RATE_COMMAND           0x01    // 设置数据包速率
#define ELRS_TLM_RATIO_COMMAND          0x02    // 设置遥测比率
#define ELRS_SWITCH_MODE_COMMAND        0x03    // 设置开关模式（Wide/Hybrid）
#define ELRS_MODEL_MATCH_COMMAND        0x05    // 设置模型匹配
#define ELRS_POWER_COMMAND              0x07    // 设置发射功率
#define ELRS_DYNAMIC_POWER_COMMAND      0x08    // 设置动态功率
#define ELRS_WIFI_COMMAND               0x10    // 启用高频头 WiFi
#define ELRS_RXWIFI_COMMAND             0x11    // 启用接收机 WiFi
#define ELRS_BLE_JOYSTIC_COMMAND        0x12    // 启用 BLE 蓝牙手柄（ESP8266 不支持）
#define ELRS_BIND_COMMAND               0x13    // 进入对频模式
#define ELRS_START_COMMAND              0x04    // 参数写入开始标记
#define ELRS_END_COMMAND                0x05    // 参数写入结束标记
#define ELRS_SETTINGS_WRITE             0x2D    // 设置写入命令
#define SYNC_BYTE                       0xC8    // CRSF 帧同步字节

// ELRS 模块串口声明（外部引用）
extern HardwareSerial ELRS_PORT;

// ==================== CRSF 原始数据包结构体 ====================
struct crsfPacket {
    uint8_t length;                         // 数据包有效长度
    uint8_t data[CRSF_MAX_PACKET_SIZE];     // 数据包内容缓冲区
};

// ==================== CRSF 数据包环形队列类 ====================
// 用于缓冲待发送的命令和接收到的遥测数据，避免丢包
class crsfPacketQueue {
private:
    crsfPacket _buffer[CRSF_QUEUE_SIZE];    // 环形缓冲区数组
    volatile uint8_t _head;                 // 写指针：指向下一个写入位置
    volatile uint8_t _tail;                 // 读指针：指向下一个读取位置
    // 掩码辅助常量：利用位与运算替代取模(%)实现超快环形索引回绕
    static const uint8_t _mask = CRSF_QUEUE_SIZE - 1;

public:
    crsfPacketQueue() : _head(0), _tail(0) {} // 构造函数：初始化读写指针为0

    // 检查队列是否有待处理数据
    bool hasItems() {return _head != _tail;}

    // 将原始数据包推入队列（生产者调用）
    bool push(const uint8_t* rawData, uint8_t len) {
        if (len > CRSF_MAX_PACKET_SIZE) return false; // 安全检查：防止溢出
        uint8_t nextHead = (_head + 1) & _mask;       // 计算下一个写位置（位与取模）
        if (nextHead == _tail) {
            return false; // 队列已满，丢弃新数据
        }
        _buffer[_head].length = len;                  // 记录包长度
        memcpy(_buffer[_head].data, rawData, len);    // 复制包数据
        _head = nextHead;                             // 原子性推进写指针
        return true;
    }

    // 从队列取出下一个数据包（消费者调用）
    bool pop(crsfPacket& outPacket) {
        if (_head == _tail) {
            return false; // 队列为空
        }
        outPacket = _buffer[_tail];                   // 拷贝数据到输出参数
        _tail = (_tail + 1) & _mask;                  // 原子性推进读指针
        return true;
    }

    // 清空队列：在切换蓝牙模式时调用，防止旧命令堆积
    void clear() {
       noInterrupts();                                // 关中断，保证原子操作
       _head = 0;
       _tail = 0;
       interrupts();                                  // 开中断
    }    
};

// ==================== CRSF 链路统计信息结构体 ====================
// 存储从 ELRS 高频头回传的实时链路质量数据
struct crsfLinkStats {
  int16_t uplinkRssi1;     // 天线1 上行RSSI（已转换为负dBm值）
  int16_t uplinkRssi2;     // 天线2 上行RSSI（已转换为负dBm值）
  uint8_t uplinkLQ;        // 上行链路质量 (0-100%)
  int8_t  uplinkSNR;       // 上行信噪比 (dB)
  uint8_t activeAntenna;   // 当前活跃天线: 0=天线1, 1=天线2
  uint8_t rfMode;          // ExpressLRS 空中速率索引
  uint8_t txPower;         // 模块发射功率等级索引
  int16_t downlinkRssi;    // 下行遥测RSSI（已转换为负dBm值）
  uint8_t downlinkLQ;      // 下行遥测链路质量 (0-100%)
  int8_t  downlinkSNR;     // 下行遥测信噪比 (dB)
};

// ==================== CRSF 协议主类 ====================
class CRSF {
public:
   // ---- 公开属性 ----
   crsfLinkStats linkStats;             // 链路统计数据实例
   bool moduleConnected = false;        // ELRS 模块连接状态标志
   bool telemetryActive = false;        // 遥测数据是否正在接收
   uint32_t lastValidFrameTime = 0;     // 最后一帧有效数据的时间戳(ms)
   float aircraftVoltage = 0.0f;        // 飞机电池电压：CRSF 电池传感器帧，单位 V
   bool aircraftBatteryValid = false;   // 是否收到过有效的飞机电池电压帧
   uint32_t lastAircraftBatteryTime = 0;// 最后一次收到飞机电池电压的时间(ms)
   crsfPacketQueue commandQueue;        // 出站命令队列（手台→高频头）
   crsfPacketQueue telemetryQueue;      // 入站遥测队列（高频头→手台）

   // ---- 公开方法 ----
   void begin(uint8_t rxPin, uint8_t txPin, bool halfDuplex); // 初始化串口和引脚
   void crsfSendChannels(int16_t channels[]);                  // 发送16通道数据帧
   void crsfSendCommand(uint8_t command, uint8_t value);       // 将命令加入发送队列
   void crsfQueuePacket(uint8_t packet[], uint8_t packetLength); // 将原始包加入队列
   void crsfSendQueuedCommand();                               // 从队列取出并发送一条命令
   void crsfReadTelemetry();                                   // 读取串口数据并解析入队
   uint32_t crsfNextInterval();                                // 获取下一帧发送间隔(us)

private:
   // ---- 私有属性 ----
   bool isHalfDuplex = false;           // 是否半双工模式标志
   
   // 同步相位追踪变量（用于精确对齐 ELRS 的帧时序）
   volatile uint32_t targetIntervalUs;         // 目标帧间隔(us)
   volatile int32_t currentPhaseShift;         // 当前相位偏移
   volatile int32_t averagePhaseShift;         // 平均相位偏移
   volatile int32_t cumulativePhaseShift = 0;  // 累积相位偏移
   int16_t phaseShiftHistory[256];             // 相位偏移历史环形缓冲
   uint8_t syncPacketCount = 0;                // 已收到的同步包计数
   volatile bool syncPacketReceived;           // 同步包接收标志
   uint32_t lastSyncPacketTime = 0;            // 最后同步包时间戳

   uint32_t lastLinkStatsFrameTime = 0;        // 最后链路统计帧时间
   uint32_t lastLinkStatRequestTime = 0;       // 最后链路统计请求时间
   uint32_t lastSyncPacketDisplay = 0;         // 最后同步包显示更新时间

   // ---- 私有方法 ----
   void crsfWritePacket(uint8_t packet[], uint8_t packetLength);   // 底层串口写入
   void crsfParseLinkStatsPacket(uint8_t rxBuffer[]);              // 解析链路统计包
   void crsfParseElrsSyncPacket(uint8_t rxBuffer[]);               // 解析ELRS同步包
};

// 声明全局 CRSF 类实例，供后台串口中断处理函数访问
extern CRSF crsfClass;


/* 
=====================================================================================
 以下为 LUA 调试输出示例及 ELRS 参数树文档（非代码，仅供开发参考）
=====================================================================================

[LUA调试日志示例]
[ELRS] -> 查询菜单参数索引: 0, 数据块: 0
收到数据包类型 0x2B (SETTINGS_RESPONSE)
[参数已保存] HooJ | 菜单 (ID: 0)
...（中间省略重复格式的参数查询日志）...
[参数已保存] Packet Rate | 选项(0-9): 25Hz~F1000 | 当前选中: 500Hz(-105dBm)
[参数已保存] Telem Ratio | 选项: Std/Race等 | 当前选中: 1:32 (546bps)
[参数已保存] Switch Mode | 选项: Wide/Hybrid | 当前选中: Wide
[参数已保存] Link Mode | 选项: Normal/MAVLink | 当前选中: Normal
[参数已保存] Model Match | 选项: Off/On | 当前选中: Off
[参数已保存] TX Power (100mW Dyn) | 子菜单 (ID: 6)
[参数已保存] Max Power | 选项: 10~500mW | 当前选中: 100mW
[参数已保存] Dynamic | 选项: Off/Dyn/AUX9~12 | 当前选中: Dyn
[参数已保存] VTX Administrator | 子菜单 (ID: 9)
[参数已保存] Band/Channel/Pwr Lvl/Pitmode | 图传相关参数
[参数已保存] Send VTx | 命令 (状态: IDLE)
[参数已保存] WiFi Connectivity | 子菜单 (ID: 15)
[参数已保存] Enable WiFi / Enable Rx WiFi | WiFi控制命令
[参数已保存] BLE Joystick | 蓝牙手柄命令
[参数已保存] Bind | 对频命令
[参数已保存] Bad/Good | 信息: 0/126
[参数已保存] 3.6.0 ISM2G4 | 固件版本信息: ff41f6
[ELRS] >>> 参数树矩阵已完全填充并缓存 <<<


=====================================================================================
ESP32 Team900 命令速查表
来源: https://github.com/danxdz/simpleTx_esp32/blob/master/src/Simple_TX.cpp
=====================================================================================
// buildElrsPacket(crsfCmdPacket, X, 3);
// 0 : ELRS 状态请求 => ??
// 1 : 设置 [Packet Rate] = 0:25Hz / 1:50Hz / 2:100Hz / 3:200Hz
// 2 : 设置 [Telem Ratio] = 0:off / 1:1:128 / 2:1:64 / 3:1:32 / 4:1:16 / 5:1:8 / 6:1:4 / 7:1:2
// 3 : 设置 [Switch Mode] = 0:Hybrid / 1:Wide
// 4 : 设置 [Model Match] = 0:Off / 1:On
// 5 : 设置 [TX Power] = 0:10mW / 1:25mW
// 6 : 设置 [Max Power] = 0:10mW / 1:25mW (*不强制更改，重启后生效)
// 7 : 设置 [Dynamic] = 0:Off / 1:On / AUX9~AUX12
// 8 : 设置 [VTX Administrator] = 0
// 9 : 设置 [Band] = 0:Off / A/B/E/F/R/L
// 10: 设置 [Channel] = 0~7 对应 1~8
// 11: 设置 [Pwr Lvl] = 0:- / 1~8
// 12: 设置 [Pitmode] = 0:Off / 1:On
// 13: 设置 [Send VTx] = 0 (发送响应 chunk=0 step=2)
// 14: 设置 [WiFi Connectivity] = 0
// 15: 设置 [Enable WiFi] = 0 (发送响应 chunk=0 step=0)
// 16: 设置 [Enable Rx WiFi] = 0 (发送响应 chunk=0 step=2)
// 17: 设置 [BLE Joystick]:
//     =0 发送响应 chunk=0 step=0
//     =1 发送响应 chunk=0 step=3
//     =2 发送响应 chunk=0 step=3
//     =3 发送响应 chunk=0 step=3
//     =4 启用BLE手柄: 停止hwTimer → 设TX Power=0 → hwTimer间隔5000 → 调整最大包大小22 → 启动BLE
// 19: 设置 [Bad/Good] = 0
// 20: 获取固件版本 [2.1.0 EU868] = 0/1


=====================================================================================
ELRS 参数树矩阵（原始数据格式）
格式: 索引:名称:父级ID:类型:最小:最大:默认;选项1:选项2:... :: 类型标记
=====================================================================================
0:Packet Rate:0:9:0:4:4;25Hz(-123dBm):50Hz(-120dBm):100Hz(-117dBm):100Hz Full(-112dBm):200Hz(-112dBm): :: OPT
1:Telem Ratio:0:9:0:9:0;Std:Off:1:128:1:64:1:32:1:16:1:8:1:4:1:2:Race: :: OPT
2:Switch Mode:0:9:0:1:0;Wide:Hybrid: :: OPT
3:Model Match:0:9:0:1:0;Off:On: :: OPT
4:TX Power (50mW):0:11:0:0:0 :: MainMenuItem 
5:Max Power:5:9:0:2:2;10:25:50: :: OPT
6:Dynamic:5:9:0:5:0;Off:Dyn:AUX9:AUX10:AUX11:AUX12: :: OPT
7:VTX Administrator:0:11:0:0:0 :: MainMenuItem
8:Band:8:9:0:6:0;Off:A:B:E:F:R:L: :: OPT
9:Channel:8:9:0:7:0;1:2:3:4:5:6:7:8: :: OPT
10:Pwr Lvl:8:9:0:8:0;-:1:2:3:4:5:6:7:8: :: OPT
11:Pitmode:8:9:0:8:79;Off:On:AUX1↑:AUX1↓:AUX2↑:AUX2↓:AUX3↑:AUX3↓:AUX4↑:AUX4↓: Pitmode: :: OPT
12:Send VTx:8:13:0:0:0 :: CMD
13:WiFi Connectivity:0:11:0:0:0 :: MainMenuItem
14:Enable WiFi:14:13:0:0:0 :: CMD
15:Enable Rx WiFi:14:13:0:0:0 :: CMD
16:BLE Joystick:0:13:0:0:0 :: CMD
17:Bind:0:13:0:0:0 :: CMD
18:Bad/Good:0:12:0:0:0 :: INFO
19:3.1.2 EU868:0:12:0:0:0 :: INFO


=====================================================================================
ELRS OLED 菜单树结构（可视化表示）
=====================================================================================
Main Menu
├── Packet Rate (OPT)
│   ├── 25Hz(-123dBm)
│   ├── 50Hz(-120dBm)
│   ├── 100Hz(-117dBm)
│   ├── 100Hz Full(-112dBm)
│   └── 200Hz(-112dBm)
├── Telem Ratio (OPT)
│   ├── Std / Off / 1:128 / 1:64 / 1:32 / 1:16 / 1:8 / 1:4 / 1:2 / Race
├── Switch Mode (OPT)
│   ├── Wide / Hybrid
├── Model Match (OPT)
│   ├── Off / On
├── TX Power (50mW)
│   ├── Max Power (OPT): 10 / 25 / 50
│   └── Dynamic (OPT): Off / Dyn / AUX9 / AUX10 / AUX11 / AUX12
├── VTX Administrator
│   ├── Band (OPT): Off / A / B / E / F / R / L
│   ├── Channel (OPT): 1-8
│   ├── Pwr Lvl (OPT): - / 1-8
│   └── Pitmode (OPT): Off / On / AUX1↑↓ ... AUX4↑↓
├── Send VTx (CMD)
├── WiFi Connectivity
│   ├── Enable WiFi (CMD)
│   └── Enable Rx WiFi (CMD)
├── BLE Joystick (CMD)
├── Bind (CMD)
├── Bad/Good (INFO)
└── 3.1.2 EU868 (INFO)
*/