/*
 * 本文件属于 ESP32C3-TX 项目
 * (GPLv3 License Header Omitted for Brevity)
 */
 
#include "oled.h"
#include "locale_zh_cn.h"

// ==================== 全局 UI 状态变量 ====================
displayState currentScreen = MAIN_PAGE; // 当前显示的页面
displayState lastScreen;                // 上一次显示的页面（用于判断是否需要重绘）
menuMode menuState = MENU_BROWSE;       // 当前菜单模式（浏览/编辑）

// 屏幕旋转设置：本项目仅支持 U8G2_R0（0°）和 U8G2_R2（180°翻转）
// 禁止使用 R1/R3（90°/270°），否则局部刷新区域和硬编码坐标会全部错位
// 如需水平镜像翻转，请在 initDisplay() 中使用 display.setFlipMode(true)
#ifdef OLED_OPTION_1
// 0.42寸 72x40 SSD1306 (I2C, 全屏缓冲模式, 无复位引脚, SCL=6, SDA=5)
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 6, 5);
#else
// 0.96寸 128x64 SSD1306 (I2C, 全屏缓冲模式, 无复位引脚, SCL=6, SDA=5)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 6, 5);
#endif

// 本地化绘制：仅替换屏幕显示内容，原始参数 label / NVS 键 / CRSF 数据均不修改。
static void drawUiStr(u8g2_uint_t x, u8g2_uint_t y, const char* source) {
    const char* translated = translateUiText(source);
    if (translated == source) { display.drawStr(x, y, source); return; }
    const uint8_t* previousFont = display.getU8g2()->font;
    display.setFont(u8g2_font_wqy12_t_gb2312);
    // 12px 中文字形比原英文小字体高，稍微下移以保持基线可见。
    display.drawUTF8(x, y + 3, translated);
    display.setFont(previousFont);
}
static int getUiTextWidth(const char* source) {
    const char* translated = translateUiText(source);
    if (translated == source) return display.getStrWidth(source);
    const uint8_t* previousFont = display.getU8g2()->font;
    display.setFont(u8g2_font_wqy12_t_gb2312);
    int width = display.getUTF8Width(translated);
    display.setFont(previousFont);
    return width;
}
static void printUiText(const char* source) {
    const char* translated = translateUiText(source);
    if (translated == source) { display.print(source); return; }
    const uint8_t* previousFont = display.getU8g2()->font;
    const u8g2_uint_t cursorX = display.getCursorX();
    const u8g2_uint_t cursorY = display.getCursorY();
    display.setFont(u8g2_font_wqy12_t_gb2312);
    display.setCursor(cursorX, cursorY + 3);
    display.print(translated);
    display.setFont(previousFont);
}

// ==================== 局部刷新核心算法 ====================
// 将项目逻辑坐标系(72x40)的 Tile 区域转换为物理屏幕控制器缓冲区中的实际 Tile 区域
// U8g2 按 8px 高的水平条带(Tile)管理显存，此函数实现逻辑-物理映射
static void updateLogicalDisplayArea(uint8_t logicalTileX, uint8_t logicalTileY,
                                     uint8_t logicalTileW, uint8_t logicalTileH) {
    if (logicalTileW == 0 || logicalTileH == 0) return;

    // 计算逻辑区域在物理缓冲区中的像素起止坐标
    const int xStart = OLED_X_OFFSET + logicalTileX * 8;
    const int yStart = OLED_Y_OFFSET + logicalTileY * 8;
    const int xEnd = OLED_X_OFFSET + (logicalTileX + logicalTileW) * 8 - 1;
    const int yEnd = OLED_Y_OFFSET + (logicalTileY + logicalTileH) * 8 - 1;

    // 将像素坐标转换回物理 Tile 索引
    const uint8_t physicalTileX = xStart / 8;
    const uint8_t physicalTileY = yStart / 8;
    const uint8_t physicalTileW = (xEnd / 8) - physicalTileX + 1;
    const uint8_t physicalTileH = (yEnd / 8) - physicalTileY + 1;

    // 调用 U8g2 底层 API 仅发送指定 Tile 区域到屏幕
    display.updateDisplayArea(physicalTileX, physicalTileY, physicalTileW, physicalTileH);
}

// ==================== 显示初始化 ====================
void initDisplay() {
    Wire.begin(5, 6);           // 强制指定 I2C 硬件引脚 (SDA=5, SCL=6)
    display.begin();            // 初始化 U8g2 驱动
    display.setContrast(255);   // 最大对比度/亮度
    display.setBusClock(800000);// 800kHz I2C 时钟（SSD1306 支持 Fast Mode Plus）
}

// ==================== 开机 Logo ====================
void drawBootLogo() {
    display.clearBuffer();
    // 绘制 NanoTX 位图 Logo (72x40)
    display.drawXBM(OLED_X_OFFSET, OLED_Y_OFFSET, 72, 40, nanotx_logo_72x40);

    display.setFont(u8g2_font_tiny5duo_tf); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 36);
    display.print("ELRS  CN");
    
    display.setCursor(OLED_X_OFFSET + 50, OLED_Y_OFFSET + 36);
    display.print("v1.2");
    
    display.sendBuffer();     
}

void clearScreen() {
    display.clearBuffer();
    display.sendBuffer(); 
}

// ==================== 页面轮转 ====================
void nextScreen() {
    switch (currentScreen) {
        case MAIN_PAGE:       currentScreen = ELRS_STATS;      break;
        case ELRS_STATS:      currentScreen = CHANNEL_OUTPUTS; break;
        case CHANNEL_OUTPUTS: currentScreen = BT_JOYSTICK;     break;
        case BT_JOYSTICK:     currentScreen = MODEL_FINDER;    break;
        case MODEL_FINDER:    currentScreen = MAIN_PAGE;       break; // 循环回到首页
        default: break; // 菜单页面不参与轮转
    }
}

