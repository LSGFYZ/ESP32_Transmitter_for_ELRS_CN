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
 
#include "elrs.h"
#include "crsf.h" // 引入 CRSF 底层通信类，编译器需要知道其方法签名

// 通过初始化列表绑定 CRSF 实例引用
ELRS::ELRS(CRSF& crsfInstance) : crsf(crsfInstance) {}

// ==================== 命令构建函数 ====================

// 发送 ELRS 设置写入命令（通用）
void ELRS::sendCommand(uint8_t command, uint8_t value) {
    uint8_t packetCmd[CRSF_CMD_PACKET_SIZE]; // 8字节命令帧缓冲

    packetCmd[0] = MODULE_ADDRESS;            // 目标设备：ELRS高频头 (0xEE)
    packetCmd[1] = 6;                         // 帧长度 = type(1) + payload(4) + crc(1)
    packetCmd[2] = ELRS_SETTINGS_WRITE;       // 帧类型：设置写入 (0x2D)
    packetCmd[3] = MODULE_ADDRESS;            // 目标地址（0x00=广播请求时使用）
    packetCmd[4] = HANDSET_ADDRESS;           // 源地址：手台 (0xEA)
    packetCmd[5] = command;                   // 命令码/参数索引
    packetCmd[6] = value;                     // 命令值
    packetCmd[7] = 0;                         // CRC占位，由队列函数自动计算

    crsf.crsfQueuePacket(packetCmd, CRSF_CMD_PACKET_SIZE); // 入队等待发送
#ifdef ELRSDEBUG        
    Serial.print("[ELRS] -> Send Command: ");
    Serial.print(command);
    Serial.print(", Value: ");
    Serial.print(value); Serial.println();
#endif
}

// 发送设备探测包（发现在线的ELRS模块）
void ELRS::pingDevices() {
    uint8_t packetCmd[6];

    packetCmd[0] = MODULE_ADDRESS;            // 目标：ELRS高频头
    packetCmd[1] = 4;                         // 帧长度 = type(1) + payload(2) + crc(1)
    packetCmd[2] = ELRS_DEVICE_PING;          // 帧类型：设备探测 (0x28)
    packetCmd[3] = BROADCAST_ADDRESS;         // 目标地址：广播 (0x00)
    packetCmd[4] = HANDSET_ADDRESS;           // 源地址：手台 (0xEA)
    packetCmd[5] = 0;                         // CRC占位

    crsf.crsfQueuePacket(packetCmd, 6);       // 入队发送
}

// 请求读取指定参数的指定数据块（支持分块传输）
void ELRS::requestSetting(uint8_t settingIndex, uint8_t chunk) {
    uint8_t packetCmd[8];

    packetCmd[0] = MODULE_ADDRESS;            // 目标：ELRS高频头 (0xEE)
    packetCmd[1] = 6;                         // 帧长度 = type(1) + payload(4) + crc(1)
    packetCmd[2] = ELRS_SETTINGS_READ;        // 帧类型：参数读取 (0x2C)
    packetCmd[3] = MODULE_ADDRESS;            // 目标地址：模块自身 (0xEE)
    packetCmd[4] = LUA_SCRIPT_ADDRESS;        // 源地址：Lua脚本 (0xEF)
    packetCmd[5] = settingIndex;              // 参数索引 (0, 1, 2...)
    packetCmd[6] = chunk;                     // 数据块偏移编号
    packetCmd[7] = 0;                         // CRC占位

    crsf.crsfQueuePacket(packetCmd, 8);       // 入队发送
#ifdef ELRSDEBUG        
    Serial.print("[ELRS] -> Querying Menu Parameter Index: ");
    Serial.print(settingIndex);
    Serial.print(", chunk: ");
    Serial.print(chunk); Serial.println();
#endif
}

// 请求ELRS运行状态信息（发送 param=0, value=0 的特殊写入命令）
void ELRS::requestElrsStatus() {
    sendCommand(0, 0);
}


// ==================== 响应包解析函数 ====================

