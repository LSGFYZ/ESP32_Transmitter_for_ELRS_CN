/*
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
 
#include "oled.h"
#include "locale_zh_cn.h"

displayState currentScreen = MAIN_PAGE;
displayState lastScreen;
menuMode menuState = MENU_BROWSE;

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

// 屏幕旋转设置：只支持 U8G2_R0（0°）和 U8G2_R2（180°）。
// 默认保持 R0。需要翻转时，只把下一行改为 U8G2_R2 即可。
// 主页采用整屏刷新，不再使用与旋转方向有关的局部刷新区域。
#ifndef OLED_ROTATION
#define OLED_ROTATION U8G2_R2
#endif

#ifdef OLED_OPTION_1
U8G2_SSD1306_72X40_ER_F_HW_I2C display(OLED_ROTATION, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(OLED_ROTATION, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);
#endif

void initDisplay() {
    // Force I2C initialization to use the board's dedicated hardware pins (SDA=5, SCL=6)
    Wire.begin(5, 6);
    // Start U8g2
    display.begin();
    display.setContrast(255);
    display.setBusClock(800000); // 400kHz fast I2C mode
}

void drawBootLogo() {
    display.clearBuffer();          // Clear the display internal ram memory
    
    // Draw NanoTX logo
    // Parameters: drawXBM(x_start, y_start, width, height, bitmap_array)
    display.drawXBM(OLED_X_OFFSET, OLED_Y_OFFSET, 72, 40, nanotx_logo_72x40);

    display.setFont(u8g2_font_tiny5duo_tf); 
    display.setCursor(OLED_X_OFFSET + 2, OLED_Y_OFFSET + 36);
    display.print("ELRS");
    
    display.setCursor(OLED_X_OFFSET + 50, OLED_Y_OFFSET + 36);
    display.print("v1.3");
    
    display.sendBuffer();     
}

void clearScreen() {
    display.clearBuffer();          // Clear the display internal ram memory
    display.sendBuffer(); 
}

void nextScreen() {
    switch (currentScreen) {
        case MAIN_PAGE:       currentScreen = ELRS_STATS;      break;
        case ELRS_STATS:      currentScreen = CHANNEL_OUTPUTS; break;
        case CHANNEL_OUTPUTS: currentScreen = BT_JOYSTICK;     break;
        case BT_JOYSTICK:     currentScreen = MODEL_FINDER;    break;
        case MODEL_FINDER:    currentScreen = MAIN_PAGE;       break; // Wrap around
    }
}

void drawHomeScreen(float voltage, bool aircraftVoltage, int telemetryState,
                    uint8_t uplinkLQ, int16_t channels[],
                    int16_t minChannelValue, int16_t maxChannelValue) {
    // aircraftVoltage 由主程序用于区分显示的是本机电压还是飞机回传电压。
    // 当前主页只负责显示传入的 voltage 数值，保留该参数以匹配主程序接口。
    (void)aircraftVoltage;

    // 每次从空白画布重新绘制主页，再一次性发送整张画面。
    // 这样 R0/R2 都不依赖局部刷新区域的坐标换算。
    display.clearBuffer();
    display.setDrawColor(1);

    // ==========================================
    // 1. TOP LEFT: BATTERY VOLTAGE
    // ==========================================
    display.setFont(u8g2_font_logisoso16_tf);
    display.setCursor(6 + OLED_X_OFFSET, 16 + OLED_Y_OFFSET);
    display.print(voltage, 1);

    display.setFont(u8g2_font_tiny5duo_tf);
    display.setCursor(display.getCursorX() + 2, 10 + OLED_Y_OFFSET);
    display.print("v");

    // ==========================================
    // 2. TOP RIGHT: LINK / TELEMETRY STATUS
    // ==========================================
    uint8_t bars = 0;
    if (telemetryState == 1) {
        if (uplinkLQ > 75) bars = 4;
        else if (uplinkLQ > 50) bars = 3;
        else if (uplinkLQ > 25) bars = 2;
        else if (uplinkLQ > 0) bars = 1;

        if (bars >= 1) display.drawBox(45 + OLED_X_OFFSET, 11 + OLED_Y_OFFSET, 3, 3);
        if (bars >= 2) display.drawBox(52 + OLED_X_OFFSET, 8 + OLED_Y_OFFSET, 3, 6);
        if (bars >= 3) display.drawBox(59 + OLED_X_OFFSET, 4 + OLED_Y_OFFSET, 3, 10);
        if (bars >= 4) display.drawBox(66 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET, 3, 14);
    } else if (telemetryState == 0) {
        display.drawBox(47 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2);
        display.drawBox(59 + OLED_X_OFFSET, 6 + OLED_Y_OFFSET, 8, 2);
    } else if (telemetryState == 4) {
        display.setFont(u8g2_font_open_iconic_embedded_2x_t);
        display.drawStr(OLED_X_OFFSET + 47, OLED_Y_OFFSET + 16, "J");
    }

    if (telemetryState == 2 || telemetryState == 3) {
        display.drawXBM(45 + OLED_X_OFFSET, 0 + OLED_Y_OFFSET, 8, 10, bt_logo_8x10);
    }

    // ==========================================
    // 3. CENTER: GIMBAL CROSSHAIR BOXES
    // ==========================================
    const uint8_t boxSize = 22;
    const uint8_t leftBoxX = 12;
    const uint8_t rightBoxX = 40;
    const uint8_t boxesY = 18;

    display.drawFrame(leftBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);
    display.drawFrame(rightBoxX + OLED_X_OFFSET, boxesY + OLED_Y_OFFSET, boxSize, boxSize);

    uint8_t leftX = map(channels[0], minChannelValue, maxChannelValue,
                        leftBoxX + 1, leftBoxX + boxSize - 2);
    uint8_t leftY = map(channels[1], minChannelValue, maxChannelValue,
                        boxesY + boxSize - 2, boxesY + 1);
    uint8_t rightX = map(channels[2], minChannelValue, maxChannelValue,
                         rightBoxX + 1, rightBoxX + boxSize - 2);
    uint8_t rightY = map(channels[3], minChannelValue, maxChannelValue,
                         boxesY + boxSize - 2, boxesY + 1);

    display.drawBox(leftX - 1 + OLED_X_OFFSET, leftY - 1 + OLED_Y_OFFSET, 2, 2);
    display.drawBox(rightX - 1 + OLED_X_OFFSET, rightY - 1 + OLED_Y_OFFSET, 2, 2);

    // ==========================================
    // 4. SIDES: AUX SWITCH INDICATORS
    // ==========================================
    display.drawLine(4 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET,
                     4 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux1Y = (channels[4] == 0) ? 36 : 16;
    display.drawBox(2 + OLED_X_OFFSET, aux1Y + OLED_Y_OFFSET, 5, 3);

    display.drawLine(68 + OLED_X_OFFSET, 18 + OLED_Y_OFFSET,
                     68 + OLED_X_OFFSET, 26 + OLED_Y_OFFSET);
    uint8_t aux2Y = (channels[5] == 0) ? 24 : 16;
    display.drawBox(66 + OLED_X_OFFSET, aux2Y + OLED_Y_OFFSET, 5, 3);

    display.drawLine(68 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET,
                     68 + OLED_X_OFFSET, 38 + OLED_Y_OFFSET);
    uint8_t aux3Y = (channels[6] == 0) ? 36 : 28;
    display.drawBox(66 + OLED_X_OFFSET, aux3Y + OLED_Y_OFFSET, 5, 3);

    // 主页唯一一次刷新：R0/R2 均发送完整 framebuffer。
    display.sendBuffer();
    lastScreen = currentScreen;
}

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

 //----- Menu text animation timing configuration
const unsigned long STATE_START_PAD  = 1000; 
const unsigned long STATE_SCROLL     = 1000;  
const unsigned long STATE_END_PAD    = 500;  
const unsigned long TOTAL_CYCLE      = STATE_START_PAD + STATE_SCROLL + STATE_END_PAD;
const int MAX_TEXT_LENGTH            = 64; // Theoretically ELRS could have longer strings, but not likely

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


// Draw a menu item within a given window (top left coordinates and h/w set with x/y/h/w)
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


// Animation configuration
// 0 = No animation
// 1 = Animate Up
// 2 = Animate Down
// 3 = Animate Left
// 4 = Animate Right
static uint8_t activeMenuAnimation = 0;   // Stores the current running animation type (1-4)
static unsigned long menuAnimStartTime = 0;
const unsigned long ANIM_DURATION = 500; // Transition duration in milliseconds
// Menu Item tracking
static int prevMenuItemIndex = 0;   // Track where we came from
static int currentMenuItemIndex = 0;// Track where we are going

void animateMenu(const ParamCollection& menu) {
    if (activeMenuAnimation == 0) {
        // Standard State: No active transition requested, render static layout normally
        drawMenuItem(menu.getCurrentParam(), 0, 0);
    } 
    else {
        // Animation Active: Calculate step frame interpolation
        unsigned long elapsed = millis() - menuAnimStartTime;
        
        if (elapsed >= ANIM_DURATION) {
            // Animation complete: clean state tracking parameters
            activeMenuAnimation = 0;
            drawMenuItem(menu.getCurrentParam(), 0, 0);
        } 
        else {
            // Linear Progress mapping percentage (0.0 -> 1.0 represented out of 1000)
            long progress = (elapsed * 1000) / ANIM_DURATION; 
            
            int offsetX_Prev = 0, offsetY_Prev = 0;
            int offsetX_Curr = 0, offsetY_Curr = 0;

            // Compute precise Pixel offsets based on target translation paths
            switch (activeMenuAnimation) {
                case 1: // Animate Up (New item enters from bottom)
                    offsetY_Prev = - (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = OLED_HEIGHT - (OLED_HEIGHT * progress) / 1000;
                    break;
                case 2: // Animate Down (New item enters from top)
                    offsetY_Prev = (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = -OLED_HEIGHT + (OLED_HEIGHT * progress) / 1000;
                    break;
                case 3: // Animate Left (New folder enters from right)
                    offsetX_Prev = - (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = OLED_WIDTH - (OLED_WIDTH * progress) / 1000;
                    break;
                case 4: // Animate Right (New folder enters from left)
                    offsetX_Prev = (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = -OLED_WIDTH + (OLED_WIDTH * progress) / 1000;
                    break;
            }

            // Render both states into the active full-buffer layout sequentially
            drawMenuItem(menu.getParam(prevMenuItemIndex), offsetX_Prev, offsetY_Prev);
            drawMenuItem(menu.getParam(currentMenuItemIndex), offsetX_Curr, offsetY_Curr);
        }
    }    
}


// Draw a menu item within an edit window (offset coordinates specify where in the window it is drawn)
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

// Animation configuration
// 0 = No animation
// 1 = Animate Up
// 2 = Animate Down
// 3 = Animate Left
// 4 = Animate Right
static uint8_t activeEditAnimation = 0;   // Stores the current running animation type (1-4)
static unsigned long editAnimStartTime = 0;
// Edit Value tracking
static int prevEditValue = 0;   // Track where we came from
static int currentEditValue = 0;// Track where we are going
void animateEditValue(const ParamCollection& menu) {
    if (activeEditAnimation == 0) {
        // Standard State: No active transition requested, render static layout normally
        drawEditValue(menu.getCurrentParam(), menu.editValue, 0, 0);
    } 
    else {
        // Animation Active: Calculate step frame interpolation
        unsigned long elapsed = millis() - editAnimStartTime;
        
        if (elapsed >= ANIM_DURATION) {
            // Animation complete: clean state tracking parameters
            activeEditAnimation = 0;
            drawEditValue(menu.getCurrentParam(), menu.editValue, 0, 0);
        } 
        else {
            // Linear Progress mapping percentage (0.0 -> 1.0 represented out of 1000)
            long progress = (elapsed * 1000) / ANIM_DURATION; 
            
            int offsetX_Prev = 0, offsetY_Prev = 0;
            int offsetX_Curr = 0, offsetY_Curr = 0;

            // Compute precise Pixel offsets based on target translation paths
            switch (activeEditAnimation) {
                case 1: // Animate Up (New value enters from bottom)
                    offsetY_Prev = - (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = OLED_HEIGHT - (OLED_HEIGHT * progress) / 1000;
                    break;
                case 2: // Animate Down (New value enters from top)
                    offsetY_Prev = (OLED_HEIGHT * progress) / 1000;
                    offsetY_Curr = -OLED_HEIGHT + (OLED_HEIGHT * progress) / 1000;
                    break;
                case 3: // Animate Left (New value enters from right)
                    offsetX_Prev = - (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = OLED_WIDTH - (OLED_WIDTH * progress) / 1000;
                    break;
                case 4: // Animate Right (New value enters from left)
                    offsetX_Prev = (OLED_WIDTH * progress) / 1000;
                    offsetX_Curr = -OLED_WIDTH + (OLED_WIDTH * progress) / 1000;
                    break;
            }

            // Render both states into the active full-buffer layout sequentially
            drawEditValue(menu.getCurrentParam(), prevEditValue, offsetX_Prev, offsetY_Prev);
            drawEditValue(menu.getCurrentParam(), currentEditValue, offsetX_Curr, offsetY_Curr);
        }
    }    
}

// Copy of the previous drawScrollingMenuValue - since the popup may be getting displayed over such a menu item we need to track this state separately
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


// uint8_t direction
// 0 = No move/button
// 1 = Up
// 2 = Down
// 3 = Left
// 4 = Right
// 5 = Joystick button
// 6 = Built in button (short press)
// 7 = Built in button (long press)
// return value
// -1: Exit menu (back to prev screen)
//  0: normal operation (editing, or navigating menu)
//  1: Save menu item
//  2: Execute command
int navigateMenu(ParamCollection &menu, uint8_t direction) {
    uint8_t nextMenuAnimation;
    uint8_t nextEditAnimation;
    int returnVal = 0;
    if (direction == 0) return 0;
    if (menu.popupActive()) {
        switch (direction) {
            case 5:
                return 2;  // User confirmed a popup prompt
                break;   
            case 6:
                return -1; // User pressed back to close the popup
                break;   
        }
    }

    switch (menuState) {
        case MENU_BROWSE:
            nextMenuAnimation = 0;
            switch (direction) {
                case 1:
                    nextMenuAnimation = menu.prevInFolder();
                    break;
                case 2:
                    nextMenuAnimation = menu.nextInFolder();
                    break;
                case 3:
                    nextMenuAnimation = menu.exitFolder();
                    break;
                case 4:
                    nextMenuAnimation = menu.enterFolder();
                    break;
                case 5:
                    switch (menu.getCurrentParam().type) {
                        case CRSF_COMMAND:
                            returnVal = 2;
                            break;
                        case CRSF_UINT8:
                        case CRSF_INT8:
                        case CRSF_UINT16:
                        case CRSF_INT16:
                        case CRSF_UINT32:
                        case CRSF_INT32:
                        case CRSF_UINT64:
                        case CRSF_INT64:
                        case CRSF_FLOAT:
                        case CRSF_TEXT_SELECTION:
                        case CRSF_STRING:
                            menu.editParam();
                            menuState = MENU_EDIT;  // Change to edit mode
                            break;
                        default:
                            // Do nothing
                            break;   
                    }
                    break;
                case 6:
                    menu.selectedParam = 0;
                    returnVal = -1;  // Exit out of menu
                    break;
                // case 7:Long press not handled in menu
            }
            // If the joystick movement resulted in an animation
            if (nextMenuAnimation != 0) {
                activeMenuAnimation = nextMenuAnimation;
                menuAnimStartTime = millis(); // Start the timer clock
                
                // Capture current menu item index (selectedParam) before updating the active item
                prevMenuItemIndex = currentMenuItemIndex;
                currentMenuItemIndex = menu.selectedParam;
            }            
            break;


        case MENU_EDIT:
            nextEditAnimation = 0;
            switch (direction) {
                case 1:
                    // Numeric/non-text selection types edited with up/down
                    if (menu.getCurrentParam().type != CRSF_TEXT_SELECTION) nextEditAnimation = menu.editParamInc();
                    break;
                case 2:
                    if (menu.getCurrentParam().type != CRSF_TEXT_SELECTION) nextEditAnimation = menu.editParamDec();
                    break;
                case 3:
                    // Text Selection type is edited with left/right
                    if (menu.getCurrentParam().type == CRSF_TEXT_SELECTION) nextEditAnimation = menu.editParamDec();
                    break;
                case 4:
                    if (menu.getCurrentParam().type == CRSF_TEXT_SELECTION) nextEditAnimation = menu.editParamInc();
                    break;
                case 5:
                    returnVal = 1;  // Indicate that we need to save the current item
                    menuState = MENU_BROWSE;
                    //menu.editParamSave();  // Save the menu (this function will also exit back to browse mode)
                    break;
                case 6:
                    menu.editParam(); // reset editValue to the param's current value
                    menuState = MENU_BROWSE;  // Go back to browse mode
                    break;
            }
            // If the joystick movement resulted in an animation
            if (nextEditAnimation != 0) {
                activeEditAnimation = nextEditAnimation;
                editAnimStartTime = millis(); // Start the timer clock
                
                // Capture current menu item index (selectedParam) before updating the active item
                prevEditValue = currentEditValue;
                currentEditValue = menu.editValue;
            }       
            break;
    }

    return returnVal;
}


void handleNavigation(int navigate, Handset& handset, ELRS& elrs, bool& toggleBtJoystick, bool& startCalibration ) {
    // 0 = No move/button
    // 1 = Up
    // 2 = Down
    // 3 = Left
    // 4 = Right
    // 5 = Joystick button
    // 6 = Built in button (short press)
    // 7 = Built in button (long press)
    if (navigate ==0) return;

    if (navigate == 7) {
        switch (currentScreen) {
            case MAIN_PAGE:
                startCalibration = true;
                break;
            case ELRS_STATS:
                currentScreen = ELRS_MENU;
                break;
            case CHANNEL_OUTPUTS: 
                currentScreen = CHANNEL_MENU;
                break;
            case BT_JOYSTICK: 
                toggleBtJoystick = !toggleBtJoystick;
                //currentScreen = MAIN_PAGE;
                break;
            // Ignore long press builtin button in ELRS_MENU & CHANNEL_MENU & MODEL_FINDER
        }
    } else {
        if (currentScreen == ELRS_MENU) {
            int result = navigateMenu(elrs.txModule.params, navigate);
            switch (result) {
                case -1: // Back
                    if (elrs.popup()) {
                        elrs.cancelCommand();
                    } 
                    else {
                        currentScreen = ELRS_STATS;  // Exit out of menu
                    }
                    break;
                case 1: // Save
                    elrs.editParamSave();
                    break;
                case 2:  // Execute Command
                    elrs.executeCommand();
                    break;
                default: // Nothing to do
                    break;
            }
        } else if (currentScreen == CHANNEL_MENU) {
            int result = navigateMenu(handset.handsetSettingsMenu, navigate);
            switch (result) {
                case -1:
                    // Not using popups in channel menu, so no need to check if popupActive()
                    currentScreen = CHANNEL_OUTPUTS;  // Exit out of menu
                    break;
                case 1:
                    handset.saveGlobalSettings();
                    break;
                case 2:  // Execute Command
                    handset.executeSettingsCommand();
                    break;                        
                default: // Nothing to do
                    break;
            }
        } else {
            if (navigate == 6) {
                nextScreen();  // Cycle screens when not in a menu
            }
        }
    }   
}


void drawMenuScreen(const ParamCollection& menu) {
    const crsfParameter &param = menu.getCurrentParam();
    display.clearBuffer();
    display.setFont(u8g2_font_NokiaSmallPlain_tr);    
    
    switch (menuState) {
        case MENU_EDIT:
            // 编辑模式：顶部标签 + 编辑框 + 方向箭头
            // 先绘制边框，再绘制标签，避免边框覆盖较高的中文字体字形
            display.drawFrame(OLED_X_OFFSET, OLED_Y_OFFSET + 14, 72, 26);
            // 参数编辑页左上角使用原始英文标签（如 THR / EXPO），不经过中文翻译和大字体切换。
            // 这样保留原版短标注，避免中文字体/基线处理造成残缺字符。
            display.drawStr(OLED_X_OFFSET + 3, OLED_Y_OFFSET + 12, param.label);

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
            // 恢复原版四向导航箭头：上/下切换项目，左返回，右进入文件夹。
            if (param.id > 0) { // 上箭头
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+0, 11, 6);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+5, OLED_X_OFFSET+39, OLED_Y_OFFSET+5, OLED_X_OFFSET+35, OLED_Y_OFFSET+0);
            }
            if (param.id < menu.paramCount) { // 下箭头
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+30, OLED_Y_OFFSET+35, 11, 6);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+31, OLED_Y_OFFSET+36, OLED_X_OFFSET+39, OLED_Y_OFFSET+36, OLED_X_OFFSET+35, OLED_Y_OFFSET+40);
            }
            if (param.parentFolder > 0) { // 左箭头：返回上级
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+0, OLED_Y_OFFSET+14, 5, 11);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+4, OLED_Y_OFFSET+15, OLED_X_OFFSET+4, OLED_Y_OFFSET+23, OLED_X_OFFSET+0, OLED_Y_OFFSET+19);
            }
            if (param.type == CRSF_FOLDER) { // 右箭头：进入子菜单
                display.setDrawColor(0); display.drawBox(OLED_X_OFFSET+66, OLED_Y_OFFSET+14, 5, 11);
                display.setDrawColor(1); display.drawTriangle(OLED_X_OFFSET+67, OLED_Y_OFFSET+15, OLED_X_OFFSET+67, OLED_Y_OFFSET+23, OLED_X_OFFSET+71, OLED_Y_OFFSET+19);
            }
            animateMenu(menu);
            break;
    }
    drawPopup(menu); // 弹窗叠加在最上层
    display.sendBuffer();
}


void drawModelFinderScreen(int raw, int avg, int strength) {
    // U8g2 full frame buffer drawing process
    display.clearBuffer();

    display.setDrawColor(1);
    // all values are negative (-120 ~ -20) so draw a little negative symbal (instead of full font width)
    display.drawBox(0 + OLED_X_OFFSET, 15 + OLED_Y_OFFSET, 6, 3);
    display.setFont(u8g2_font_logisoso28_tn); 
    display.setCursor(8 + OLED_X_OFFSET, 28 + OLED_Y_OFFSET);
    display.print(-raw);

    display.setFont(u8g2_font_squeezed_b6_tr); 
    display.setCursor(display.getCursorX()+2, 28 + OLED_Y_OFFSET);
    display.print("dBm");
    
    display.drawBox(0 + OLED_X_OFFSET, 30 + OLED_Y_OFFSET, (strength * OLED_WIDTH / 100), 10);

    display.sendBuffer();
}


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