// ==================== 主页绘制（含差量局部刷新优化）====================
void drawHomeScreen(float voltage, bool aircraftVoltageMode, int telemetryState, uint8_t uplinkLQ, 
                    int16_t channels[], int16_t minChannelValue, int16_t maxChannelValue) {
    // 静态变量记录上一帧状态，用于差量刷新判断
    static int lastVoltageX10 = -99;
    static bool lastAircraftVoltageMode = false;
    static int lastTelemetryState = -1;
    static int lastBars = 0;
    static int16_t lastAux1 = -1;
    static int16_t lastAux2 = -1;
    static int16_t lastAux3 = -1;

#ifdef TIMINGDEBUG
    uint32_t processTime, transmitTime;
    uint32_t startTime = micros();
#endif

    // --- 上半区：电压 + 信号强度 ---
    // 仅清除上半区(17px高)，减少显存操作
    display.setDrawColor(0);
    display.drawBox(OLED_X_OFFSET, OLED_Y_OFFSET, 72, 17);
    display.setDrawColor(1);

    // 1. 左上角：大号电压数字。负值表示飞机遥测暂时无效。
    display.setFont(u8g2_font_logisoso16_tf); 
    display.setCursor(6 + OLED_X_OFFSET, 16 + OLED_Y_OFFSET);
    if (voltage >= 0.0f) {
        display.print(voltage, 1);
    } else {
        // 遥测无效时使用三字符占位，避免挤占右侧信号显示区域。
        display.print("-.-");
    }

    // 数字右侧采用固定的“V/V 分数标记”：横线始终显示，只有当前来源对应的 V 显示。
    // 上方 V = 飞机回传电压；下方 V = 遥控器本机电池电压。
    display.setFont(u8g2_font_tiny5duo_tf);
    int voltageMarkX = display.getCursorX() + 2;
    display.drawHLine(voltageMarkX, 8 + OLED_Y_OFFSET, 5);
    if (aircraftVoltageMode) {
        display.setCursor(voltageMarkX, 6 + OLED_Y_OFFSET);  // 横线上方，较原位置上移 1 像素
    } else {
        display.setCursor(voltageMarkX, 15 + OLED_Y_OFFSET); // 横线下方，较原位置上移 1 像素
    }
    display.print("v");

    // 2. 右上角：信号强度柱状图 / 连接状态 / 蓝牙图标
    uint8_t bars = 0;
    if (telemetryState == 1) {
        // 根据上行链路质量(LQ)计算信号格数
        if (uplinkLQ > 75) bars = 4;
        else if (uplinkLQ > 50) bars = 3;
        else if (uplinkLQ > 25) bars = 2;
        else if (uplinkLQ > 0)  bars = 1;
        
        if (bars >= 1) display.drawBox(45 + OLED_X_OFFSET, 11 + OLED_Y_OFFSET, 3, 3);
        if (bars >= 2) display.drawBox(52 + OLED_X_OFFSET, 8 + OLED_Y_OFFSET,  3, 6);
        if (bars >= 3) display.drawBox(59 + OLED_X_OFFSET, 4 + OLED_Y_OFFSET,  3, 10);
        if (bars >= 4) display.drawBox(66 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET,  3, 14);
    } else if (telemetryState == 0) {
        // 未连接：显示 "--"
        display.drawBox(47 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2);
        display.drawBox(59 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2);
    } else if (telemetryState == 4) {
        // 蓝牙手柄模式：显示 "J" 图标
        display.setFont(u8g2_font_open_iconic_embedded_2x_t); 
        drawUiStr(OLED_X_OFFSET + 47, OLED_Y_OFFSET + 16, "J");
    }
    if (telemetryState == 2 || telemetryState == 3) {
        // 蓝牙发射模式：显示蓝牙小图标
        display.drawXBM(45 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET, 8, 10, bt_logo_8x10);
    }

    // 差量刷新计算：来源标记也属于电压区域，模式切换时必须刷新标记和横线。
    uint8_t tileX = 0, tileY = 0, tileW = 9, tileH = 2;
    bool voltageDisplayChanged = ((int)(voltage * 10) != lastVoltageX10) ||
                                 (aircraftVoltageMode != lastAircraftVoltageMode) ||
                                 (currentScreen != lastScreen);
    bool signalDisplayChanged = (lastBars != bars) ||
                                (telemetryState != lastTelemetryState) ||
                                (currentScreen != lastScreen);
    if (voltageDisplayChanged && !signalDisplayChanged) {
        // 电压数字/来源标记变化，刷新左侧 48px，覆盖分数标记的完整宽度。
        tileX = 0; tileW = 6;
    } else if (!voltageDisplayChanged && signalDisplayChanged) {
        tileX = 4; tileW = 5;
    } else if (!voltageDisplayChanged && !signalDisplayChanged) {
        tileW = 0;
    }

#ifdef TIMINGDEBUG
    processTime = micros() - startTime;
#endif
    // 根据屏幕型号选择直接刷新或逻辑坐标转换刷新
#ifdef OLED_OPTION_1
    if (tileW > 0) display.updateDisplayArea(tileX, tileY, tileW, tileH);
#else
    if (tileW > 0) updateLogicalDisplayArea(tileX, tileY, tileW, tileH);
#endif
#ifdef TIMINGDEBUG
    transmitTime = micros() - (startTime + processTime);
    Serial.print("Top Half Process Time: "); Serial.print(processTime); 
    Serial.print("uS. Send time: "); Serial.print(transmitTime); Serial.println("uS.");
#endif
    lastVoltageX10 = voltage*10;
    lastAircraftVoltageMode = aircraftVoltageMode;
    lastBars = bars;
    lastTelemetryState = telemetryState;

    // --- 下半区：摇杆十字准星 + AUX滑块 ---
    const uint8_t boxSize = 22;
    const uint8_t leftBoxX = 12, rightBoxX = 40, boxesY = 18;

    // 快速擦除下半区背景
    display.setDrawColor(0);
    display.drawBox(0+OLED_X_OFFSET, 17+OLED_Y_OFFSET, 72, 22); 
    display.setDrawColor(1);

    // 绘制左右摇杆框
    display.drawFrame(leftBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    display.drawFrame(rightBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    
    // 分上下半段映射，并把校准中位值精确映射到方框几何中心。
    // 2x2 准星的绘制起点是坐标-1，因此这里映射的是准星中心参考坐标。
    // 原先一次性 map() 会因整数截断把中位值映射到中心左/上 1 像素。
    const int16_t channelMid = minChannelValue + (maxChannelValue - minChannelValue) / 2;
    auto mapAxisCentered = [&](int16_t value, int16_t targetMin, int16_t targetMid, int16_t targetMax) -> uint8_t {
        value = constrain(value, minChannelValue, maxChannelValue);
        if (value <= channelMid) {
            return (uint8_t)map(value, minChannelValue, channelMid, targetMin, targetMid);
        }
        return (uint8_t)map(value, channelMid, maxChannelValue, targetMid, targetMax);
    };
    const int16_t xTargetMin = leftBoxX + 2;
    const int16_t xTargetMid = leftBoxX + boxSize / 2;
    const int16_t xTargetMax = leftBoxX + boxSize - 2;
    const int16_t yTargetMin = boxesY + 2;
    const int16_t yTargetMid = boxesY + boxSize / 2;
    const int16_t yTargetMax = boxesY + boxSize - 2;

    // config.h 中 stickInputs 的顺序是 LEFTX=0、LEFTY=1、RIGHTX=2、RIGHTY=3。
    // 左侧框显示左摇杆，右侧框显示右摇杆。保留原有映射，不给显示位置额外加迟滞。
    uint8_t leftX  = mapAxisCentered(channels[0], xTargetMin, xTargetMid, xTargetMax);
    uint8_t leftY  = mapAxisCentered(channels[1], yTargetMax, yTargetMid, yTargetMin); // Y轴反转
    uint8_t rightX = mapAxisCentered(channels[2], rightBoxX + 2, rightBoxX + boxSize / 2, rightBoxX + boxSize - 2);
    uint8_t rightY = mapAxisCentered(channels[3], yTargetMax, yTargetMid, yTargetMin); // Y轴反转

    // 绘制十字准星指示点
    display.drawBox(leftX - 1 + OLED_X_OFFSET, leftY - 1 + OLED_Y_OFFSET, 2, 2);
    display.drawBox(rightX - 1 + OLED_X_OFFSET, rightY - 1 + OLED_Y_OFFSET, 2, 2);

    // 两侧 AUX 滑块 (二值化显示：0=低位, 非0=高位)
    display.drawLine(4 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET, 4 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux1Y = (channels[4]==0) ? 36 : 16;
    display.drawBox(2 + OLED_X_OFFSET, aux1Y + OLED_Y_OFFSET, 5, 3);

    display.drawLine(68 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET, 68 + OLED_X_OFFSET, 26 + OLED_Y_OFFSET);
    uint8_t aux2Y = (channels[5]==0) ? 24 : 16;
    display.drawBox(66 + OLED_X_OFFSET, aux2Y + OLED_Y_OFFSET, 5, 3);

    display.drawLine(68 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET, 68 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux3Y = (channels[6]==0) ? 36 : 28;
    display.drawBox(66 + OLED_X_OFFSET, aux3Y + OLED_Y_OFFSET, 5, 3);

    // 下半区差量刷新
    tileX = 0; tileY = 2; tileW = 9; tileH = 3;
    if (channels[4]==lastAux1 && currentScreen == lastScreen) { tileX += 1; tileW -= 1; }
    if (channels[5]==lastAux2 && channels[6]==lastAux3 && currentScreen == lastScreen) tileW -= 1;

#ifdef TIMINGDEBUG
    processTime = micros() - startTime;
#endif
#ifdef OLED_OPTION_1
    display.updateDisplayArea(tileX, tileY, tileW, tileH);
#else
    updateLogicalDisplayArea(tileX, tileY, tileW, tileH);
#endif
#ifdef TIMINGDEBUG
    transmitTime = micros() - (startTime + processTime);
    Serial.print("Stick Pos Process Time: "); Serial.print(processTime); 
    Serial.print("uS. Send time: "); Serial.print(transmitTime); Serial.println("uS.");
#endif

    lastAux1 = channels[4]; lastAux2 = channels[5]; lastAux3 = channels[6];
    lastScreen = currentScreen;
}

// ==================== ELRS 统计页 ====================
void drawElrsStatsScreen(const ELRS& elrs) {
    display.clearBuffer();
    display.setFont(u8g2_font_squeezed_b7_tr); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 8);
    
    if (strlen(elrs.txModule.name) > 0) {
        display.print(elrs.txModule.name);
        display.setFont(u8g2_font_NokiaSmallPlain_tr); 
        display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 22);

        if (elrs.ready()) {
            // 通过标签名查找参数并显示当前值
            int index = elrs.txModule.params.findParamByLabel(LABEL_PACKET_RATE);
            if (index >= 0) {
                const crsfParameter &param = elrs.txModule.params.getParam(index);
                printUiText(param.choices[param.currentVal].text); 
            } else { printUiText("Packet rate ???"); }
            
            display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 36);
            index = elrs.txModule.params.findParamByLabel(LABEL_TX_POWER_STRING);
            if (index >= 0) {
                const crsfParameter &param = elrs.txModule.params.getParam(index);
                printUiText(param.label); 
            } else { printUiText("TX power ???"); }
        } else {
            printUiText("Loading Params...");
        }
    } else {
        // 未连接高频头时直接绘制短中文，避免英文翻译路径/字体切换导致乱码或截断。
        display.setFont(u8g2_font_wqy12_t_gb2312);
        const char* noModuleText = "模块未连接";
        int16_t noModuleWidth = display.getUTF8Width(noModuleText);
        display.drawUTF8(OLED_X_OFFSET + (72 - noModuleWidth) / 2,
                         OLED_Y_OFFSET + 17, noModuleText);
        display.setFont(u8g2_font_streamline_interface_essential_circle_triangle_t);
        int16_t warningWidth = display.getStrWidth("3");
        display.setCursor(OLED_X_OFFSET + (72 - warningWidth) / 2, OLED_Y_OFFSET + 38);
        display.print("3"); // 警告三角图标按实际字宽水平居中
    }
    display.sendBuffer();
    lastScreen = currentScreen;
}

// ==================== 通道输出柱状图页 ====================
void drawChannelOutputsScreen(int16_t channels[], int16_t min, int16_t max) {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tr); 
    drawUiStr(OLED_X_OFFSET + 0, OLED_Y_OFFSET + 7, "Channel Output");

    const char* labels[] = {"A", "E", "T", "R"};
    const int16_t startX = OLED_X_OFFSET + 10;                     
    const int16_t endX = OLED_X_OFFSET + 70;                       
    const int16_t midX = startX + (endX - startX) / 2; 
    const int16_t barHeight = 3;                   
    
    for (uint8_t i = 0; i < 4; i++) {
        int16_t y = OLED_Y_OFFSET + 14 + (i * 8); 
        drawUiStr(OLED_X_OFFSET + 0, y + 3, labels[i]);
        
        // 绘制刻度框架（左/中/右三条竖线 + 横线）
        display.drawHLine(startX, y, endX - startX + 1);
        display.drawVLine(startX, y - 2, 5);
        display.drawVLine(midX, y - 2, 5);
        display.drawVLine(endX, y - 2, 5);
        
        int16_t currentVal = constrain(channels[i], min, max);
        // 从中点向两侧延伸的柱状图
        int16_t currentX = startX + (int32_t)(currentVal - min) * (endX - startX) / (max - min);
        if (currentX > midX) {
            display.drawBox(midX, y - 1, currentX - midX + 1, barHeight);
        } else if (currentX < midX) {
            display.drawBox(currentX, y - 1, midX - currentX + 1, barHeight);
        }
    }
    display.sendBuffer();
    lastScreen = currentScreen;
}