// 解析设备信息包 (0x29)：提取模块名称、序列号、版本号、参数总数
void ELRS::parseDeviceInfoPacket(uint8_t rxBuffer[], uint8_t length) {
  // 清除旧的模块名称缓存
  memset(txModule.name, 0, sizeof(txModule.name)); 

  uint8_t currentIdx = 5;   // 从载荷第3字节开始（跳过addr+len+type+dest+src）
  uint8_t charCount = 0;
  
  // 提取以null结尾的模块名称字符串
  while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length - 1) && charCount < (CRSF_MAX_STRING_LEN - 1)) {
    txModule.name[charCount++] = (char)rxBuffer[currentIdx++];
  }
  txModule.name[charCount] = '\0';
  currentIdx++; // 跳过null终止符

  // 验证剩余数据是否足够容纳14字节的固定字段
  if (currentIdx + 14 <= (length + 2)) {
    // 提取32位大端格式字段
    txModule.serialNumber[0] = rxBuffer[currentIdx++];
    txModule.serialNumber[1] = rxBuffer[currentIdx++];
    txModule.serialNumber[2] = rxBuffer[currentIdx++];
    txModule.serialNumber[3] = rxBuffer[currentIdx++];

    txModule.hwVersion[0] = rxBuffer[currentIdx++];
    txModule.hwVersion[1] = rxBuffer[currentIdx++];
    txModule.hwVersion[2] = rxBuffer[currentIdx++];
    txModule.hwVersion[3] = rxBuffer[currentIdx++];
    
    txModule.swVersion[0] = rxBuffer[currentIdx++];
    txModule.swVersion[1] = rxBuffer[currentIdx++];
    txModule.swVersion[2] = rxBuffer[currentIdx++];
    txModule.swVersion[3] = rxBuffer[currentIdx++];

    // 提取配置结构控制标签
    totalSettingsCount = rxBuffer[currentIdx++];    // 参数总数
    txModule.protocolVersion = rxBuffer[currentIdx++]; // CRSF协议版本

    // 重置已加载参数计数
    txModule.params.paramCount = 0;
#ifdef ELRSDEBUG        
    Serial.println("============ [ELRS DEVICE DETECTED] ============");
    Serial.print("Module Name         : "); Serial.println(txModule.name);
    Serial.print("Serial number       : "); 
    Serial.print((char)txModule.serialNumber[0]); Serial.print((char)txModule.serialNumber[1]); 
    Serial.print((char)txModule.serialNumber[2]); Serial.println((char)txModule.serialNumber[3]); 
    Serial.print("Hardware Version    : "); 
    Serial.print(txModule.hwVersion[0], HEX); Serial.print(txModule.hwVersion[1], HEX); 
    Serial.print(txModule.hwVersion[2], HEX); Serial.println(txModule.hwVersion[3], HEX); 
    Serial.print("Firmware Version    : "); 
    Serial.print(txModule.swVersion[0], HEX); Serial.print(txModule.swVersion[1], HEX); 
    Serial.print(txModule.swVersion[2], HEX); Serial.println(txModule.swVersion[3], HEX); 
    Serial.print("Available Settings  : "); Serial.print(totalSettingsCount); Serial.println(" Menu Items");
    Serial.print("Protocol Version    : "); Serial.println(txModule.protocolVersion);
    Serial.println("================================================\n");
#endif
  }
}

