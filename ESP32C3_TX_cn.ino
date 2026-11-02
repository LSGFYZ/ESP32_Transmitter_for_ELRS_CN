
/*
// Simple ESP32 C3 transmitter
// ESP32 C3 0.42" OLED
// ELRS 2.4G NANO RX as a TX module
// Based on SimpleTX by kkbin505
// https://github.com/kkbin505/Arduino-Transmitter-for-ELRS

 * This file is part of ESP32C3-TX
 *
 * ESP32C3-TX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ESP32C3-TX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <http://www.gnu.org/licenses/>.
 */
// ARM（AUX1/CH5） 输入模式：二选一
//#define ARM_MODE_BUTTON   // ===== 按键模式：短按切换 ARM =====
#define ARM_MODE_SLIDE // ===== 两档滑动开关模式：低电平 ARM，高电平 DISARM =====

// =================================
// 额外功能: BLE蓝牙手柄模式、OLED菜单设置、电池电压监测
// 功能: 摇杆输入 -> CRSF协议 -> ELRS发射模块 -> 接收机控制模型
// 使用 ESP32C3 + 0.96寸 OLED + ELRS 2.4G 模块
// 基于 SimpleTX by kkbin505 修改
// ===== ESP32C3 遥控器主程序 =====
#include <Arduino.h>
#include <Preferences.h>
#include <BleGamepad.h>  //安装 LemmingDev 开发的 bleGamepad 库
#include "hardware.h"
#include "config.h"
#include "crsf.h"
#include "elrs.h"
#include "oled.h"

//#define DEBUG // if not commented out, Serial.print() is active! For debugging only!!
//#define TIMINGDEBUG  // When this is active Serial port will print out timing details

//----- Handset class contains settings and menu ------
Handset transmitter;

//----- Bluetooth joystick functionality --------------
BleGamepad bleGamepad("NanoTX Gamepad", "NanoTX", 100); // Set 100% battery to start with - gets updated below;
bool btJoystickModeActive = false; // Track whether bt joystick mode is selected or not
bool bleGamepadBegun = false;      // Track whether we need to start bleGamepad


// 索引: RIGHTX=0, RIGHTY=1, LEFTY=2, LEFTX=3, SW1=4, SW2=5, SW3=6
// stickValues[7]: 存储4个摇杆轴 + 3个辅助开关的值
//----- 全局输入变量 ---------------------------------
//----- global variables for inputs -------------------
int16_t stickValues[7];  // 4 axis of joysticks + 3 aux inputs


// ----- 原始ADC值结构体: 存储摇杆和电池电压的原始读取值 -----
struct ADCValues {
    int32_t leftX;
    int32_t leftY;
    int32_t rightX;
    int32_t rightY;
    int32_t voltage;
};
ADCValues rawValues;


// ----- 滤波状态结构体: 存储上一周期的滤波结果，用于IIR滤波 -----
struct FilterState {
    int32_t rightX;
    int32_t rightY;
    int32_t leftY;
    int32_t leftX;
};
FilterState filteredRaw;

float batteryVoltage;

bool ARMED = false;

// 0 = No move/button
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
// 5 = Joystick button
// 6 = Built in button (short press)
// 7 = Built in button (long press)

// 导航操作码: 0=无操作, 1=上, 2=下, 3=左, 4=右, 5=摇杆按键, 6=板载按键(短按), 7=板载按键(长按)
uint8_t navigate = 0;


// ----- 函数声明 ------------------------------------
//----- function headers ----------------------------------
inline int32_t lowLatencyFilter(int32_t currentRaw, int32_t *previousFiltered);
int16_t calibrate(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal);
int32_t applyCyclicExpo(int32_t stickDiff, uint8_t expoFactor);
int32_t applyThrottleExpo(int32_t adcVal, uint8_t expoFactor);
uint16_t processStick(int32_t adcValue, uint8_t chIndex);
void mapChannels();
void getAnalogInputs();
void getDigitalInputs();
void checkGestureSetting();
bool checkStickMove();
void playTone(uint8_t timesPerSecond);
void blinkLED(int ledPin, uint16_t blinkRate);
void stickMenuNavigation();
void statusDisplay();
void logData();


// elrsClass: ELRS模块控制类实例
// crsfClass: CRSF协议处理类实例
// rcChannels[]: 存储各通道映射后的CRSF值
// ----- ELRS通信相关全局变量 --------------------------
//----- global variables for ELRS -------------------------
int16_t rcChannels[CRSF_MAX_CHANNEL];
CRSF crsfClass;
// Instantiate ELRS (Module) class, passing CRSF driver object to it
ELRS elrsClass(crsfClass);

// -----------------------------------------------------------------------------------------------------

// ===== 校准相关函数 =====
// Calibration functions
// -----------------------------------------------------------------------------------------------------
void calibrationLoad();
void calibrationSave();
void calibrationChirp(uint8_t times);
bool calibrationRun();


// CALIB_MOVE_TMO: 摇杆满行程校准等待时间(ms)
// CALIB_CENT_TMO: 摇杆居中校准等待时间(ms)
// ESP32 NVS存储偏好设置(用于保存校准数据)
Preferences prefs;
#define CALIB_CENT_TMO  5000    //ms
#define CALIB_MOVE_TMO  15000   // ms
bool calibrationRequested=false;
uint32_t calibrationTimerStart;
int      cal_reset = 0 ;


// 每个摇杆轴有3个校准点: 最小值(全行程一端)、中位值(居中)、最大值(全行程另一端)
// ----- 校准值结构体: 存储每个摇杆轴的最小/最大/中位值 -----
struct CalibValues {
    int rightXMin  = 0;
    int rightXMax  = 4095;
    int rightXMid  = 2048;
    int rightYMin  = 0;
    int rightYMax  = 4095;
    int rightYMid  = 2048;
    int leftXMin   = 0;
    int leftXMax   = 4095;
    int leftXMid   = 2048;
    int leftYMin   = 0;
    int leftYMax   = 4095;
    int leftYMid   = 2048;
};
CalibValues calValues;


// 将四个摇杆轴(右X/右Y/左X/左Y)的min/mid/max共12个校准值保存到Flash
// ===== 保存校准值到NVS存储 =====
void calibrationSave() {
    // Open the "gimbal" namespace in Read-Write mode (false)
    prefs.begin("gimbal", false);
    
    prefs.putShort("rightXMin",  calValues.rightXMin);
    prefs.putShort("rightXMid", calValues.rightXMid);
    prefs.putShort("rightXMax",  calValues.rightXMax);
    
    prefs.putShort("rightYMin",  calValues.rightYMin);
    prefs.putShort("rightYMid", calValues.rightYMid);
    prefs.putShort("rightYMax",  calValues.rightYMax);
    
    prefs.putShort("leftXMin",  calValues.leftXMin);
    prefs.putShort("leftXMid", calValues.leftXMid);
    prefs.putShort("leftXMax",  calValues.leftXMax);

    prefs.putShort("leftYMin",  calValues.leftYMin);
    prefs.putShort("leftYMid",  calValues.leftYMid);
    prefs.putShort("leftYMax",  calValues.leftYMax);
        
    prefs.end();
}