// ==================== 蓝牙连接状态页 ====================
void drawBtJoystickScreen(bool btJoystickActive) {
    display.clearBuffer();
    display.setDrawColor(1);

    // 以实际可视的 72x40 像素为基准：标题在上，左侧粗白蓝牙标志，右侧只显示当前状态。
    const char* title = "蓝牙连接";
    display.setFont(u8g2_font_wqy12_t_gb2312);
    int16_t titleWidth = display.getUTF8Width(title);
    display.drawUTF8(OLED_X_OFFSET + (OLED_WIDTH - titleWidth) / 2,
                     OLED_Y_OFFSET + 10, title);

    // 用加粗线段绘制实心感更强的白色蓝牙标志，避免 8x10 细小位图缩放后难以辨认。
    const int16_t bx = OLED_X_OFFSET + 7;
    const int16_t by = OLED_Y_OFFSET + 14;
    auto drawBoldLine = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        if (x1 == x2) {
            display.drawLine(x1 - 1, y1, x2 - 1, y2);
            display.drawLine(x1, y1, x2, y2);
            display.drawLine(x1 + 1, y1, x2 + 1, y2);
        } else {
            display.drawLine(x1, y1 - 1, x2, y2 - 1);
            display.drawLine(x1, y1, x2, y2);
            display.drawLine(x1, y1 + 1, x2, y2 + 1);
        }
    };
    // 中轴与上下折线，以及左侧两条斜线。
    drawBoldLine(bx + 10, by,      bx + 10, by + 21);
    drawBoldLine(bx + 10, by + 1,  bx + 19, by + 7);
    drawBoldLine(bx + 19, by + 7,  bx + 10, by + 11);
    drawBoldLine(bx + 10, by + 11, bx + 19, by + 15);
    drawBoldLine(bx + 19, by + 15, bx + 10, by + 21);
    drawBoldLine(bx + 2,  by + 5,  bx + 10, by + 11);
    drawBoldLine(bx + 2,  by + 16, bx + 10, by + 11);

    // 右侧只显示当前状态，用大号拉丁字体让 ON/OFF 与蓝牙标志更醒目。
    // 仅更改状态文字的显示，不改动蓝牙状态判断或按键逻辑。
    const char* status = btJoystickActive ? "ON" : "OFF";
    display.setFont(u8g2_font_logisoso16_tf);
    int16_t statusWidth = display.getUTF8Width(status);
    const int16_t statusAreaX = OLED_X_OFFSET + 32;
    const int16_t statusAreaWidth = OLED_WIDTH - 32;
    const int16_t statusX = statusAreaX + (statusAreaWidth - statusWidth) / 2;
    display.drawUTF8(statusX, OLED_Y_OFFSET + 32, status);
    display.sendBuffer();
    lastScreen = currentScreen;
}