// 解析ELRS状态包 (0x2E)：提取链路质量、连接状态、告警消息
void ELRS::parseElrsStatusPacket(uint8_t rxBuffer[], uint8_t length) {
    // rxBuffer[3]=目标地址, rxBuffer[4]=源地址
    // 丢弃非ELRS模块发出的状态包
    if (rxBuffer[4] != MODULE_ADDRESS) {
#ifdef ELRSDEBUG  
    Serial.print("\nUnexpected Origin Address in Link Stats Packet (0x2E):    0x"); 
    Serial.println(rxBuffer[4], HEX);
#endif
        return;
    }
    elrsStatus.packetsBad  = rxBuffer[5];                              // 坏包计数
    elrsStatus.packetsGood = ((uint16_t)rxBuffer[6] << 8) | rxBuffer[7]; // 好包计数(大端)
    uint8_t rawFlags = rxBuffer[8];                                    // 状态标志位
    elrsStatus.isConnected   = (rawFlags & 0x01) != 0;                 // Bit0: 接收机已连接
    elrsStatus.modelMismatch = (rawFlags & 0x04) != 0;                 // Bit2: 模型匹配失败
    elrsStatus.isArmed       = (rawFlags & 0x08) != 0;                 // Bit3: 已解锁
    
    // 提取以null结尾的状态消息字符串
    memset(elrsStatus.statusMessage, 0, sizeof(elrsStatus.statusMessage));
    uint8_t stringStartIdx = 9;
    uint8_t charCounter = 0;
    while (rxBuffer[stringStartIdx] != 0x00 && stringStartIdx < (length + 2) && charCounter < 31) {
        elrsStatus.statusMessage[charCounter++] = (char)rxBuffer[stringStartIdx++];
    }
    elrsStatus.statusMessage[charCounter] = '\0';

    // 更新菜单项0的显示文本（复用为菜单标题栏，实时显示好包/坏包比）
    snprintf(txModule.params[0].choices[0].text, sizeof(txModule.params[0].choices[0].text), 
             "Pkts: %u/%u", elrsStatus.packetsGood, elrsStatus.packetsBad);

#ifdef ELRSDEBUG        
    Serial.println("\n============ [ELRS LINK STATUS 0x2E] ============");
    Serial.print("Pkt Stream Health : Good: "); Serial.print(elrsStatus.packetsGood);
    Serial.print(" | Bad: "); Serial.println(elrsStatus.packetsBad);
    Serial.print("Link State Flags  : ");
    if (elrsStatus.isConnected)   Serial.print("[CONNECTED] ");   
    else Serial.print("[DISCONNECTED] ");
    if (elrsStatus.isArmed)       Serial.print("[ARMED] ");       
    else Serial.print("[DISARMED] ");
    if (elrsStatus.modelMismatch) Serial.print("[!!! MODEL MISMATCH !!!] ");
    Serial.println();
    if (strlen(elrsStatus.statusMessage) > 0) {
        Serial.print("Module Alert Msg  : "); Serial.println(elrsStatus.statusMessage);
    }
    Serial.println("=================================================");
#endif
}

// 解析参数信息包 (0x2B)：处理分块传输重组
// ⚠️ 核心难点：ELRS大参数（如Pitmode 22选项）需拆成多块传输，此函数负责重组
void ELRS::parseParameterInfoPacket(uint8_t rxBuffer[], uint8_t length) {
    // 静态缓冲区：跨调用保持状态，用于拼接多个数据块
    static uint8_t tempParamStorage[CRSF_PAYLOAD_SIZE_MAX * 5]; // 最多5块×60字节
    static uint16_t tempStorageIndex = 0;      // 当前已拼接的字节数
    static uint8_t expectedChunksRemaining = 0; // 预期剩余块数
    
    uint8_t fieldIndex = rxBuffer[5]; // 参数索引
    
    // 如果收到的包不是当前正在等待的参数 → 重置重组状态
    if (fieldIndex != currentSettingsIndex) {
        memset(tempParamStorage, 0, sizeof(tempParamStorage));
        tempStorageIndex = 0; 
        currentChunk = 0;
        return;
    }

    int slot = txModule.params.getParamSlot(fieldIndex);
    chunksRemaining = rxBuffer[6]; // 剩余块数
    
    // 安全检查：槽位无效 或 剩余块数不连续 → 丢弃
    if (slot == -1 || (tempStorageIndex > 0 && chunksRemaining != expectedChunksRemaining)) {
        return;
    }

    if (currentChunk == 0) {
        // 第一块：清空缓冲并复制完整包（含CRC）
        memset(tempParamStorage, 0, sizeof(tempParamStorage));
        memcpy(tempParamStorage, rxBuffer, length);
        tempStorageIndex = length;
    } else {
        // 后续块：追加到缓冲区（覆盖前一块的CRC位置）
        if (sizeof(tempParamStorage) > tempStorageIndex + (length - 7)) {
            size_t offset = tempStorageIndex - 1; // 回退1字节覆盖前一块CRC
            memcpy(tempParamStorage + offset, rxBuffer + 7, length - 7); // 跳过帧头7字节
            tempStorageIndex += length - 8;       // 净增字节数（少1因为覆盖了CRC）
        } else {
            // 缓冲区溢出保护：放弃当前参数，重置状态
            expectedChunksRemaining = 0;
            currentChunk = 0;
            currentSettingsIndex = -1;
            return;
        }
    } 
    
    // 还有剩余块 → 继续等待下一块
    if (chunksRemaining > 0) {
        currentChunk++;    	
        expectedChunksRemaining = chunksRemaining - 1;
    } else {
        // 所有块已收齐 → 调用完整参数解析器
        parseParameter(tempParamStorage, tempStorageIndex);
        
        // 重置重组状态
        tempStorageIndex = 0;
        currentChunk = 0;
        currentSettingsIndex = -1;
    }
    
    lastParameterQueryTime = 0; // 立即触发下一个参数查询（不等待500ms超时）
}

