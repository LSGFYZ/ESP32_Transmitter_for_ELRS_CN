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
 
#include "params.h" // 引入参数管理库，用于菜单和设置存储

//#define USE_M7 // [可选] 启用 M7 GPS 模块支持（当前已注释禁用）

/*
 =======================================================================================================
 简易发射机配置选项（注释掉不需要的选项即可关闭对应功能）
 =======================================================================================================
 */
// 定义摇杆通道的硬编码反向（取消注释即反向该通道）- 也可在手台设置中动态配置
#define RIGHTX_REVERSE      // 右横滚（副翼）通道反向
//#define RIGHTY_REVERSE    // 右俯仰（升降）通道反向
#define LEFTX_REVERSE       // 左横滚（方向舵）通道反向
//#define LEFTY_REVERSE     // 左俯仰（油门）通道反向

// 有源蜂鸣器只能发出单一音调（常用于飞控作为提示音）
// 也可以替换为振动马达
//#define ACTIVE_BUZZER     // [可选] 启用有源蜂鸣器/振动马达

//----- 电压监测相关配置 -------------------------
// 定义电池警告电压阈值
#define VOLTAGE_SCALE 454       // ADC 电压缩放系数（用于将 ADC 原始值转换为实际电压）
#define WARNING_VOLTAGE 3.7     // 1S 锂电池低压警告阈值：每节 3.7V
#define BEEPING_VOLTAGE 3.5     // 1S 锂电池持续蜂鸣报警阈值：每节 3.5V
#define ON_USB          1.0     // USB 供电识别阈值：低于此电压视为无电池/USB 供电状态

#define AUX_LONG_PRESS  1000    // AUX3 长按触发时间（毫秒），默认 1 秒，可在手台设置中修改
#define BTN_LONG_PRESS  1500    // 内置按键长按判定时间（毫秒），硬编码不可通过菜单修改
#define BTN_SHORT_PRESS 100     // 内置按键/摇杆按键消抖时间（毫秒），硬编码

// 定义开机校准/指令识别的摇杆位置范围
// 约 50% 摇杆偏转量对应的 ADC 值区间
#define RC_MIN_COMMAND 511      // 指令识别最小 ADC 值
#define RC_MAX_COMMAND 1535     // 指令识别最大 ADC 值

// 定义摇杆静止超时报警时间
#define STICK_ALARM_TIME 300000 // 300000ms = 300秒 = 5分钟，摇杆无操作超过此时间触发报警

// 定义模拟输入范围限制
// ESP32 ADC 理论支持 12 位分辨率 (0-4095)，但实际可用精度仅约一半
#define ADC_MIN 0               // ADC 最小有效值
#define ADC_MID 1023            // ADC 中点值（摇杆居中位置）
#define ADC_MAX 2047            // ADC 最大有效值

// IIR 低通滤波器系数：数值越大滤波越平滑，但延迟越高
// 值为 2 时（右移2位）= 25% 新采样 + 75% 历史值
// 值为 3 时（右移3位，即除以8）= 12.5% 新采样 + 87.5% 历史值
#define FILTER_SHIFT 3          // 当前滤波移位量：3（较平滑，适合大多数场景）

// 通道顺序枚举：定义 CRSF 协议输出的通道映射
enum ChannelOrder {
    AIL,                        // CH1: 副翼 (Aileron)
    ELE,                        // CH2: 升降 (Elevator)
    THR,                        // CH3: 油门 (Throttle)
    RUD,                        // CH4: 方向舵 (Rudder)
    AUX1,                       // CH5: ExpressLRS 解锁开关 (ARM)
    AUX2,                       // CH6: 自稳/空中模式切换 (Angle/AirMode)
    AUX3,                       // CH7: 炸机后翻转恢复 (Flip After Crash)
    AUX4,                       // CH8: 自定义辅助通道
    AUX5,                       // CH9: 自定义辅助通道
    AUX6,                       // CH10: 自定义辅助通道
    AUX7,                       // CH11: 自定义辅助通道
    AUX8,                       // CH12: 自定义辅助通道
};

// 物理输入源枚举：对应硬件上的摇杆和拨动开关
enum stickInputs {
    LEFTX,                      // 左摇杆 X 轴（通常为方向舵）
    LEFTY,                      // 左摇杆 Y 轴（通常为油门）
    RIGHTX,                     // 右摇杆 X 轴（通常为副翼）
    RIGHTY,                     // 右摇杆 Y 轴（通常为升降）
    SW1,                        // 拨动开关 1
    SW2,                        // 拨动开关 2
    SW3,                        // 拨动开关 3
};

// 手台全局设置结构体：存储所有可调节的用户参数
struct HandsetSettings {
    uint8_t channelMode;        // 遥控器通道模式（如美国手/日本手等）
    bool adcFilter;             // ADC 滤波开关：true=启用 IIR 滤波，false=直通原始值
    uint16_t deadband;          // 摇杆死区大小：消除中心抖动
    uint16_t auxLongPress;      // AUX 长按时间（毫秒），可覆盖默认值
    uint16_t voltageScale;      // 电压缩放系数，适配不同分压电阻
    bool throttleCentered;      // 油门是否居中：true=双向油门（如车模），false=单向油门（飞行器）

    bool reverse[4];            // 四主通道反向标志：索引对应 A/E/T/R，true=反向
    uint8_t limit[4];           // 四主通道行程限制：0-100%，索引对应 A/E/T/R
    uint8_t expo[4];            // 四主通道指数曲线：0-100%，索引对应 A/E/T/R，改善手感线性度
};