// ==================== 文本滚动动画引擎 ====================
const unsigned long STATE_START_PAD  = 1000; // 滚动前静止等待(ms)
const unsigned long STATE_SCROLL     = 1000; // 滚动持续时间(ms)
const unsigned long STATE_END_PAD    = 500;  // 滚动后静止等待(ms)
const unsigned long TOTAL_CYCLE      = STATE_START_PAD + STATE_SCROLL + STATE_END_PAD;
const int MAX_TEXT_LENGTH            = 64;

// 菜单项文本滚动（带独立状态跟踪）
void drawScrollingMenuValue(const char* text, int textX, int textY, int displayAreaWidth) {
    int textWidth = getUiTextWidth(text);
    unsigned long currentMillis = millis();
    static unsigned long lastResetTime = 0;
    static char lastText[MAX_TEXT_LENGTH] = "";
    enum ScrollState {START, SCROLLING, FINISH};
    static ScrollState currentState = START;

    // 文本变化时重置动画状态
    if (strcmp(text, lastText) != 0) {
        strncpy(lastText, text, sizeof(lastText) - 1);
        lastText[sizeof(lastText) - 1] = '\0';
        currentState = START;
        lastResetTime = currentMillis;
    }
    
    // 短文本直接居中显示，不滚动
    if (textWidth <= displayAreaWidth) {
        int centerX = textX + (displayAreaWidth - textWidth) / 2;
        drawUiStr(centerX, textY, text);
        return;
    }

    // 循环滚动状态机
    if (currentMillis - lastResetTime >= TOTAL_CYCLE) lastResetTime = currentMillis;
    unsigned long progress = currentMillis - lastResetTime;
    int maxScrollDistance = textWidth - displayAreaWidth;
    int currentXOffset = 0;

    if (progress < STATE_START_PAD) {
        currentXOffset = 0;
    } else if (progress < (STATE_START_PAD + STATE_SCROLL)) {
        unsigned long scrollProgress = progress - STATE_START_PAD;
        currentXOffset = (maxScrollDistance * scrollProgress) / STATE_SCROLL;
    } else {
        currentXOffset = maxScrollDistance;
    }

    // 注意：调用方需提前设置 setClipWindow 防止文字溢出
    drawUiStr(textX - currentXOffset, textY, text);
}