// 解析完整的参数数据（重组后调用）
// 根据参数类型(UINT8/TEXT_SELECTION/FOLDER/INFO/COMMAND)分别解析并存入ParamCollection
void ELRS::parseParameter(uint8_t rxBuffer[], uint8_t length) {
    uint8_t currentIdx = 5;
    uint8_t fieldIndex = rxBuffer[currentIdx++]; // 参数索引

    int slot = txModule.params.getParamSlot(fieldIndex);
    if (slot != -1) {
        // ===== 特殊处理：索引0（根菜单标题）=====
        // ELRS返回的是名为"HooJ"的文件夹，我们将其复用为配置菜单标题
        if (fieldIndex == 0) {
            txModule.params[0].parentFolder = 0;
            txModule.params[0].type = CRSF_TEXT_SELECTION;
            strcpy(txModule.params[0].label, "ELRS Config");
            snprintf(txModule.params[0].choices[0].text, sizeof(txModule.params[0].choices[0].text), 
                     "Pkts: %u/%u", elrsStatus.packetsGood, elrsStatus.packetsBad);
            txModule.params[0].currentVal = 0; 
            return;     
        }

        currentIdx++; // 跳过chunks remaining字节
        txModule.params[slot].parentFolder = rxBuffer[currentIdx++];               // 父级菜单ID
        txModule.params[slot].type = static_cast<crsfValueType>(rxBuffer[currentIdx] & 0x7F); // 参数类型(低7位)
        txModule.params[slot].hidden = (rxBuffer[currentIdx++] & 0x80);            // 隐藏标志(最高位)
        
        // 解析参数标签字符串（从索引9开始）
        uint8_t labelCharCount = 0;
        currentIdx = 9;
        while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && labelCharCount < (CRSF_MAX_STRING_LEN - 1)) {
          txModule.params[slot].label[labelCharCount++] = (char)rxBuffer[currentIdx++];
        }
        txModule.params[slot].label[labelCharCount] = '\0'; 
        currentIdx++; // 跳过标签null终止符
         
        uint8_t unitCharCount = 0;
        uint8_t choiceCharCount = 0;
        uint8_t valueStringCharCount = 0;
        
        // ===== 按参数类型分别解析 =====
        switch(txModule.params[slot].type) {
            case CRSF_UINT8:
                // UINT8数值型（如VTX Channel）
                txModule.params[slot].currentVal = (int32_t)rxBuffer[currentIdx++];
                txModule.params[slot].minVal = (int32_t)rxBuffer[currentIdx++];
                txModule.params[slot].maxVal = (int32_t)rxBuffer[currentIdx++];
                currentIdx++; // 跳过一个未知的0x00字节
                // 解析单位字符串
                unitCharCount = 0;
                while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length + 2) && unitCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                    txModule.params[slot].units[unitCharCount++] = (char)rxBuffer[currentIdx++];
                }
                txModule.params[slot].units[unitCharCount] = '\0';  
#ifdef ELRSDEBUG        
                Serial.print("[PARAMETER SAVED] ");
                Serial.print(txModule.params[slot].label);
                Serial.print(" ("); Serial.print(txModule.params[slot].minVal); 
                Serial.print("-"); Serial.print(txModule.params[slot].maxVal); Serial.print("): ");
                Serial.print(txModule.params[slot].currentVal); Serial.println(txModule.params[slot].units);