// 读取保存的校准值，如果无保存数据则使用默认值(0/2048/4095)
// ===== 从NVS存储加载校准值 =====
void calibrationLoad() {
    // Open the "gimbal" namespace in Read-Only mode
    prefs.begin("gimbal", true);
    
    // Read values, providing safe defaults if no data has been saved yet
    calValues.rightXMin = prefs.getShort("rightXMin", 0);
    calValues.rightXMid = prefs.getShort("rightXMid", 2048);
    calValues.rightXMax = prefs.getShort("rightXMax", 4095);
    
    calValues.rightYMin = prefs.getShort("rightYMin", 0);
    calValues.rightYMid = prefs.getShort("rightYMid", 2048);
    calValues.rightYMax = prefs.getShort("rightYMax", 4095);
    
    calValues.leftXMin  = prefs.getShort("leftXMin", 0);
    calValues.leftXMid  = prefs.getShort("leftXMid", 2048);
    calValues.leftXMax  = prefs.getShort("leftXMax", 4095);
    
    calValues.leftYMin  = prefs.getShort("leftYMin", 0);
    calValues.leftYMid  = prefs.getShort("leftYMid", 2048);
    calValues.leftYMax  = prefs.getShort("leftYMax", 4095);
    
    prefs.end();

}

//Flash LED and beep for (times)

// times: 闪烁/鸣叫次数
// ===== 校准提示: LED闪烁 + 蜂鸣器鸣叫 =====
void calibrationChirp(uint8_t times) {
    
    digitalWrite(DIGITAL_PIN_LED, HIGH);
    delay(500);
    digitalWrite(DIGITAL_PIN_LED, LOW);
    delay(500);

    for (uint8_t i = 0; i < times; i++) {
        //tone(DIGITAL_PIN_BUZZER,5000,100);
        digitalWrite(DIGITAL_PIN_LED, HIGH);
        delay(100);
        //digitalWrite(DIGITAL_PIN_BUZZER, LOW);
        digitalWrite(DIGITAL_PIN_LED, LOW);
        delay(100);
    }
    digitalWrite(DIGITAL_PIN_LED, LOW);
}