// ==================== 菜单项绘制（按参数类型分派）====================
void drawMenuItem(const crsfParameter& menuItem, int offsetX, int offsetY) {
    int textWidth, centerX;
    float floatVal;
    static const int tenToThePowerOf[] = {1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};
    char buffer[32];
    display.setFont(u8g2_font_squeezed_b7_tr); 
    
    switch (menuItem.type) {
        case CRSF_FOLDER:
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH-7, OLED_Y_OFFSET+OLED_HEIGHT-5);
            drawScrollingMenuValue(menuItem.label, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 23 + offsetY, 62);
            break;
        case CRSF_COMMAND:
        case CRSF_INFO:
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            drawScrollingMenuValue(menuItem.label, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 23 + offsetY, 67);
            break;
        case CRSF_UINT8: case CRSF_INT8: case CRSF_UINT16: case CRSF_INT16:
        case CRSF_UINT32: case CRSF_INT32: case CRSF_UINT64: case CRSF_INT64:
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            textWidth = getUiTextWidth(menuItem.label);
            centerX = (OLED_WIDTH - textWidth) / 2;
            drawUiStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 15 + offsetY, menuItem.label);
            display.setFont(u8g2_font_NokiaSmallPlain_tr);
            snprintf(buffer, sizeof(buffer), "%d%s", (int32_t)menuItem.currentVal, menuItem.units);
            drawScrollingMenuValue(buffer, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 30 + offsetY, 62);
            break;            
        case CRSF_FLOAT:
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            textWidth = getUiTextWidth(menuItem.label);
            centerX = (OLED_WIDTH - textWidth) / 2;
            drawUiStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 15 + offsetY, menuItem.label);
            display.setFont(u8g2_font_NokiaSmallPlain_tr);                
            floatVal = (float)menuItem.currentVal / tenToThePowerOf[menuItem.precision];
            snprintf(buffer, sizeof(buffer), "%.*f%s", menuItem.precision, floatVal, menuItem.units);
            drawScrollingMenuValue(buffer, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 30 + offsetY, 62);
            break;
        case CRSF_TEXT_SELECTION:
            display.setClipWindow(OLED_X_OFFSET+5, OLED_Y_OFFSET+5, OLED_X_OFFSET+OLED_WIDTH, OLED_Y_OFFSET+OLED_HEIGHT-5);
            textWidth = getUiTextWidth(menuItem.label);
            centerX = (OLED_WIDTH - textWidth) / 2;
            drawUiStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 15 + offsetY, menuItem.label);
            display.setFont(u8g2_font_NokiaSmallPlain_tr);                
            drawScrollingMenuValue(menuItem.choices[menuItem.currentVal].text, OLED_X_OFFSET + 6 + offsetX, OLED_Y_OFFSET + 30 + offsetY, 62);
            break;   
    }
    display.setMaxClipWindow(); // 重置裁剪窗口
}

// ==================== 菜单过渡动画引擎 ====================
static uint8_t activeMenuAnimation = 0;     // 0=无, 1=上, 2=下, 3=左, 4=右
static unsigned long menuAnimStartTime = 0;
const unsigned long ANIM_DURATION = 500;    // 过渡动画时长(ms)
static int prevMenuItemIndex = 0;           // 动画起始项索引
static int currentMenuItemIndex = 0;        // 动画目标项索引