#endif
                break;
                
            case CRSF_TEXT_SELECTION:
                // 下拉选择型（ELRS最常用的参数类型）
                // 选项以分号';'分隔，整体以null结尾
                txModule.params[slot].choicesCount = 0;
                while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && txModule.params[slot].choicesCount < CRSF_MAX_PARAMS) {
                  char c = (char)rxBuffer[currentIdx++];
                  if (c == ';') {
                      // 遇到分号 → 封存当前选项字符串
                      txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount] = '\0';
                      txModule.params[slot].choicesCount++;
                      choiceCharCount = 0;
                  } else if (choiceCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                      txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount++] = c;
                  }
                }
                // 封存最后一个选项
                if (txModule.params[slot].choicesCount <= CRSF_MAX_PARAMS) {
                    txModule.params[slot].choices[txModule.params[slot].choicesCount].text[choiceCharCount] = '\0';
                    txModule.params[slot].choicesCount++;
                }
                
                // 选项字符串null之后依次为：当前值、最小值、最大值、步长(保留)、单位字符串
                currentIdx++; // 跳过选项字符串的null终止符
                if (currentIdx < (length + 2)) txModule.params[slot].currentVal = rxBuffer[currentIdx];
                currentIdx++;
                if (currentIdx < (length + 2)) txModule.params[slot].minVal = rxBuffer[currentIdx];
                currentIdx++;
                if (currentIdx < (length + 2)) txModule.params[slot].maxVal = rxBuffer[currentIdx];
                currentIdx++; // 步长（所有已知情况均为0）
                currentIdx++; // 跳过步长后的分隔符
                // 解析单位字符串
                unitCharCount = 0;
                while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length + 2) && unitCharCount < (CRSF_MAX_STRING_LEN - 1)) {
                    txModule.params[slot].units[unitCharCount++] = (char)rxBuffer[currentIdx++];
                }
                txModule.params[slot].units[unitCharCount] = '\0';      
#ifdef ELRSDEBUG        
                Serial.print("[PARAMETER SAVED] ");
                Serial.print(txModule.params[slot].label);
                Serial.print(" | Choices("); Serial.print(txModule.params[slot].minVal); 
                Serial.print("-"); Serial.print(txModule.params[slot].maxVal); Serial.print("): ");
                for (int i = 0; i < txModule.params[slot].choicesCount; i++) {
                    Serial.print(txModule.params[slot].choices[i].text); 
                    if (i < txModule.params[slot].choicesCount - 1) Serial.print(", ");
                } 
                Serial.print(" | Active Selection: "); 
                Serial.print(txModule.params[slot].choices[txModule.params[slot].currentVal].text);
                Serial.println(txModule.params[slot].units);
#endif
                break;
                
            case CRSF_FOLDER:
                // 文件夹类型（子菜单容器）
#ifdef ELRSDEBUG        
                Serial.print("[PARAMETER SAVED] ");
                Serial.print(txModule.params[slot].label);
                Serial.print(" | Menu (ID: "); Serial.print(txModule.params[slot].id); Serial.println(")");
#endif
                break;
                
            case CRSF_INFO:
                // 只读信息字符串（如固件版本、Bad/Good计数）
                while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
                  txModule.params[slot].valueString[valueStringCharCount++] = (char)rxBuffer[currentIdx++];
                }
                txModule.params[slot].valueString[valueStringCharCount] = '\0';
#ifdef ELRSDEBUG        
                Serial.print("[PARAMETER SAVED] ");
                Serial.print(txModule.params[slot].label);
                Serial.print(" | Info String: "); Serial.println(txModule.params[slot].valueString);
#endif
                break;
                
            case CRSF_COMMAND:
                // 命令类型（Bind/WiFi/BLE等异步操作）
                // 载荷结构：step(1) + timeout(1) + status_string(null terminated)
                txModule.params[slot].step = rxBuffer[currentIdx++];         // 命令执行步骤
                txModule.params[slot].timeout = rxBuffer[currentIdx++] * 10; // 超时(ms)，原始值×10
                
                // 提取状态/信息字符串
                while (rxBuffer[currentIdx] != 0x00 && currentIdx < (length-1) && valueStringCharCount < (CRSF_MAX_PARAM_DATA_LEN - 1)) {
                  txModule.params[slot].valueString[valueStringCharCount++] = (char)rxBuffer[currentIdx++];
                }
                txModule.params[slot].valueString[valueStringCharCount] = '\0';

                // 如果命令处于执行中或等待确认状态 → 设置弹窗轮询定时器
                if (txModule.params[slot].step > ELRS_COMMAND_IDLE && ready()) {
                    nextPopupPollingTime = millis() + txModule.params[slot].timeout;
                }
#ifdef ELRSDEBUG        
                Serial.print("[PARAMETER SAVED] ");
                Serial.print(txModule.params[slot].label);
                Serial.print(" | Command (Current state "); 
                switch(txModule.params[slot].step) {
                  case 0: Serial.print("IDLE"); break;
                  case 1: Serial.print("CLICK"); break;
                  case 2: Serial.print("EXECUTING"); break;
                  case 3: Serial.print("ASKCONFIRM"); break;
                  case 4: Serial.print("CONFIRMED"); break;
                  case 5: Serial.print("CANCEL"); break;
                  case 6: Serial.print("QUERY"); break;
                }
                Serial.print(" | Timeout "); Serial.print(txModule.params[slot].timeout); 
                Serial.print(" | Info/Status : "); Serial.print(txModule.params[slot].valueString); 
                Serial.println(")");
