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
 
// ==================== 硬件配置（引脚定义与串口选择）====================

// --- ELRS 高频头串口配置 ---
// #define ELRS_PORT Serial1  // 串口号已在 crsf.h 中硬编码为 Serial0，此处仅作备注
// 取消下面这行的注释以启用半双工模式（TX/RX 共用一根线）
// 默认使用全双工模式（TX 和 RX 独立接线）
//#define ELRS_HALF_DUPLEX
#define ELRS_TX   21    // ELRS 串口 TX 引脚 (GPIO21)
#define ELRS_RX   20    // ELRS 串口 RX 引脚 (GPIO20)

// --- 摇杆模拟输入引脚 ---
const uint8_t STICK_RIGHT_X  = 0;     // 右摇杆 X 轴 (ADC0)
const uint8_t STICK_RIGHT_Y = 1;      // 右摇杆 Y 轴 (ADC1)
const uint8_t STICK_LEFT_Y = 3;       // 左摇杆 Y 轴 (ADC3)
const uint8_t STICK_LEFT_X   = 4;     // 左摇杆 X 轴 (ADC4)
const uint8_t ANALOG_PIN_VOLTAGE  = 2;// 电池电压检测引脚 (ADC2)

// --- 开关与按键数字输入引脚 ---
const uint8_t DIGITAL_PIN_SWITCH_ARM =  7;  // 解锁(Arm)开关 (GPIO7)
const uint8_t DIGITAL_PIN_SWITCH_AUX2 = 10; // AUX2 辅助开关 (GPIO10)
const uint8_t DIGITAL_PIN_BUTTON = 9;       // 功能按键/编码器按键 (GPIO9)

// --- 输出设备引脚 ---
const uint8_t DIGITAL_PIN_LED =    8;      // 状态 LED 引脚 (GPIO8)
const bool PIN_LED_LOW_ILLUM  =    true;   // LED 低电平点亮标志
                                            // true = 低电平亮(共阳接法), false = 高电平亮(共阴接法)

// --- OLED 显示屏 I2C 引脚 ---
const uint8_t I2C_SCL_OLED = 6;       // OLED I2C 时钟线 (GPIO6)
const uint8_t I2C_SDA_OLED = 5;       // OLED I2C 数据线 (GPIO5)