void animateMenu(const ParamCollection& menu) {
    if (activeMenuAnimation == 0) {
        drawMenuItem(menu.getCurrentParam(), 0, 0); // 无动画：静态绘制
    } else {
        unsigned long elapsed = millis() - menuAnimStartTime;
        if (elapsed >= ANIM_DURATION) {
            activeMenuAnimation = 0; // 动画结束
            drawMenuItem(menu.getCurrentParam(), 0, 0);
        } else {
            // 线性插值进度 (0~1000)
            long progress = (elapsed * 1000) / ANIM_DURATION; 
            int offsetX_Prev = 0, offsetY_Prev = 0, offsetX_Curr = 0, offsetY_Curr = 0;

            switch (activeMenuAnimation) {
                case 1: // 上滑：旧项上移离开，新项从下方进入
                    offsetY_Prev = -(OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = OLED_HEIGHT - (OLED_HEIGHT * progress) / 1000; break;
                case 2: // 下滑：旧项下移离开，新项从上方进入
                    offsetY_Prev = (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = -OLED_HEIGHT + (OLED_HEIGHT * progress) / 1000; break;
                case 3: // 左滑（进入子文件夹）：旧项左移，新项从右侧进入
                    offsetX_Prev = -(OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = OLED_WIDTH - (OLED_WIDTH * progress) / 1000; break;
                case 4: // 右滑（退出文件夹）：旧项右移，新项从左侧进入
                    offsetX_Prev = (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = -OLED_WIDTH + (OLED_WIDTH * progress) / 1000; break;
            }
            // 同时绘制新旧两项实现滑动效果
            drawMenuItem(menu.getParam(prevMenuItemIndex), offsetX_Prev, offsetY_Prev);
            drawMenuItem(menu.getParam(currentMenuItemIndex), offsetX_Curr, offsetY_Curr);
        }
    }    
}

// ==================== 编辑值绘制 ====================
void drawEditValue(const crsfParameter& menuItem, int32_t editValue, int offsetX, int offsetY) {
    int textWidth, centerX;
    float floatVal;
    static const int tenToThePowerOf[] = {1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};
    char buffer[32];
    display.setFont(u8g2_font_NokiaSmallPlain_tr); 
    
    switch (menuItem.type) {
        case CRSF_UINT8: case CRSF_INT8: case CRSF_UINT16: case CRSF_INT16:
        case CRSF_UINT32: case CRSF_INT32: case CRSF_UINT64: case CRSF_INT64:
        case CRSF_FLOAT:
            display.setClipWindow(OLED_X_OFFSET+1, OLED_Y_OFFSET+22, OLED_X_OFFSET+OLED_WIDTH-1, OLED_Y_OFFSET+OLED_HEIGHT-7);
            if (menuItem.precision > 0) {
                floatVal = (float)editValue / tenToThePowerOf[menuItem.precision];
                snprintf(buffer, sizeof(buffer), "%.*f%s", menuItem.precision, floatVal, menuItem.units);
            } else {
                snprintf(buffer, sizeof(buffer), "%d%s", (int32_t)editValue, menuItem.units);
            }
            textWidth = getUiTextWidth(buffer);
            centerX = (OLED_WIDTH - textWidth) / 2;
            drawUiStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 31 + offsetY, buffer);
            break;            
        case CRSF_TEXT_SELECTION:
            display.setClipWindow(OLED_X_OFFSET+10, OLED_Y_OFFSET+15, OLED_X_OFFSET+OLED_WIDTH-10, OLED_Y_OFFSET+OLED_HEIGHT-1);
            snprintf(buffer, sizeof(buffer), "%s%s", menuItem.choices[editValue].text, menuItem.units);
            textWidth = getUiTextWidth(buffer);
            centerX = (OLED_WIDTH - textWidth) / 2;
            drawUiStr(OLED_X_OFFSET + centerX + offsetX, OLED_Y_OFFSET + 31 + offsetY, buffer);
            break;   
    }
    display.setMaxClipWindow();
}

// ==================== 编辑值过渡动画 ====================
static uint8_t activeEditAnimation = 0;
static unsigned long editAnimStartTime = 0;
static int prevEditValue = 0;
static int currentEditValue = 0;

void animateEditValue(const ParamCollection& menu) {
    if (activeEditAnimation == 0) {
        drawEditValue(menu.getCurrentParam(), menu.editValue, 0, 0);
    } else {
        unsigned long elapsed = millis() - editAnimStartTime;
        if (elapsed >= ANIM_DURATION) {
            activeEditAnimation = 0;
            drawEditValue(menu.getCurrentParam(), menu.editValue, 0, 0);
        } else {
            long progress = (elapsed * 1000) / ANIM_DURATION; 
            int offsetX_Prev = 0, offsetY_Prev = 0, offsetX_Curr = 0, offsetY_Curr = 0;
            switch (activeEditAnimation) {
                case 1: offsetY_Prev = -(OLED_HEIGHT*progress)/1000; offsetY_Curr = OLED_HEIGHT-(OLED_HEIGHT*progress)/1000; break;
                case 2: offsetY_Prev = (OLED_HEIGHT*progress)/1000;  offsetY_Curr = -OLED_HEIGHT+(OLED_HEIGHT*progress)/1000; break;
                case 3: offsetX_Prev = -(OLED_WIDTH*progress)/1000;  offsetX_Curr = OLED_WIDTH-(OLED_WIDTH*progress)/1000; break;
                case 4: offsetX_Prev = (OLED_WIDTH*progress)/1000;   offsetX_Curr = -OLED_WIDTH+(OLED_WIDTH*progress)/1000; break;
            }
            drawEditValue(menu.getCurrentParam(), prevEditValue, offsetX_Prev, offsetY_Prev);
            drawEditValue(menu.getCurrentParam(), currentEditValue, offsetX_Curr, offsetY_Curr);
        }
    }    
}

// ==================== 弹窗文本滚动（独立状态副本）====================
// 与 drawScrollingMenuValue 逻辑相同但使用独立的 static 变量
// 原因：弹窗覆盖在菜单项之上，两者需要各自独立的滚动相位
void drawScrollingPopupText(const char* text, int textX, int textY, int displayAreaWidth) {
    int textWidth = getUiTextWidth(text);
    unsigned long currentMillis = millis();
    static unsigned long lastResetTime = 0;
    static char lastText[MAX_TEXT_LENGTH] = "";
    enum ScrollState {START, SCROLLING, FINISH};
    static ScrollState currentState = START;

    if (strcmp(text, lastText) != 0) {
        strncpy(lastText, text, sizeof(lastText) - 1);
        lastText[sizeof(lastText) - 1] = '\0';
        currentState = START; lastResetTime = currentMillis;
    }
    if (textWidth <= displayAreaWidth) {
        drawUiStr(textX + (displayAreaWidth - textWidth) / 2, textY, text); return;
    }
    if (currentMillis - lastResetTime >= TOTAL_CYCLE) lastResetTime = currentMillis;
    unsigned long progress = currentMillis - lastResetTime;
    int maxScrollDistance = textWidth - displayAreaWidth;
    int currentXOffset = 0;
    if (progress < STATE_START_PAD) currentXOffset = 0;
    else if (progress < (STATE_START_PAD + STATE_SCROLL))
        currentXOffset = (maxScrollDistance * (progress - STATE_START_PAD)) / STATE_SCROLL;
    else currentXOffset = maxScrollDistance;

    drawUiStr(textX - currentXOffset, textY, text);
}

// ==================== 命令确认/状态弹窗 ====================
void drawPopup(const ParamCollection& menu) {
    if (menu.popupActive()) {
        // 绘制半透明遮罩效果（黑色填充+白色边框）
        display.setDrawColor(0);
        display.drawBox(3+OLED_X_OFFSET, 3+OLED_Y_OFFSET, OLED_WIDTH-6, OLED_HEIGHT-6); 
        display.setDrawColor(1);
        display.drawFrame(3+OLED_X_OFFSET, 3+OLED_Y_OFFSET, OLED_WIDTH-6, OLED_HEIGHT-6);

        display.setClipWindow(5+OLED_X_OFFSET, 5+OLED_Y_OFFSET, OLED_X_OFFSET+OLED_WIDTH-10, OLED_Y_OFFSET+OLED_HEIGHT-10);
        drawScrollingPopupText(menu.getCurrentParam().valueString, OLED_X_OFFSET + 5, OLED_Y_OFFSET + 17, OLED_WIDTH-10);
        
        switch (menu.getCurrentParam().step) {
            case ELRS_COMMAND_EXECUTING:
                drawUiStr(7+OLED_X_OFFSET, OLED_Y_OFFSET+OLED_HEIGHT-12, "[Running...]"); break;
            case ELRS_COMMAND_ASKCONFIRM:
                drawUiStr(10+OLED_X_OFFSET, OLED_Y_OFFSET+OLED_HEIGHT-12, "[Confirm?]"); break;
        }
    }
}

// ==================== 菜单导航逻辑核心 ====================
// direction: 0=无, 1=上, 2=下, 3=左, 4=右, 5=摇杆按下, 6=按键短按, 7=按键长按
// 返回值: -1=退出菜单, 0=正常操作, 1=保存, 2=执行命令
int navigateMenu(ParamCollection &menu, uint8_t direction) {
    uint8_t nextMenuAnimation = 0, nextEditAnimation = 0;
    int returnVal = 0;
    if (direction == 0) return 0;
    
    // 弹窗激活时仅响应确认和返回
    if (menu.popupActive()) {
        if (direction == 5) return 2;  // 确认
        if (direction == 6) return -1; // 关闭弹窗
    }

    switch (menuState) {
        case MENU_BROWSE:
            switch (direction) {
                case 1: nextMenuAnimation = menu.prevInFolder(); break;
                case 2: nextMenuAnimation = menu.nextInFolder(); break;
                case 3: nextMenuAnimation = menu.exitFolder(); break;
                case 4: nextMenuAnimation = menu.enterFolder(); break;
                case 5: // 摇杆按下：命令→执行，其他→进入编辑
                    if (menu.getCurrentParam().type == CRSF_COMMAND) { returnVal = 2; }
                    else if (menu.getCurrentParam().type <= CRSF_STRING) {
                        menu.editParam(); menuState = MENU_EDIT;
                    } break;
                case 6: menu.selectedParam = 0; returnVal = -1; break; // 短按返回
            }
            if (nextMenuAnimation != 0) {
                activeMenuAnimation = nextMenuAnimation;
                menuAnimStartTime = millis();
                prevMenuItemIndex = currentMenuItemIndex;
                currentMenuItemIndex = menu.selectedParam;
            }            
            break;

        case MENU_EDIT:
            switch (direction) {
                case 1: if (menu.getCurrentParam().type != CRSF_TEXT_SELECTION) 
                            nextEditAnimation = menu.editParamInc(); break;
                case 2: if (menu.getCurrentParam().type != CRSF_TEXT_SELECTION) 
                            nextEditAnimation = menu.editParamDec(); break;
                case 3: if (menu.getCurrentParam().type == CRSF_TEXT_SELECTION) 
                            nextEditAnimation = menu.editParamDec(); break;
                case 4: if (menu.getCurrentParam().type == CRSF_TEXT_SELECTION) 
                            nextEditAnimation = menu.editParamInc(); break;
                case 5: returnVal = 1; menuState = MENU_BROWSE; break; // 确认保存
                case 6: menu.editParam(); menuState = MENU_BROWSE; break; // 取消编辑
            }
            if (nextEditAnimation != 0) {
                activeEditAnimation = nextEditAnimation;
                editAnimStartTime = millis();
                prevEditValue = currentEditValue;
                currentEditValue = menu.editValue;
            }       
            break;
    }
    return returnVal;
}

// ==================== 统一输入事件分发器 ====================
void handleNavigation(int navigate, Handset& handset, ELRS& elrs, 
                      bool& toggleBtJoystick, bool& startCalibration) {
    if (navigate == 0) return;

    // 长按(7)：全局快捷操作
    if (navigate == 7) {
        switch (currentScreen) {
            case MAIN_PAGE:       startCalibration = true; break;
            case ELRS_STATS:      currentScreen = ELRS_MENU; break;
            case CHANNEL_OUTPUTS: currentScreen = CHANNEL_MENU; break;
            case BT_JOYSTICK:     toggleBtJoystick = !toggleBtJoystick; break;
            default: break;
        }
    } else {
        // ELRS 菜单导航
        if (currentScreen == ELRS_MENU) {
            int result = navigateMenu(elrs.txModule.params, navigate);
            switch (result) {
                case -1: if (elrs.popup()) elrs.cancelCommand(); 
                         else currentScreen = ELRS_STATS; break;
                case 1:  elrs.editParamSave(); break;
                case 2:  elrs.executeCommand(); break;
            }
        } 
        // 本地通道设置菜单导航
        else if (currentScreen == CHANNEL_MENU) {
            int result = navigateMenu(handset.handsetSettingsMenu, navigate);
            switch (result) {
                case -1: currentScreen = CHANNEL_OUTPUTS; break;
                case 1:  handset.saveGlobalSettings(); break;
                case 2:  handset.executeSettingsCommand(); break;
            }
        } 
        // 非菜单页面：短按切换页面
        else {
            if (navigate == 6) nextScreen();
        }
    }   
}

// ==================== 菜单页面主绘制函数 ====================
void drawMenuScreen(const ParamCollection& menu) {
    const crsfParameter &param = menu.getCurrentParam();
    display.clearBuffer();
    display.setFont(u8g2_font_NokiaSmallPlain_tr);    
    
    switch (menuState) {
        case MENU_EDIT:
            // 编辑模式：顶部标签 + 编辑框 + 方向箭头
            display.setCursor(OLED_X_OFFSET + 3, OLED_Y_OFFSET + 12);
            printUiText(param.label);
            display.drawFrame(OLED_X_OFFSET, OLED_Y_OFFSET + 14, 72, 26);

            // 根据值和类型绘制可调方向箭头
            if (menu.editValue > param.minVal) {   
                if (param.type == CRSF_TEXT_SELECTION) {  
                    display.setDrawColor(0); display.drawBox(OLED_X_OFFSET, OLED_Y_OFFSET+21, 6, 11);
                    display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+5, OLED_Y_OFFSET+22, OLED_X_OFFSET+5, OLED_Y_OFFSET+30, OLED_X_OFFSET, OLED_Y_OFFSET+26);
                } else {                                 
                    display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+35, 11, 6);
                    display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+34, OLED_X_OFFSET+39, OLED_Y_OFFSET+34, OLED_X_OFFSET+35, OLED_Y_OFFSET+39);
                }
            }
            if (menu.editValue < param.maxVal) { 
                if (param.type == CRSF_TEXT_SELECTION) {  
                    display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+66, OLED_Y_OFFSET+21, 6, 11);
                    display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+67, OLED_Y_OFFSET+22, OLED_X_OFFSET+67, OLED_Y_OFFSET+30, OLED_X_OFFSET+72, OLED_Y_OFFSET+26);
                } else {                                  
                    display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+14, 11, 6);
                    display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+19, OLED_X_OFFSET+39, OLED_Y_OFFSET+19, OLED_X_OFFSET+35, OLED_Y_OFFSET+14);
                }
            }
            animateEditValue(menu);
            break;

        default: // MENU_BROWSE
            // 浏览模式：四向导航箭头
            if (param.id > 0) { // 上箭头
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+0, 11, 6);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+5, OLED_X_OFFSET+39, OLED_Y_OFFSET+5, OLED_X_OFFSET+35, OLED_Y_OFFSET+0);
            }
            if (param.id < menu.paramCount) { // 下箭头
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+35, 11, 6);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+36, OLED_X_OFFSET+39, OLED_Y_OFFSET+36, OLED_X_OFFSET+35, OLED_Y_OFFSET+40);
            }    
            if (param.parentFolder > 0) { // 左箭头（返回上级）
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+0, OLED_Y_OFFSET+14, 5, 11);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+4, OLED_Y_OFFSET+15, OLED_X_OFFSET+4, OLED_Y_OFFSET+23, OLED_X_OFFSET+0, OLED_Y_OFFSET+19);
            }    
            if (param.type == CRSF_FOLDER) { // 右箭头（进入子菜单）
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+66, OLED_Y_OFFSET+14, 5, 11);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+67, OLED_Y_OFFSET+15, OLED_X_OFFSET+67, OLED_Y_OFFSET+23, OLED_X_OFFSET+71, OLED_Y_OFFSET+19);
            }
            animateMenu(menu);
            break;
    }
    drawPopup(menu); // 弹窗叠加在最上层
    display.sendBuffer();
}