#endif
                break;
                
            default:
                // 规范中定义但ELRS未使用的类型(String/Float/Int)
#ifdef ELRSDEBUG        
                Serial.print("Unhanded info packet type. Packet data: ");
                for (int i = 0; i < length; i++){
                  Serial.print(rxBuffer[i],HEX); Serial.print(" "); 
                } Serial.println();
#endif
                break;
        } // Switch
    } // If Slot!=-1
}


// ==================== 工具函数 ====================

// 清除模块信息和参数缓存（断连时调用）
void ELRS::clearModule() {
    memset(txModule.name, 0, sizeof(txModule.name));   
    txModule.params.clearParams();
    txModule.params.paramCount = 0;
    txModule.paramsLoaded = false;
    moduleInfoReceived = false;
}

// 保存编辑中的参数值到高频头
void ELRS::editParamSave() {
    // 发送写入命令并入队
    sendCommand(txModule.params.selectedParam, txModule.params.editValue);
    // 立即更新本地缓存值（不等回传确认，提升UI响应速度）
    txModule.params[txModule.params.selectedParam].currentVal = txModule.params.editValue;
    
    // 刷新当前参数及其同级兄弟参数和父级文件夹
    loadOneParam(txModule.params.selectedParam);
    int parentFolder = txModule.params.getCurrentParam().parentFolder;
    if (parentFolder > 0) {
        for (uint8_t i = parentFolder + 1; i < txModule.params.paramCount; i++) {
            if (i != txModule.params.selectedParam && 
                txModule.params[i].parentFolder == parentFolder && 
                !txModule.params[i].hidden &&
                txModule.params[i].type != CRSF_FOLDER &&
                txModule.params[i].type != CRSF_COMMAND) {
                loadOneParam(i); // 刷新同级可见的非文件夹/非命令参数
            }
        }
        loadOneParam(parentFolder); // 刷新父级文件夹
    }

    lastParameterQueryTime = millis();
    lastHandshakeTime += 100; // 微调握手时间避免与参数查询冲突
}

// 执行命令或确认弹窗中的命令
void ELRS::executeCommand() {
    if (popup()) {
        // 弹窗模式下：发送确认指令
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CONFIRMED);
    } else if (ready() && txModule.params.getCurrentParam().type == CRSF_COMMAND) {
        // 菜单模式下：发送点击指令触发命令
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CLICK); 
    }
}

// 取消命令或关闭弹窗
void ELRS::cancelCommand() {
    if (popup()) {
        currentSettingsIndex = txModule.params.selectedParam;
        sendCommand(currentSettingsIndex, ELRS_COMMAND_CANCEL);
    }
}

// 启动全量参数加载（倒序压栈使弹出顺序为正序）
void ELRS::loadAllParams(uint8_t totalCount) {
    loadQueue.clear();
    for (int i = totalCount; i >= 0; i--) {
        loadQueue.push(i);
    }
    connectionState = ELRS_LOAD_PARAMS; 
}

// 将单个参数加入加载队列（后台增量刷新）
void ELRS::loadOneParam(uint8_t paramIndex) {
    if (loadQueue.push(paramIndex)) {
        // 如果当前处于READY状态，强制切回LOAD_PARAMS以处理新请求
        if (connectionState == ELRS_READY) {
            connectionState = ELRS_LOAD_PARAMS;
        }
    }
}