// 返回: true=校准进行中/完成, false=因ARMED状态被拒绝
// 完成后自动保存到NVS
// 阶段2(满行程): 给用户15秒时间将每个摇杆推到各个方向极限，记录min/max
// 阶段1(居中): 等待用户将四个摇杆全部推回中位，记录中位值
// 分两个阶段:
// ===== 执行校准流程 =====
bool calibrationRun() {
    if (ARMED){
        Serial.println("Cannot calibrate while arm switch is active");
        cal_reset = 0;
        calibrationRequested = false;
        delay(1000);
        return false;
    }

    // Reset variables to "centers"
    while(cal_reset<1){
    Serial.println("Starting Calibration");
    drawCalibrationScreen(1,0);  // OLED display update
    calibrationChirp(2);    // start calibration
    calibrationTimerStart = millis();
    calValues.rightXMin = 65535;
    calValues.rightXMax = 0;
    calValues.rightXMid = 0;
    calValues.rightYMin = 65535;
    calValues.rightYMax = 0;
    calValues.rightYMid = 0;
    calValues.leftXMin  = 65535;
    calValues.leftXMax  = 0;
    calValues.leftXMid  = 0;
    calValues.leftYMin  = 65535;
    calValues.leftYMax  = 0;
    calValues.leftYMid  = 0;
    cal_reset++;
    }

    uint32_t currentMillis = millis();

    if (currentMillis <= (calibrationTimerStart + CALIB_CENT_TMO)){
        // A Center
        int val = analogRead(STICK_RIGHT_X);
        calValues.rightXMid = val;
        
        // E Center
        val = analogRead(STICK_RIGHT_Y);
        calValues.rightYMid = val;

        // T Center
        val = analogRead(STICK_LEFT_Y);
        calValues.leftYMid = val;
    
        // R Center
        val = analogRead(STICK_LEFT_X);
        calValues.leftXMid = val;
 
        drawCalibrationScreen(2,((int)(calibrationTimerStart + CALIB_CENT_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Center All Sticks:   Aileron Center:");
        Serial.print(calValues.rightXMid);
        Serial.print(" Elevator Center:");
        Serial.print(calValues.rightYMid);
        Serial.print(" Throttle Center:");
        Serial.print(calValues.leftYMid);
        Serial.print(" Rudder Center:");
        Serial.print(calValues.leftXMid);
        Serial.println();

        blinkLED(DIGITAL_PIN_LED, 1000);  // Slow flash while centering
        
    }
    // 15 seconds for moving sticks
    else if (currentMillis > (calibrationTimerStart + CALIB_CENT_TMO) && currentMillis <  (calibrationTimerStart + CALIB_MOVE_TMO)){
        // A Min-Max
        int val = analogRead(STICK_RIGHT_X);
        if (val < calValues.rightXMin) {
            calValues.rightXMin = val;
        } 
        if (val > calValues.rightXMax) {
            calValues.rightXMax = val;
        }
        // E Min-Max
        val = analogRead(STICK_RIGHT_Y);
        if (val < calValues.rightYMin) {
            calValues.rightYMin = val;
        } 
        if (val > calValues.rightYMax) {
            calValues.rightYMax = val;
        }

        // T Min-Max
        val = analogRead(STICK_LEFT_Y);
        if (val < calValues.leftYMin) {
            calValues.leftYMin = val;
        } 
        if (val > calValues.leftYMax) {
            calValues.leftYMax = val;
        }

        // R Min-Max
        val = analogRead(STICK_LEFT_X);
        if (val < calValues.leftXMin) {
            calValues.leftXMin = val;
        } 
        if (val > calValues.leftXMax) {
            calValues.leftXMax = val;
        }

        drawCalibrationScreen(3,((int)(calibrationTimerStart + CALIB_MOVE_TMO - currentMillis)/1000)+1);  // OLED display update

        Serial.print("Move sticks full range: Aileron Min:");
        Serial.print(calValues.rightXMin);
        Serial.print(" Max:");
        Serial.print(calValues.rightXMax);
        Serial.print(" Elevator Min:");
        Serial.print(calValues.rightYMin);
        Serial.print(" Max:");
        Serial.print(calValues.rightYMax);
        Serial.print(" Rudder Min:");
        Serial.print(calValues.leftXMin);
        Serial.print(" Max:");
        Serial.print(calValues.leftXMax);
        Serial.print(" Throttle Min:");
        Serial.print(calValues.leftYMin);
        Serial.print(" Max:");
        Serial.print(calValues.leftYMax);
        Serial.println();

        blinkLED(DIGITAL_PIN_LED, 100);  // Fast flash while moving to limits
    }
    else {
        Serial.println("Calibration Done");  
        calibrationSave();

        drawCalibrationScreen(4,0);  // OLED display update

        cal_reset = 0;
        calibrationRequested = false;
        calibrationChirp(3);    // ok
        lastScreen = CALIBRATION;
    }
    return true;

}

// -----------------------------------------------------------------------------------------------------

// ===== 模拟量输入处理 =====
// Handle analog input
// -----------------------------------------------------------------------------------------------------

// 6. 检查摇杆是否触发菜单导航
// 5. 应用摇杆反向配置
// 4. 应用校准映射(将原始ADC值映射到标准范围)
// 3. 应用IIR低延迟滤波器平滑数据
// 2. 读取4个摇杆轴的原始ADC值(0-4095)
// 1. 读取电池电压
// 执行步骤:
// ===== 读取并处理所有模拟输入 =====
void getAnalogInputs() {
    // Read Voltage
    rawValues.voltage = analogRead(ANALOG_PIN_VOLTAGE);
    if (transmitter.globalSettings.voltageScale > 0) { // Prevent divide by zero possibility
        batteryVoltage = (float)rawValues.voltage / transmitter.globalSettings.voltageScale;
    }
    else {
        batteryVoltage = (float)rawValues.voltage / VOLTAGE_SCALE;
    }


    // 1. Read raw 12-bit ADC values (0 - 4095)
    int32_t currentRightX = analogRead(STICK_RIGHT_X);
    int32_t currentRightY = analogRead(STICK_RIGHT_Y);
    int32_t currentLeftY  = analogRead(STICK_LEFT_Y);
    int32_t currentLeftX  = analogRead(STICK_LEFT_X);
    
    // 2. Apply fast IIR filter to raw hardware values (if enabled in globalSettings)
    rawValues.rightX = lowLatencyFilter(currentRightX, &filteredRaw.rightX);
    rawValues.rightY = lowLatencyFilter(currentRightY, &filteredRaw.rightY);
    rawValues.leftY  = lowLatencyFilter(currentLeftY,  &filteredRaw.leftY);
    rawValues.leftX  = lowLatencyFilter(currentLeftX,  &filteredRaw.leftX);
    
    // 3. Apply calibration mappings
    stickValues[RIGHTX] = calibrate(rawValues.rightX, calValues.rightXMin, calValues.rightXMid, calValues.rightXMax);
    stickValues[RIGHTY] = calibrate(rawValues.rightY, calValues.rightYMin, calValues.rightYMid, calValues.rightYMax);
    stickValues[LEFTY]  = calibrate(rawValues.leftY,  calValues.leftYMin, calValues.leftYMid, calValues.leftYMax);
    stickValues[LEFTX]  = calibrate(rawValues.leftX,  calValues.leftXMin, calValues.leftXMid, calValues.leftXMax);

    // 4. Hard-coded reverse (Gimbal configuration)
    #ifdef RIGHTX_REVERSE
        stickValues[RIGHTX]  = ADC_MAX-stickValues[RIGHTX];
    #endif
    #ifdef RIGHTY_REVERSE
        stickValues[RIGHTY] = ADC_MAX-stickValues[RIGHTY];
    #endif
    #ifdef LEFTX_REVERSE
        stickValues[LEFTX]   = ADC_MAX-stickValues[LEFTX];
    #endif
    #ifdef LEFTY_REVERSE
        stickValues[LEFTY] = ADC_MAX-stickValues[LEFTY];
    #endif

    // 5. Check if the current movement qualifies for menu navigation
    stickMenuNavigation();

}


// 必须回到中心区域100ms后才能触发下一次移动(防抖)
// 摇杆推出阈值(50%行程)后触发一次方向移动
// 使用右摇杆进行菜单导航:
// ----- 摇杆菜单导航函数 -----
// Use right joystick for menu navigation
// Moving past the threshold (50% throw) will move once in that direction
// Go back below the threshold for 100ms before you can initiate another move in any direction

// - 触发后锁定，必须回到中心才能再次触发(防止按住不动连续触发)
// - 偏移消抖: 摇杆推出后需保持同一方向BTN_SHORT_PRESS时间才触发导航
// - 居中消抖: 摇杆回到中心后需保持稳定BTN_SHORT_PRESS时间才确认居中
// 防抖逻辑:
// 使用右摇杆的上下左右进行菜单导航
// ===== 摇杆菜单导航 =====
void stickMenuNavigation() {
    // Timers for filtering noise (debouncing)
    static uint32_t centerStartTime = 0;
    static uint32_t deflectionStartTime = 0;
    
    // State tracking variables
    static bool isCentered = true;       // True if joystick is confirmed at center
    static bool hasNavigated = false;    // Prevents continuous triggers while holding
    static uint8_t pendingDirection = 0; // Temporarily stores the direction being debounced

    uint32_t currentMillis = millis();
    navigate = 0; // Reset navigate variable at the start of each loop

    // Check if joystick is currently physically in the center zone
    bool stickCentered = (stickValues[RIGHTY] > RC_MIN_COMMAND && stickValues[RIGHTY] < RC_MAX_COMMAND &&
                               stickValues[RIGHTX]  > RC_MIN_COMMAND && stickValues[RIGHTX]  < RC_MAX_COMMAND);

    // Handle Center Debouncing
    if (stickCentered) {
        deflectionStartTime = 0; // Reset deflection timer
        if (centerStartTime == 0) {
            centerStartTime = currentMillis; // Start center debounce timer
        } else if (currentMillis - centerStartTime >= BTN_SHORT_PRESS) {
            isCentered = true;   // Joystick is officially centered
            hasNavigated = false; // Reset navigation lock for the next movement
        }
    } 
    // Handle Deflection Debouncing & Navigation
    else {
        centerStartTime = 0; // Reset center timer
        
        // Determine current physical direction of deflection
        uint8_t currentDirection = 0;
        if (stickValues[RIGHTY] >= RC_MAX_COMMAND)       currentDirection = 1; // Up
        else if (stickValues[RIGHTY] <= RC_MIN_COMMAND)  currentDirection = 2; // Down
        else if (stickValues[RIGHTX] <= RC_MIN_COMMAND)  currentDirection = 3; // Left
        else if (stickValues[RIGHTX] >= RC_MAX_COMMAND)  currentDirection = 4; // Right

        // If the direction changed mid-deflection, reset the deflection timer
        if (currentDirection != pendingDirection) {
            deflectionStartTime = currentMillis;
            pendingDirection = currentDirection;
        } 
        // If held in the same direction for long enough, check if we can navigate
        else if (currentMillis - deflectionStartTime >= BTN_SHORT_PRESS) {
            if (isCentered && !hasNavigated) {
                navigate = pendingDirection; // Trigger the menu action
                hasNavigated = true;         // Lock execution until center return
                isCentered = false;          // No longer marked as centered
            }
        }
    }
}

// ADC Filtering, since ESP32 ADCs are supposed to be noisy

// 返回: 滤波后的值
// 参数: currentRaw=当前原始值, previousFiltered=上一周期滤波结果指针
// FILTER_SHIFT越小滤波越强但响应越慢
// 公式: NewFiltered = PreviousFiltered + ((CurrentRaw - PreviousFiltered) >> FILTER_SHIFT)
// 用整数移位运算实现快速IIR滤波，避免浮点运算耗时
// ===== 低延迟IIR滤波器 =====
inline int32_t lowLatencyFilter(int32_t currentRaw, int32_t *previousFiltered) {
    if (transmitter.globalSettings.adcFilter && *previousFiltered > 0) {
        // IIR Filter implementation using fast integer bit-shifting:
        // NewFiltered = PreviousFiltered + ((CurrentRaw - PreviousFiltered) >> FILTER_SHIFT)
        *previousFiltered = *previousFiltered + ((currentRaw - *previousFiltered) >> FILTER_SHIFT);
    }
    else {
        *previousFiltered = currentRaw;
    }
    return *previousFiltered;
}

// Calibration maps top and bottom parts of stick deflections separately to account for uneven ADC ranges

// 返回: 校准后的16位有符号值
// 参数: raw=原始ADC值, minVal=最小值, centerVal=中位值, maxVal=最大值
// 以centerVal为分界点，上下半段分别映射(处理ADC非线性)
// 将原始ADC值按校准范围映射到标准输出范围(ADC_MIN到ADC_MAX)
// ===== 摇杆校准映射 =====
int16_t calibrate(int16_t raw, int16_t minVal, int16_t centerVal, int16_t maxVal) {
    raw = constrain(raw, minVal, maxVal);
    
    if (raw <= centerVal) {
        // Map lower half of stick deflection 
        return map(raw, minVal, centerVal, ADC_MIN, ADC_MID);
    } else {
        // Map upper half of stick deflection
        return map(raw, centerVal, maxVal, ADC_MID+1, ADC_MAX);
    }
}

// Expo function for Aileron, Elevator, Rudder (Zero-Centered: -1024 to +1024)

// 返回: 应用指数曲线后的值
// expoFactor=0表示线性无曲线, 值越大曲线越明显
// 参数: stickDiff=以中心为基准的偏移值(-1024到+1024), expoFactor=指数系数(0-100)
// 使摇杆靠近中心时灵敏度降低(精细操控)，远离中心时灵敏度升高(快速转向)
// 用于副翼(AIL)、升降(ELE)、方向(RUD)的指数曲线处理
// ===== 周期通道指数曲线(率/高) =====
int32_t applyCyclicExpo(int32_t stickDiff, uint8_t expoFactor) {
    if (expoFactor == 0) return stickDiff;
    int32_t stickCubed = (stickDiff * stickDiff * stickDiff) >> 20; 
    return ((100 - expoFactor) * stickDiff + expoFactor * stickCubed) / 100;
}

// For Throttle (Full Range: 0 to 2047)

// 返回: 应用指数曲线后的油门值
// 注意: 内部先右移1位降到0-1023范围避免32位整数溢出
// 参数: adcVal=ADC值(0-2047), expoFactor=指数系数(0-100)
// 使油门在低位时变化平缓(精细调速)，高位时变化陡峭(快速加速)
// 用于油门(THR)的指数曲线处理(从零开始曲线)
// ===== 油门通道指数曲线 =====
int32_t applyThrottleExpo(int32_t adcVal, uint8_t expoFactor) {
    if (expoFactor == 0) return adcVal;
    // Shift down to 0-1023 to avoid 32-bit integer overflow during cubing
    int32_t downscaled = adcVal >> 1; 
    int32_t cubed = (downscaled * downscaled * downscaled) >> 20; // 0 to 1023
    int32_t throttleCubed = cubed << 1; // Scale back up to 0-2047
    return ((100 - expoFactor) * adcVal + expoFactor * throttleCubed) / 100;
}


// 返回: CRSF标准通道值(172-1812)
// 参数: adcValue=ADC输入值, chIndex=通道索引
// 4. 最终钳位在CRSF数字通道范围内
// 3. 周期通道处理(死区->指数曲线->限幅->反向->映射)
// 2. 油门通道特殊处理(指数曲线+上限限制+反向+映射)
// 1. 钳位限制在ADC_MIN~ADC_MAX范围内
// 处理流程:
// 核心函数: 将原始ADC值经过各种处理后输出为CRSF协议标准值
// ===== 摇杆值处理 -> CRSF通道输出 =====
uint16_t processStick(int32_t adcValue, uint8_t chIndex) {
    if (adcValue < ADC_MIN) adcValue = ADC_MIN;
    if (adcValue > ADC_MAX) adcValue = ADC_MAX;

    int32_t crsfOutput = CRSF_DIGITAL_CHANNEL_MAX - CRSF_DIGITAL_CHANNEL_MIN >> 1; // Default to center value
    uint8_t expo = transmitter.globalSettings.expo[chIndex];
    uint8_t limit = transmitter.globalSettings.limit[chIndex];

    if (chIndex == THR && !transmitter.globalSettings.throttleCentered) {
        // Throttle-specific Expo (Curved from 0)
        adcValue = applyThrottleExpo(adcValue, expo);

        // Top-End Throttle Limit
        adcValue = (adcValue * limit) / 100;

        // User-configured reverse
        if (transmitter.globalSettings.reverse[chIndex]) {
            adcValue = 2047 - adcValue;
        }

        // Map to CRSF Range (172 to 1812)
        // Ratio: 1640 / 2048 = 0.80078125 -> approximated by (* 820) >> 10
        crsfOutput = ((adcValue * 820) >> 10) + 172;

    } else {
        int32_t stickCentered = adcValue - ADC_MID;

        // Apply Dynamic Deadband
        if (transmitter.globalSettings.deadband > 0) {
            if (abs(stickCentered) <= transmitter.globalSettings.deadband) {
                stickCentered = 0;
            } else {
                // Interpolate the stick curve outward past the deadband edge
                if (stickCentered > 0) {
                    stickCentered = ((stickCentered - transmitter.globalSettings.deadband) * 1024) / (1024 - transmitter.globalSettings.deadband);
                } else {
                    stickCentered = ((stickCentered + transmitter.globalSettings.deadband) * 1024) / (1024 - transmitter.globalSettings.deadband);
                }
            }
        }

        // Expo around center stick
        stickCentered = applyCyclicExpo(stickCentered, expo);

        // Channel limiting
        stickCentered = (stickCentered * limit) / 100;

        // User-configured reverse
        if (transmitter.globalSettings.reverse[chIndex]) {
            stickCentered = -stickCentered;
        }

        // Map to CRSF Range and center it on 992
        int32_t crsfScaled = (stickCentered * 820) >> 10;
        crsfOutput = crsfScaled + 992;
    }

    if (crsfOutput < CRSF_DIGITAL_CHANNEL_MIN) crsfOutput = CRSF_DIGITAL_CHANNEL_MIN;
    if (crsfOutput > CRSF_DIGITAL_CHANNEL_MAX) crsfOutput = CRSF_DIGITAL_CHANNEL_MAX;

    return (uint16_t)crsfOutput;
}


// Map stick values into traditional standard CRSF channel microsecond positions (1000us to 2000us)

// AUX1/AUX2/AUX3: 对应SW1/SW2/SW3开关(触发=最大值, 未触发=最小值)
// Mode 4: AIL=左X, ELE=右Y, THR=左Y, RUD=右X
// Mode 3: AIL=左X, ELE=左Y, THR=右Y, RUD=右X
// Mode 2: AIL=右X, ELE=右Y, THR=左Y, RUD=左X (默认)
// Mode 1: AIL=右X, ELE=左Y, THR=右Y, RUD=左X
// 根据遥控器模式(Mode 1-4)将摇杆值映射到标准CRSF通道
// ===== 通道映射 =====
void mapChannels() {
    int mode = transmitter.globalSettings.channelMode;
    if (mode < 1 || mode > 4) mode = 2; // Default to Mode 2

    switch (mode) {
        case 1:
            rcChannels[AIL]   = processStick(stickValues[RIGHTX], 0); 
            rcChannels[ELE]  = processStick(stickValues[LEFTY],  1);
            rcChannels[THR]  = processStick(stickValues[RIGHTY], 2);
            rcChannels[RUD]    = processStick(stickValues[LEFTX],  3); 
            break;
        case 2:
            rcChannels[AIL]   = processStick(stickValues[RIGHTX], 0); 
            rcChannels[ELE]  = processStick(stickValues[RIGHTY], 1);
            rcChannels[THR]  = processStick(stickValues[LEFTY],  2);
            rcChannels[RUD]    = processStick(stickValues[LEFTX],  3); 
            break;
        case 3:
            rcChannels[AIL]   = processStick(stickValues[LEFTX],  0); 
            rcChannels[ELE]  = processStick(stickValues[LEFTY],  1);
            rcChannels[THR]  = processStick(stickValues[RIGHTY], 2);
            rcChannels[RUD]    = processStick(stickValues[RIGHTX], 3); 
            break;
        case 4:
            rcChannels[AIL]   = processStick(stickValues[LEFTX],  0); 
            rcChannels[ELE]  = processStick(stickValues[RIGHTY], 1);
            rcChannels[THR]  = processStick(stickValues[LEFTY],  2);
            rcChannels[RUD]    = processStick(stickValues[RIGHTX], 3); 
            break;
    }

    rcChannels[AUX1] = (stickValues[SW1] == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX2] = (stickValues[SW2] == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;
    rcChannels[AUX3] = (stickValues[SW3] == 1) ? CRSF_DIGITAL_CHANNEL_MAX : CRSF_DIGITAL_CHANNEL_MIN;    
}



// 所有按键均带软件消抖(比较按下持续时间)
// 3. 右摇杆按键/AUX2/AUX3开关
// 2. 左摇杆按键/ARM开关
// 1. 板载按键(短按/长按)
// 处理所有数字输入设备:
// ===== 数字输入处理 =====
void getDigitalInputs(){
    /*
     * Handle digital input
     */
    
    // ESP32-C3 Built in Button  板载按键
    static uint32_t buttonPressedTime = 0;
    if (digitalRead(DIGITAL_PIN_BUTTON) == LOW) {
        if (buttonPressedTime == 0)
            buttonPressedTime = millis(); // Record start time
    } else {
        if (buttonPressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - buttonPressedTime;
            buttonPressedTime = 0;
            if (pressDuration >= BTN_LONG_PRESS) { // LongPress Duration for this is separate and hard-coded to 2s
                navigate = 7;
                // Set flag to start calibration
                // calibrationRequested = true;
            } else if (pressDuration >= BTN_SHORT_PRESS) {
                navigate = 6; // Built in button pressed
                //nextScreen();
            }
        }
    }
    

   // Left Joystick / ARM input
// GPIO7（AUX1/CH5）：按键模式或两档滑动开关模式

#ifdef ARM_MODE_BUTTON

    // ===== 按键模式：短按切换 ARM =====
    static uint32_t aux1PressedTime = 0;

    if (digitalRead(DIGITAL_PIN_SWITCH_ARM) == LOW) {
        if (aux1PressedTime == 0)
            aux1PressedTime = millis();
    } else {
        if (aux1PressedTime > 0) {
            unsigned long pressDuration = millis() - aux1PressedTime;
            aux1PressedTime = 0;

            if (pressDuration >= transmitter.globalSettings.auxLongPress) {
                if (!ARMED) {
                    // 保留原来的长按处理位置
                }
            } else if (pressDuration >= BTN_SHORT_PRESS) {
                stickValues[SW1] = !stickValues[SW1];
                ARMED = stickValues[SW1];
            }
        }
    }

#elif defined(ARM_MODE_SLIDE)

    // ===== 两档滑动开关模式：低电平 ARM，高电平 DISARM =====
    if (digitalRead(DIGITAL_PIN_SWITCH_ARM) == LOW) {
        stickValues[SW1] = true;
        ARMED = true;
    } else {
        stickValues[SW1] = false;
        ARMED = false;
    }

#endif

    // Right Joystick/Aux2/3 button 右摇杆按键/AUX2/AUX3开关
    static uint32_t aux2PressedTime = 0;
    if (digitalRead(DIGITAL_PIN_SWITCH_AUX2) == LOW) {
        if (aux2PressedTime == 0)
            aux2PressedTime = millis(); // Record time button was pressed
    } else {
        if (aux2PressedTime> 0) {  // pressStartTime is non-zero
            unsigned long pressDuration = millis() - aux2PressedTime;
            aux2PressedTime = 0;
            if (pressDuration >= transmitter.globalSettings.auxLongPress) {
                //if (!ARMED) {
                   // Not armed, long press - initiate menu? TBD
                //}
                stickValues[SW3] = !stickValues[SW3]; // Toggle AUX3
            } else if (pressDuration >= BTN_SHORT_PRESS) {
                stickValues[SW2] = !stickValues[SW2]; // Toggle AUX2
                navigate = 5; // Joystick button pressed
            }
        }
    }
}

// -----------------------------------------------------------------------------------------------------
// Startup stick commands
// -----------------------------------------------------------------------------------------------------

// 同时检测从接通到断开的下降沿作为去抖确认
// 用于快速设置切换(如切换ELRS预设配置文件)
// 检测ARM开关从断开到接通的上升沿(上电瞬间)
// ===== 检测ARM开关手势操作 =====
void checkGestureSetting() {
    bool currentArmState = stickValues[SW1]; 
    static bool lastArmState = false;        // Tracks previous arm switch position
    static uint32_t armTimer = 0;            // Records when the switch was turned on
    static bool gestureTriggerReady = false; // Arming arm-switch trigger sequence flag

    // Detect the moment the switch goes from DISARMED to ARMED (Rising Edge)
    if (currentArmState && !lastArmState) {
        armTimer = millis();
        gestureTriggerReady = true; 
    }

    // Detect the moment the switch goes from ARMED back to DISARMED (Falling Edge)
    if (!currentArmState && lastArmState) {
        // Debounce: Ensure it was held armed for at least 1000ms
        if (gestureTriggerReady && (millis() - armTimer >= 1000)) {
            //selectSetting();    // Previously used for selecting from a set of ELRS presets. Future use for selecting between model presets?
            gestureTriggerReady = false; 
        }
    }
    lastArmState = currentArmState;
}


// 返回: true=超时, false=正常
// 用于提醒用户油门未动(防止模型失控)
// 如果油门杆在STICK_ALARM_TIME时间内没有移动，返回true
// ===== 检查油门空闲超时 =====
bool checkThrottleIdleTimeout(){
    static int previous_throttle = 191;
    static int stickMoved = 0;
    static uint32_t stickMovedMillis = 0;

    // check if throttle stick moved, warning after 5 minutes
    if(abs(previous_throttle - rcChannels[THR]) < 30){
        stickMoved = 0;
        //Serial.println(abs(previous_throttle - rcChannels[THR]));
    }else{
        previous_throttle = rcChannels[THR];
        stickMovedMillis = millis();
        stickMoved = 1;
    }

    if (millis() - stickMovedMillis > STICK_ALARM_TIME){
       // Serial.println((millis() - stickMovedMillis));
        return true;
    }else{
        return false;
    }
}


// 当前实现被注释掉，可根据需要启用
// 预留函数，用于通过蜂鸣器发出提示音
// ===== 蜂鸣器鸣响(当前未启用) =====
void playTone(uint8_t timesPerSecond) {
    // static uint32_t previousToneMillis = 0;
    // unsigned long currentMillis = millis();
    // //ACTIVE_BUZZER / VIBRATOR
    // if (currentMillis - previousToneMillis <= timesPerSecond*100) {
    //     analogWrite(DIGITAL_PIN_BUZZER, 128);
    // }else if(currentMillis - previousToneMillis > timeSeconde*100 && currentMillis - previousToneMillis <= timeSeconde*200){
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }else if(currentMillis - previousToneMillis > timeSeconde*200 && currentMillis - previousToneMillis <= timeSeconde*300){
    //     analogWrite(DIGITAL_PIN_BUZZER, 128);
    // }else if(currentMillis - previousToneMillis > timeSeconde*300 && currentMillis - previousToneMillis <= 5000){
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }else{
    //     previousToneMillis = currentMillis;     // save the last time buzzer play tone
    //     analogWrite(DIGITAL_PIN_BUZZER, 0);
    // }
}


// 参数: ledPin=LED引脚号, blinkRate=闪烁间隔(ms)
// 非阻塞式LED闪烁(使用millis()计时，不占用CPU)
// ===== LED闪烁控制 =====
void blinkLED(int ledPin, uint16_t blinkRate) {
    static uint8_t ledState = LOW;
    static uint32_t previousLedMillis = 0;
    uint32_t currentMillis = millis();

    if (currentMillis - previousLedMillis >= blinkRate) {
        previousLedMillis = currentMillis;     // save the last time you blinked the LED
        ledState ^= 1;                      // if the LED is off turn it on and vice-versa
        digitalWrite(ledPin, ledState);
    }
}


// Flash Led and play buzzer depending on current state

// 屏幕刷新率: 已武装且非蓝牙模式时2Hz，其他情况10Hz
// 4. 导航事件处理(菜单操作)
// 3. OLED屏幕刷新(根据当前屏幕类型绘制不同界面)
// 2. 蜂鸣器提示
// 1. LED状态指示(信号强度/电池低电压/USB供电/油门空闲警告)
// 综合处理:
// ===== 状态显示与系统管理 =====
void statusDisplay(){
    static int avg = -120;
    int raw = -120;
    int strength = 0;
    char* type;

    if (currentScreen == MODEL_FINDER) {
        // If we are on the model finder screen, prioritize the LED display to show current state
        // Calculations from https://github.com/iamsunilchahal/edgetx-lua-scripts-bw/blob/main/SCRIPTS/TOOLS/ELRS_Finder.lua
        raw = crsfClass.telemetryActive ? crsfClass.linkStats.uplinkRssi1 : -120;
        // Exponential moving average for stability
        avg = 0.8 * avg + 0.2 * (raw);

        // Map avg (~-110..-40) to 0..100 “strength”
        strength = constrain( (avg + 110) * (100/(70)), 0, 100 );    // -110→0, -40→100

        uint16_t period = constrain (1200 - (strength * 10), 100, 1200); // ticks (10ms each): 120→1.2s, 10→0.1s
        blinkLED(DIGITAL_PIN_LED, period);

    } else if (batteryVoltage < WARNING_VOLTAGE && batteryVoltage >= BEEPING_VOLTAGE) {
        blinkLED(DIGITAL_PIN_LED, 500);
    }else if(batteryVoltage < BEEPING_VOLTAGE && batteryVoltage >= ON_USB){
        blinkLED(DIGITAL_PIN_LED, 250);
        playTone(2);
    }

    // check stick idle timer
    if (checkThrottleIdleTimeout() == true){
        blinkLED(DIGITAL_PIN_LED, 100);
        playTone(5);
    }

    // OLED Screen update
    // Global variable navigate will have been set by the getAnalogInputs or getDigitalInputs functions 
    // If no movement was made, navigate will be zero. Otherwise, Up = 1, Down = 2, Left = 3, Right = 4.
    // Joystick (5) or builtin button (6,7 for short/long press) take precedence over joystick movement
    if (navigate > 0) {  // Navigation has occurred
        handleNavigation(navigate, transmitter, elrsClass, btJoystickModeActive, calibrationRequested);
        navigate = 0; // Reset navigation
    }

    int connectionState = 0;
    uint32_t currentTime = millis();
    static uint32_t lastScreenUpdateTime = 0;
    // 首页包含实时摇杆准星：固定约 30 FPS 刷新，避免 ARMED 状态下原先 500ms（仅 2 FPS）造成卡顿。
    // 其他页面仍沿用原来的低刷新频率，避免无谓增加菜单页刷新开销。
    uint16_t screenUpdateTime = (currentScreen == MAIN_PAGE) ? 33 :
                                ((ARMED && !btJoystickModeActive) ? 500 : 100);
    if (currentTime - lastScreenUpdateTime >= screenUpdateTime) { 
        lastScreenUpdateTime = currentTime;
        
        switch (currentScreen) {
            case MAIN_PAGE: {     
                // 0 = ELRS not connected, 1 = Telemetry Active, 4 = BT Joystick mode
                connectionState = (crsfClass.telemetryActive) ? 1 : 0;
                connectionState = (btJoystickModeActive) ? 4 : connectionState;

                // 每 3 秒在遥控器本机电压与飞机回传电压之间切换。
                // 本机 batteryVoltage 的 ADC 采样/校正和所有报警逻辑保持不变。
                static uint32_t lastVoltageDisplaySwitch = 0;
                static bool showAircraftVoltage = false;
                static bool voltageDisplayTimerStarted = false;
                if (!voltageDisplayTimerStarted) {
                    lastVoltageDisplaySwitch = currentTime;
                    voltageDisplayTimerStarted = true;
                } else if (currentTime - lastVoltageDisplaySwitch >= 3000) {
                    showAircraftVoltage = !showAircraftVoltage;
                    lastVoltageDisplaySwitch = currentTime;
                }

                bool aircraftVoltageFresh = crsfClass.aircraftBatteryValid &&
                    (currentTime - crsfClass.lastAircraftBatteryTime < 3000);
                float displayVoltage = batteryVoltage;
                if (showAircraftVoltage) {
                    displayVoltage = aircraftVoltageFresh ? crsfClass.aircraftVoltage : -1.0f;
                }
                drawHomeScreen(displayVoltage, showAircraftVoltage, connectionState,
                               crsfClass.linkStats.uplinkLQ, stickValues, ADC_MIN, ADC_MAX);
                break;
            }
            case ELRS_STATS: 
                drawElrsStatsScreen(elrsClass);
                break; 
            case ELRS_MENU: 
                drawMenuScreen(elrsClass.txModule.params);
                break; 
            case CHANNEL_OUTPUTS: 
                drawChannelOutputsScreen(rcChannels,CRSF_DIGITAL_CHANNEL_MIN,CRSF_DIGITAL_CHANNEL_MAX);
                break;
            case CHANNEL_MENU: 
                drawMenuScreen(transmitter.handsetSettingsMenu);
                break; 
            case BT_JOYSTICK: 
                drawBtJoystickScreen(btJoystickModeActive);
                break;
            case MODEL_FINDER: 
                drawModelFinderScreen(raw,avg,strength);
                break;
        }     
    }
}


// 注意: 仅在DEBUG宏定义时才会执行
// - ELRS链路质量(RSSI/发射功率/链路质量)
// - ARM状态
// - AUX开关状态
// - 各通道CRSF值(AIL/ELE/THR/RUD)
// - 电池电压
// 每250ms通过USB串口打印一次调试信息:
// ===== 串口调试数据输出 =====
void logData(){
    static uint32_t lastDisplayTime = 0;
    if (millis() - lastDisplayTime > 250) {
        lastDisplayTime = millis();

        char buf[6]; 
        // Serial.print("Raw AETR:");
        // sprintf(buf, "%5d", rawValues.rightX);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.rightY);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.leftY);
        // Serial.print(buf);
        // sprintf(buf, "%5d", rawValues.leftX);
        // Serial.print(buf);

        Serial.print("Bat:");
        sprintf(buf, "%5.2f", batteryVoltage);
        Serial.print("v Channels AETR: ");
        sprintf(buf, "%5d", rcChannels[AIL]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[ELE]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[THR]);
        Serial.print(buf);
        sprintf(buf, "%5d", rcChannels[RUD]);
        Serial.print(buf);
        Serial.print(" AUX: ");
        Serial.print(stickValues[SW1]);
        Serial.print(stickValues[SW2]);
        Serial.print(stickValues[SW3]);
        if (ARMED) Serial.print("(ARMED)");
        Serial.print("  "); 
        if (crsfClass.moduleConnected){
            // Generate the 4-line bar metric matching EdgeTX's logic
            String barIndicator = "[    ]";
            Serial.print("ELRS ");
            if (crsfClass.telemetryActive) {
                Serial.print("RSSI1: "); Serial.print(crsfClass.linkStats.uplinkRssi1); Serial.print("dBm");
                Serial.print(" | TX Pwr: "); Serial.print(crsfClass.linkStats.txPower); 
                Serial.print(" | LQ: "); Serial.print(crsfClass.linkStats.rfMode); Serial.print(" : "); Serial.print(crsfClass.linkStats.uplinkLQ); Serial.print(" ");

                if (crsfClass.linkStats.uplinkLQ >= 80)      barIndicator = "[====]";
                else if (crsfClass.linkStats.uplinkLQ >= 50) barIndicator = "[=== ]";
                else if (crsfClass.linkStats.uplinkLQ >= 20) barIndicator = "[==  ]";
                else if (crsfClass.linkStats.uplinkLQ > 0)   barIndicator = "[=   ]";
            }
            Serial.print(barIndicator); Serial.print("  "); 
        }

        Serial.println();

    }
}

// -----------------------------------------------------------------------------------------------------
// Board setup
// -----------------------------------------------------------------------------------------------------

// 11. 初始化CRSF串口通信
// 10. 启动串口调试(9600波特)
// 9. 初始化OLED显示屏
// 8. 加载全局设置
// 7. 加载校准数据
// 6. 配置LED为输出
// 5. 配置开关/按键引脚为上拉输入
// 4. 配置摇杆引脚为输入模式
// 3. 设置衰减为11dB(0-3.3V全量程)
// 2. 设置ADC为12位分辨率(0-4095)
// 1. 初始化CRSF通道数组为最小值
// 执行:
// ===== Arduino初始化函数(上电执行一次) =====
void setup()
{
    // inialize rc data
    for (uint8_t i = 0; i < CRSF_MAX_CHANNEL; i++) {
        rcChannels[i] = CRSF_DIGITAL_CHANNEL_MIN;
    }

    // Configure ESP32 analog inputs to 12-bit resolution (0 - 4095)
    // Native ESP32 configuration commands
    analogReadResolution(12);       // Fix resolution layout at 12-bit scale
    analogSetAttenuation(ADC_11db);  // Force 0 - 3.3V full dynamic voltage reading scale

    pinMode(STICK_RIGHT_X, INPUT);
    pinMode(STICK_RIGHT_Y, INPUT);
    pinMode(STICK_LEFT_Y, INPUT);
    pinMode(STICK_LEFT_X, INPUT);
    pinMode(ANALOG_PIN_VOLTAGE, INPUT);

    pinMode(DIGITAL_PIN_SWITCH_ARM, INPUT_PULLUP);
    pinMode(DIGITAL_PIN_SWITCH_AUX2, INPUT_PULLUP);

    pinMode(DIGITAL_PIN_LED, OUTPUT);    // LED
    //pinMode(DIGITAL_PIN_BUZZER, OUTPUT); // BUZZER
    pinMode(DIGITAL_PIN_BUTTON, INPUT_PULLUP);
    
    calibrationLoad();
    transmitter.loadGlobalSettings(true);
    
    // passive buzzer, no startup tone
    // digitalWrite(DIGITAL_PIN_BUZZER, HIGH); //BUZZER OFF
    digitalWrite(DIGITAL_PIN_LED, LOW); // LED ON
    
    // Initialize OLED
    initDisplay();
    drawBootLogo();

    // Serial output over USB (Uart6)
    Serial.begin(9600);

// CRSF over Uart1
#ifdef ELRS_HALF_DUPLEX
    crsfClass.begin(ELRS_RX, ELRS_TX, true);
#else
    crsfClass.begin(ELRS_RX, ELRS_TX, false);
#endif
    delay(3000);
}

#ifdef TIMINGDEBUG
uint32_t startLoopUs = 0;
uint32_t loopCounter = 0;
uint32_t maxLoopTime = 0;
uint32_t minLoopTime = 999999;
uint32_t maxOledTime = 0;
uint32_t minOledTime = 999999;
uint32_t cumOledTime = 0;
uint32_t cntOledRuns = 0;
uint32_t maxElrsTime = 0;
uint32_t minElrsTime = 999999;
uint32_t lastStatsPrintTime = 0;
uint32_t totalTransmitFrames = 0;
#endif
uint32_t lastSyncPrintTime = 0;

// -----------------------------------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------------------------------

// 3. (TIMINGDEBUG模式)输出性能指标
//    i. 更新OLED显示/LED/蜂鸣器
//       - 正常模式: 通过CRSF/串口发送RC数据
//       - 蓝牙手柄模式: 通过BLE发送游戏手柄数据
//    h. 发送RC数据:
//    g. 更新ELRS模块
//    f. 读取遥测数据
//    e. (DEBUG模式)输出调试数据
//    d. 映射通道到CRSF标准值
//    c. 检测ARM开关手势
//    b. 读取数字输入(按键/开关)
//    a. 读取摇杆模拟量并处理
// 2. 否则:
// 1. 如果请求校准，执行校准流程
// 主循环执行流程:
// ===== Arduino主循环函数(无限循环) =====
void loop()
{
    static uint32_t crsfTime = 0;
#ifdef TIMINGDEBUG
    static uint32_t nextLogTimeUs = 0;
    uint32_t nowUs = micros();

    if (nextLogTimeUs = 0) nextLogTimeUs = nowUs + 4000;
    loopCounter++;
    if (nowUs >= nextLogTimeUs) {
        startLoopUs = micros();
    }
#endif
    // Calibration (if requested)
    if (calibrationRequested)
        calibrationRun();
    else {
        // Get the gimbal stick values
        getAnalogInputs();

        // Read AUX channels and buttons
        getDigitalInputs();

        // Check for quick settings change via arm toggle gesture
        checkGestureSetting(); 
        
        mapChannels();

#ifdef DEBUG        
        // Debug data
        logData();
#endif
 
        // ---------------------------------------------------------------------------
        // Read Telemetry data every loop (telemetry processed in crsfCheckTelemetry)
        crsfClass.crsfReadTelemetry();

        // -------------------------------------------------
        // Poll the telemetry data and run lua script logic
        elrsClass.update(currentScreen == ELRS_MENU);       

        // --------------------------------
        // Send RC data to external module / BLE Gamepad
        uint32_t currentMicros = micros();
        if (currentMicros > crsfTime) {
            if (btJoystickModeActive) {
                if (!bleGamepadBegun) {
                    bleGamepad.begin();
                    bleGamepadBegun = true;
                }

                // Send Channel outputs or stick values.... 
                // Bluetooth HID expects 0 to 32767
                // int16_t bleLeftX  = map(rcChannels[RUD],   CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX, 0, 32767);
                // int16_t bleLeftY  = map(rcChannels[THR], CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX, 0, 32767);
                // int16_t bleRightX = map(rcChannels[AIL],  CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX, 0, 32767);
                // int16_t bleRightY = map(rcChannels[ELE], CRSF_DIGITAL_CHANNEL_MIN, CRSF_DIGITAL_CHANNEL_MAX, 0, 32767);
                // // Handle button states directly from stickValues 
                // if (stickValues[SW1] == 1) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
                // if (stickValues[SW2] == 1) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);
                // if (stickValues[SW3] == 1) bleGamepad.press(BUTTON_3); else bleGamepad.release(BUTTON_3);
                // bleGamepad.setAxes(bleLeftX, bleLeftY, 0, bleRightX, bleRightY, 0, 0, 0);       //(X, Y, Z, RX, RY, RZ, slider1, slider2)

                int bleLeftX   = map(stickValues[LEFTX],  ADC_MIN, ADC_MAX, 0, 32767);
                int bleLeftY   = map(stickValues[LEFTY],  ADC_MIN, ADC_MAX, 0, 32767);
                int bleRightX  = map(stickValues[RIGHTX], ADC_MIN, ADC_MAX, 0, 32767);
                int bleRightY  = map(stickValues[RIGHTY], ADC_MIN, ADC_MAX, 0, 32767);
                int bleLeftZ   = (stickValues[SW1] == 1) ? 32767 : 0;
                int bleRightZ  = (stickValues[SW2] == 1) ? 32767 : 0;
                int slider1    = (stickValues[SW3] == 1) ? 32767 : 0;
                bleGamepad.setAxes(bleLeftX, bleLeftY, bleLeftZ, bleRightX, bleRightY, bleRightZ, slider1, 0);       //(X, Y, Z, RX, RY, RZ, slider1, slider2)

                if (batteryVoltage > 4.1) {
                    bleGamepad.setBatteryLevel(100);
                } else if (batteryVoltage > 3.9) {
                    bleGamepad.setBatteryLevel(80);                    
                } else if (batteryVoltage > 3.8) {
                    bleGamepad.setBatteryLevel(60);                    
                } else if (batteryVoltage > 3.7) {
                    bleGamepad.setBatteryLevel(40);                    
                } else if (batteryVoltage > 3.6) {
                    bleGamepad.setBatteryLevel(20);                    
                } else if (batteryVoltage > 3.5) {
                    bleGamepad.setBatteryLevel(10);                    
                } else if (batteryVoltage > 3.4) {
                    bleGamepad.setBatteryLevel(1);                    
                } else if (batteryVoltage > ON_USB) {
                    bleGamepad.setBatteryLevel(80);                    
                } else { // On USB
                    bleGamepad.setBatteryLevel(100);                    
                }

                crsfTime = currentMicros + CRSF_TIME_BETWEEN_FRAMES_US; // default to 500hz as defined in crsf.h
            }
            else {
                if (bleGamepadBegun) {
                    crsfClass.commandQueue.clear();  // Clear out any queued up commands (device pings may queue up during BLE usage)
                    Serial.flush();  // Clear out the queued up serial output too
                    bleGamepad.end();
                    bleGamepadBegun = false;
                }                
                // Command packet interleaving
                if (crsfClass.commandQueue.hasItems()) {
                    crsfClass.crsfSendQueuedCommand();
                } else {
                    crsfClass.crsfSendChannels(rcChannels);
                }

#ifdef TIMINGDEBUG
                uint32_t elrsTime = micros() - currentMicros;
                if (elrsTime > maxElrsTime) maxElrsTime = elrsTime;
                if (elrsTime < minElrsTime) minElrsTime = elrsTime;
#endif
                uint32_t adjustment = crsfClass.crsfNextInterval();
                crsfTime = currentMicros + adjustment; //CRSF_TIME_BETWEEN_FRAMES_US;
            }
#ifdef DEBUG
            if (millis() - lastSyncPrintTime > 2000) {
                Serial.println("\n============ [Loop timing] ============");
                Serial.print("Next Interval : "); Serial.print(adjustment); Serial.println(" microseconds");
                Serial.println("=======================================");
                lastSyncPrintTime = millis();
            }
#endif
        }

#ifdef TIMINGDEBUG
        uint32_t oledStartTime = micros();
#endif  

        // Update OLED display, flash led and use beeper
        statusDisplay();

#ifdef TIMINGDEBUG
        uint32_t oledTime = micros() - oledStartTime;
        if (oledTime > maxOledTime) maxOledTime = oledTime;
        if (oledTime < minOledTime) minOledTime = oledTime;
        cumOledTime += oledTime;
        cntOledRuns++;
#endif

    } // Not calibrating
#ifdef TIMINGDEBUG
    if (nowUs >= nextLogTimeUs) {
        uint32_t elapsedUs = micros() - startLoopUs;
        if (elapsedUs > maxLoopTime) maxLoopTime = elapsedUs;
        if (elapsedUs < minLoopTime) minLoopTime = elapsedUs;
        totalTransmitFrames++;

        nextLogTimeUs += 4000; 
    }
    // ---------------------------------------------------------------------------
    if (millis() - lastStatsPrintTime > 5000) {
        lastStatsPrintTime = millis();      
        Serial.println("\n============ [HANDSET PERFORMANCE METRICS] ============");
        Serial.print("Max Loop Duration : "); Serial.print(maxLoopTime); Serial.println(" microseconds");
        Serial.print("Min Loop Duration : "); Serial.print(minLoopTime); Serial.println(" microseconds");
        Serial.print("Avg Loop Duration : "); Serial.print(5000000/totalTransmitFrames); Serial.println(" microseconds");
        Serial.print("Total Transmit Frames   : "); Serial.println(totalTransmitFrames);
        Serial.print("Max OLED Duration : "); Serial.print(maxOledTime); Serial.println(" microseconds");
        Serial.print("Min OLED Duration : "); Serial.print(minOledTime); Serial.println(" microseconds");
        Serial.print("Avg OLED Duration : "); Serial.print(cumOledTime/cntOledRuns); Serial.println(" microseconds");
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCounter);
        Serial.print("Max ELRS Duration : "); Serial.print(maxElrsTime); Serial.println(" microseconds");
        Serial.print("Min ELRS Duration : "); Serial.print(minElrsTime); Serial.println(" microseconds");
        Serial.print("Avg ELRS Duration : "); Serial.print(5000000/loopCounter); Serial.println(" microseconds");
        Serial.print("Total ELRS Frames   : "); Serial.println(loopCounter);
        Serial.println("=======================================================");
        
        // Reset max peak-hold values to check for performance dips in the next window
        maxLoopTime = 0;
        minLoopTime = 0;
        maxElrsTime = 0;
        minElrsTime = 0;
        totalTransmitFrames = 0;
        loopCounter = 0;
    }    
#endif
}