// Handset 类：封装手台设置的加载、保存与菜单构建逻辑
class Handset {
public:
    HandsetSettings globalSettings;         // 全局设置实例，存储当前生效的所有参数
    ParamCollection handsetSettingsMenu;    // 参数集合对象，用于构建 OLED 菜单树

    // 加载全局设置
    // initialize=true: 从头重建菜单并加载默认值（首次使用或恢复出厂时调用）
    // initialize=false: 仅更新已有菜单项的值（正常启动时调用）
    void loadGlobalSettings(bool initialize);

    // 保存当前选中的菜单项到 EEPROM/NVS 持久化存储
    void saveGlobalSettings();

    // 执行当前选中的命令（如重置所有设置）- 当前版本未实现
    void executeSettingsCommand();

    // 取消当前正在执行的命令 - 当前版本未实现
    void cancelSettingsCommand();

private:
    // 向菜单添加文件夹节点
    // idx: 菜单项索引 | parent: 父级索引 | label: 显示名称
    bool addFolder(int idx, int parent, const char* label);

    // 向菜单添加只读信息项
    // valueString: 显示的静态文本内容
    bool addInfo(int idx, int parent, const char* label, const char* valueString);

    // 向菜单添加多选参数项（下拉选择框）
    // choices: 选项字符串数组 | currVal/min/max: 当前值及范围
    bool addChoiceParam(int idx, int parent, const char* label, 
                        const std::vector<const char*>& choices, 
                        int currVal, int min, int max);

    // 向菜单添加数值调节参数项（带步进和单位）
    // type: CRSF 值类型 | step: 调节步长 | units: 单位字符串（如 "%", "mW"）
    bool addNumericParam(int idx, int parent, crsfValueType type, const char* label, 
                         int currVal, int min, int max, int step, const char* units);
};

// =====================================================================================
// 以下为外部 ELRS 模块响应数据的示例注释（非代码，仅供调试参考）
// 展示了通过 CRSF 协议查询/设置 ELRS 高频头参数时的数据包格式
// =====================================================================================

// [CRSF] -> 查询菜单参数索引: 1, 数据块: 4
// 收到数据包类型 0x2B (PARAMETER_SETTINGS_ENTRY)
// [参数已保存] Packet Rate | 选项(0-9): 50Hz(-115dBm), 100Hz Full(-112dBm), 
//   150Hz(-112dBm), 250Hz(-108dBm), 333Hz Full(-105dBm), 500Hz(-105dBm), 
//   D250(-104dBm), D500(-104dBm), F500(-104dBm), F1000(-104dBm) 
//   | 当前选中: 333Hz Full(-105dBm)

// [CRSF] -> 查询菜单参数索引: 2, 数据块: 1
// [参数已保存] Telem Ratio | 选项: Std, Off, 1:128, 1:64, 1:32, 1:16, 1:8, 1:4, 1:2, Race
//   | 当前选中: 1:8 (3219bps)

// [CRSF] -> 查询菜单参数索引: 3, 数据块: 1
// [参数已保存] Switch Mode | 选项: 8ch, 16ch Rate/2, 12ch Mixed | 当前选中: 8ch

// [CRSF] -> 查询菜单参数索引: 4, 数据块: 0
// [参数已保存] Model Match | 选项: Off, On | 当前选中: Off (ID: 0)

// [CRSF] -> 查询菜单参数索引: 5, 数据块: 0
// [参数已保存] TX Power (100mW Dyn) | 子菜单 (ID: 5)

// [CRSF] -> 查询菜单参数索引: 6, 数据块: 0
// [参数已保存] Max Power | 选项: 10, 25, 50, 100, 250, 500 mW | 当前选中: 100mW

// [CRSF] -> 查询菜单参数索引: 7, 数据块: 1
// [参数已保存] Dynamic | 选项: Off, Dyn, AUX9-AUX12 | 当前选中: Dyn

// [CRSF] -> 查询菜单参数索引: 8, 数据块: 0
// [参数已保存] VTX Administrator | 子菜单 (ID: 8)

// [CRSF] -> 查询菜单参数索引: 9, 数据块: 0
// [参数已保存] Band | 选项: Off, A, B, E, F, R, L | 当前选中: Off

// [CRSF] -> 查询菜单参数索引: 10, 数据块: 0
// [参数已保存] Channel | 选项: 1-8 | 当前选中: 1

// [CRSF] -> 查询菜单参数索引: 11, 数据块: 0
// [参数已保存] Pwr Lvl | 选项: -, 1-8 | 当前选中: -

// [CRSF] -> 查询菜单参数索引: 12, 数据块: 3
// [参数已保存] Pitmode | 选项: Off, On, AUX1↑↓...AUX10↑↓ | 当前选中: Off

// [CRSF] -> 查询菜单参数索引: 13, 数据块: 0
// [参数已保存] Send VTx | 命令 (状态: IDLE | 超时: 2000ms)

// [CRSF] -> 查询菜单参数索引: 14, 数据块: 0
// [参数已保存] WiFi Connectivity | 子菜单 (ID: 14)

// [CRSF] -> 查询菜单参数索引: 15, 数据块: 0
// [参数已保存] Enable WiFi | 命令 (状态: IDLE | 超时: 2000ms)