// ==================== 主驱动函数（在loop中周期调用）====================
void ELRS::update(bool menuActive) {
    // --- 阶段1: 处理入站遥测包 ---
    if (crsf.telemetryQueue.hasItems()) {
      crsfPacket telemetryPacket;
      if (crsf.telemetryQueue.pop(telemetryPacket)) {
        uint8_t packetType = telemetryPacket.data[2];
#ifdef ELRSDEBUG        
        Serial.print("Received Packet type 0x"), Serial.println(packetType, HEX);
#endif
        switch (packetType) {
          case ELRS_DEVICE_INFO:       // 0x29 设备信息
            moduleInfoReceived = true;
            parseDeviceInfoPacket(telemetryPacket.data, telemetryPacket.length);     
            break;           
          case ELRS_SETTINGS_RESPONSE: // 0x2B 参数响应（含分块数据）
            parseParameterInfoPacket(telemetryPacket.data, telemetryPacket.length);
            break;
          case ELRS_STATUS:            // 0x2E 运行状态
            parseElrsStatusPacket(telemetryPacket.data, telemetryPacket.length);
            break;
        }
      }
    }

    // --- 阶段2: 连接状态机驱动 ---
    uint32_t now = millis();

    switch(connectionState) {
        case ELRS_BOOT_DELAY:
            // 等待3秒让ELRS模块硬件完成上电初始化
            if (millis() >= 3000) {
                connectionState = ELRS_PINGING;
            }
            break;

        case ELRS_PINGING:
            // 每1秒发送一次设备探测包
            if (now - lastHandshakeTime > 1000) {
                lastHandshakeTime = now;
                pingDevices();
#ifdef ELRSDEBUG        
                Serial.println("[ELRS] Sending Device Ping (0x28)...");
#endif
            }
            
            // 转换条件：收到设备信息包且成功解析出模块名称
            if (strlen(txModule.name) > 0) {
                txModule.paramsLoaded = false;
                loadAllParams(totalSettingsCount);     // 启动全量参数加载
                connectionState = ELRS_LOAD_PARAMS;
                currentChunk = 0;      
                lastParameterQueryTime = now;
                lastHandshakeTime = now;
                Serial.println("[ELRS] Module Detected! Getting Stats/Initializing param discovery...");
                requestElrsStatus();                   // 同时请求运行状态
            }
            break;

        case ELRS_LOAD_PARAMS:
            // 每1秒请求一次运行状态（加载期间保持心跳）
            if (now - lastHandshakeTime > 1000) {
                lastHandshakeTime = now;
                requestElrsStatus();
#ifdef ELRSDEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            // 每500ms查询一个参数（或重试/获取下一块）
            if (now - lastParameterQueryTime > 500) {
                if (currentSettingsIndex != -1) {
                    // 有未完成的参数 → 重试或获取下一块
                    requestSetting(currentSettingsIndex, currentChunk);
                    lastParameterQueryTime = now;
                } else {
                    // 从队列取下一个参数
                    if (!loadQueue.isEmpty()) {
                        currentSettingsIndex = loadQueue.pop();
                        currentChunk = 0;
                        requestSetting(currentSettingsIndex, currentChunk);
                        lastParameterQueryTime = now;
                    } else {
                        // 队列为空 → 全部加载完成
                        txModule.paramsLoaded = true;
                        Serial.println("[ELRS] >>> PARAMETER TREE MATRIX FULLY POPULATED AND CACHED <<<");
                        connectionState = ELRS_READY;
                    }
                }
            }
            break;

        case ELRS_READY:
            // 菜单激活时每1秒刷新运行状态
            if (menuActive && now - lastHandshakeTime > 1000) {
                requestElrsStatus();
                lastHandshakeTime = now;
#ifdef ELRSDEBUG        
                Serial.println("[ELRS] Sending LinkStat Request (0x2D, 0, 0)...");
#endif
            }
            // 弹窗激活时定期轮询命令执行状态
            if (popup() && now > nextPopupPollingTime) {
                currentSettingsIndex = txModule.params.selectedParam;
                sendCommand(currentSettingsIndex, ELRS_COMMAND_QUERY);
                // 下次轮询时间 = 当前时间 + 参数超时值（默认1秒）
                nextPopupPollingTime = now + (txModule.params.getCurrentParam().timeout > 0 ? 
                                             txModule.params.getCurrentParam().timeout : 1000);
#ifdef ELRSDEBUG        
                Serial.print("[ELRS] Polling command status (0x2D, "); 
                Serial.print(currentSettingsIndex, HEX); Serial.print(", "); 
                Serial.print(ELRS_COMMAND_QUERY, HEX); Serial.println(")...");
#endif
            }            
            break;
    }

    // --- 阶段3: 断连检测与恢复 ---
    // CRSF层检测到模块断连 → 清除缓存并回退到PINGING状态重新发现
    if (!crsf.moduleConnected && (connectionState > ELRS_PINGING)) {
        loadQueue.clear();
        clearModule();
        connectionState = ELRS_PINGING;
        Serial.println("[ELRS] Module connection Lost.");
    }        
}