// ==================== 模型查找器页（RSSI 信号强度）====================
void drawModelFinderScreen(int raw, int avg, int strength) {
    display.clearBuffer();
    display.setDrawColor(1);
    display.drawBox(0 + OLED_X_OFFSET, 15 + OLED_Y_OFFSET, 6, 3); // 负号
    display.setFont(u8g2_font_logisoso28_tn); 
    display.setCursor(8 + OLED_X_OFFSET, 28 + OLED_Y_OFFSET);
    display.print(-raw); // RSSI 为负值，取反显示

    display.setFont(u8g2_font_squeezed_b6_tr); 
    display.setCursor(display.getCursorX()+2, 28 + OLED_Y_OFFSET);
    display.print("dBm");
    
    // 底部信号强度条
    display.drawBox(0 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET, (strength * OLED_WIDTH / 100), 10);
    display.sendBuffer();
}

// ==================== 通用消息页 ====================
void drawMessage(char title[], char line1[], char line2[]) {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_squeezed_b6_tr); 
    int xOffset = (12 - strlen(title)) * 7; // 简易居中对齐
    display.setCursor(xOffset + OLED_X_OFFSET, 8 + OLED_Y_OFFSET);
    printUiText(title);
    
    display.setFont(u8g2_font_nokiafc22_tf);
    if(strlen(line2) > 0) {
        display.setCursor(2 + OLED_X_OFFSET, 22 + OLED_Y_OFFSET); printUiText(line1);
        display.setCursor(2 + OLED_X_OFFSET, 36 + OLED_Y_OFFSET); printUiText(line2);
    } else {
        display.setCursor(2 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET); printUiText(line1);  
    }
}

// ==================== 摇杆校准向导页 ====================
void drawCalibrationScreen(int stage, int secondsLeft) {
    display.clearBuffer();
    display.setDrawColor(1);

    // 72x40 小屏校准页不再显示英文标题；中文状态居中，倒计时独立放在下一行。
    const char* statusText = "准备校准";
    if (stage == 2) statusText = "摇杆回中";
    else if (stage == 3) statusText = "推动摇杆";
    else if (stage == 4) statusText = "校准完成";

    display.setFont(u8g2_font_wqy12_t_gb2312);
    int16_t statusWidth = display.getUTF8Width(statusText);
    int16_t statusX = OLED_X_OFFSET + (72 - statusWidth) / 2;
    int16_t statusY = (stage == 2 || stage == 3) ? 9 : 14;
    display.drawUTF8(statusX, OLED_Y_OFFSET + statusY, statusText);

    if (stage == 2 || stage == 3) {
        char countdown[5];
        snprintf(countdown, sizeof(countdown), "%d", secondsLeft);
        display.setFont(u8g2_font_5x7_tr);
        int16_t countdownWidth = display.getStrWidth(countdown);
        display.setCursor(OLED_X_OFFSET + (72 - countdownWidth) / 2,
                          OLED_Y_OFFSET + 16);
        display.print(countdown);
    }

    // 摇杆框；校准提示位于上方，框内动画保持原样。
    const uint8_t boxSize = 22, leftBoxX = 12, rightBoxX = 40, boxesY = 18;
    display.drawFrame(leftBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    display.drawFrame(rightBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);

    static bool direction = true;
    static float currXPos = 2, currYPos = 2;
    switch (stage) {
        case 1: case 2: case 4:
            display.drawBox(leftBoxX + ((boxSize-2)/2) + OLED_X_OFFSET, boxesY + ((boxSize-2)/2) + OLED_Y_OFFSET, 2, 2);
            display.drawBox(rightBoxX + ((boxSize-2)/2) + OLED_X_OFFSET, boxesY + ((boxSize-2)/2) + OLED_Y_OFFSET, 2, 2);
            break;
        case 3:
            display.drawBox(leftBoxX + (int)currXPos + OLED_X_OFFSET, boxesY + (int)currYPos + OLED_Y_OFFSET, 2, 2);
            display.drawBox(rightBoxX + boxSize + OLED_X_OFFSET - (int)currXPos, boxesY + (int)currYPos + OLED_Y_OFFSET, 2, 2);
            if (direction) {
                currXPos += 0.3; currYPos += 0.3;
                if (currXPos > boxSize - 2) { currXPos -= 0.6; currYPos -= 0.6; direction = false; }
            } else {
                currXPos -= 0.3; currYPos -= 0.3;
                if (currXPos < 2) { currXPos += 0.6; currYPos += 0.6; direction = true; }
            }
            break;
    }
    display.sendBuffer();
}