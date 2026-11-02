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
 
#include "crsf.h"

// ==================== CRSF CRC8 校验实现 ====================
// 查表法 CRC8，来源于 CRSF 协议文档 rev7
// 多项式: 0xD5 (x^8 + x^7 + x^6 + x^4 + x^2 + 1)
static uint8_t crsf_crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9};

// 计算 CRC8 校验值
// ptr: 数据指针 | len: 数据长度
uint8_t crsf_crc8(const uint8_t *ptr, uint8_t len) {
    uint8_t crc = 0; // CRC 初始值为 0
    for (uint8_t i = 0; i < len; i++){
        crc = crsf_crc8tab[crc ^ *ptr++]; // 查表法逐字节累积 CRC
    }
    return crc;
}

// 验证接收到的数据包 CRC 是否正确
// packet: 完整数据包（含帧头）| length: 总长度
bool checkCrc(uint8_t *packet, uint8_t length) {
    // CRC 计算范围：从 type 字段开始到 payload 结束（跳过 addr 和 len）
    // 比较计算结果与包尾的 CRC 字节
    return (crsf_crc8(&packet[2], packet[1] - 1) == packet[length - 1]);
}

// ==================== 串口初始化 ====================
void CRSF::begin(uint8_t rxPin, uint8_t txPin, bool halfDuplex) {
  isHalfDuplex = halfDuplex;
  if (isHalfDuplex) {
    // 半双工模式：TX 和 RX 共用同一引脚
    // 921600 baud, SERIAL_8N1, RX=txPin, TX=txPin（收发同脚）
    ELRS_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, txPin, txPin); 
    // 配置 GPIO 为开漏输出模式 + 上拉电阻（半双工总线必须）
    pinMode(txPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(txPin, HIGH); // 拉高总线（空闲状态）
  } else {
    // 全双工模式：TX 和 RX 使用独立引脚
    // 921600 baud, SERIAL_8N1, RX=GPIO20, TX=GPIO21
    ELRS_PORT.begin(SERIAL_BAUDRATE, SERIAL_8N1, rxPin, txPin); 
  }
}

// ==================== 发送 16 通道数据 ====================
// 将 16 个 int16 通道值打包为 CRSF 通道帧
// CRSF 通道编码：每个通道 11bit，范围 172-1811（中点 992）
// 16 通道 × 11bit = 176bit = 22 字节载荷
void CRSF::crsfSendChannels(int16_t channels[]) {
    uint8_t packet[CRSF_PACKET_SIZE]; // 26 字节完整帧缓冲

    // --- 帧头构建 ---
    packet[0] = MODULE_ADDRESS;       // 目标设备地址：ELRS 高频头 (0xEE)
    packet[1] = 24;                   // 帧长度 = type(1) + payload(22) + crc(1) = 24
    packet[2] = ELRS_CHANNELS;        // 帧类型：通道数据 (0x16)

    // --- 16 通道 11bit 位域压缩打包 ---
    // 每个通道取低 11 位 (& 0x07FF)，然后按 bit 紧密排列
    // CH0: bits [10:0] → packet[3] 全部 + packet[4] 低3位
    packet[3] = (uint8_t)(channels[0] & 0x07FF);
    packet[4] = (uint8_t)((channels[0] & 0x07FF) >> 8 | (channels[1] & 0x07FF) << 3);
    // CH1: bits [10:0] → packet[4] 高5位 + packet[5] 低6位
    packet[5] = (uint8_t)((channels[1] & 0x07FF) >> 5 | (channels[2] & 0x07FF) << 6);
    // CH2: bits [10:0] → packet[5] 高2位 + packet[6] 全部 + packet[7] 低1位
    packet[6] = (uint8_t)((channels[2] & 0x07FF) >> 2);
    packet[7] = (uint8_t)((channels[2] & 0x07FF) >> 10 | (channels[3] & 0x07FF) << 1);
    // CH3: bits [10:0] → packet[7] 高7位 + packet[8] 低4位
    packet[8] = (uint8_t)((channels[3] & 0x07FF) >> 7 | (channels[4] & 0x07FF) << 4);
    // CH4: bits [10:0] → packet[8] 高4位 + packet[9] 低7位
    packet[9] = (uint8_t)((channels[4] & 0x07FF) >> 4 | (channels[5] & 0x07FF) << 7);
    // CH5: bits [10:0] → packet[9] 高1位 + packet[10] 全部 + packet[11] 低2位
    packet[10] = (uint8_t)((channels[5] & 0x07FF) >> 1);
    packet[11] = (uint8_t)((channels[5] & 0x07FF) >> 9 | (channels[6] & 0x07FF) << 2);
    // CH6: bits [10:0] → packet[11] 高6位 + packet[12] 低5位
    packet[12] = (uint8_t)((channels[6] & 0x07FF) >> 6 | (channels[7] & 0x07FF) << 5);
    // CH7: bits [10:0] → packet[12] 高3位 + packet[13] 全部
    packet[13] = (uint8_t)((channels[7] & 0x07FF) >> 3);
    // CH8-CH15: 同样的 11bit 打包模式重复
    packet[14] = (uint8_t)((channels[8] & 0x07FF));
    packet[15] = (uint8_t)((channels[8] & 0x07FF) >> 8 | (channels[9] & 0x07FF) << 3);
    packet[16] = (uint8_t)((channels[9] & 0x07FF) >> 5 | (channels[10] & 0x07FF) << 6);
    packet[17] = (uint8_t)((channels[10] & 0x07FF) >> 2);
    packet[18] = (uint8_t)((channels[10] & 0x07FF) >> 10 | (channels[11] & 0x07FF) << 1);
    packet[19] = (uint8_t)((channels[11] & 0x07FF) >> 7 | (channels[12] & 0x07FF) << 4);
    packet[20] = (uint8_t)((channels[12] & 0x07FF) >> 4 | (channels[13] & 0x07FF) << 7);
    packet[21] = (uint8_t)((channels[13] & 0x07FF) >> 1);
    packet[22] = (uint8_t)((channels[13] & 0x07FF) >> 9 | (channels[14] & 0x07FF) << 2);
    packet[23] = (uint8_t)((channels[14] & 0x07FF) >> 6 | (channels[15] & 0x07FF) << 5);
    packet[24] = (uint8_t)((channels[15] & 0x07FF) >> 3);

    // --- CRC 校验 ---
    packet[25] = crsf_crc8(&packet[2], packet[1] - 1); // 对 type+payload 计算 CRC
    
    // 直接写入串口（通道数据不走队列，保证最低延迟）
    crsfWritePacket(packet, CRSF_PACKET_SIZE);
}

// ==================== 发送 ELRS 设置命令 ====================
// 将命令加入发送队列（非阻塞），由主循环定时取出发送
void CRSF::crsfSendCommand(uint8_t command, uint8_t value) {
    uint8_t packetCmd[CRSF_CMD_PACKET_SIZE]; // 8 字节命令帧

    packetCmd[0] = MODULE_ADDRESS;            // 目标：ELRS 高频头
    packetCmd[1] = 6;                         // 帧长度 = type(1) + payload(4) + crc(1) = 6
    packetCmd[2] = ELRS_SETTINGS_WRITE;       // 帧类型：设置写入 (0x2D)
    packetCmd[3] = MODULE_ADDRESS;            // 目标地址（参数广播请求时用 0x00）
    packetCmd[4] = HANDSET_ADDRESS;           // 源地址：手台 (0xEA)
    packetCmd[5] = command;                   // 命令码（如 PKT_RATE, TLM_RATIO 等）
    packetCmd[6] = value;                     // 命令参数值
    packetCmd[7] = crsf_crc8(&packetCmd[2], packetCmd[1] - 1); // CRC

    // 注意：命令不直接发送，而是入队等待调度，避免与通道数据冲突
    crsfQueuePacket(packetCmd, CRSF_CMD_PACKET_SIZE);
}

// ==================== 从队列发送下一条待处理命令 ====================
void CRSF::crsfSendQueuedCommand() {
    if (commandQueue.hasItems()) {
        crsfPacket pendingPacket;
        if (commandQueue.pop(pendingPacket)) {
            crsfWritePacket(pendingPacket.data, pendingPacket.length);
            // [调试代码已注释] 打印发送的命令和数据
        }
    }
}

// ==================== 将数据包加入发送队列 ====================
void CRSF::crsfQueuePacket(uint8_t packet[], uint8_t packetLength) {
    // 在入队前重新计算 CRC（确保数据完整性）
    packet[packetLength-1] = crsf_crc8(&packet[2], packetLength-3); 
    // 推入环形缓冲队列
    if (!commandQueue.push(packet, packetLength)) {
        // 队列满时静默丢弃（可在此添加错误处理）
    }
}

// ==================== 底层串口写入（含半双工回显清除）====================
void CRSF::crsfWritePacket(uint8_t packet[], uint8_t packetLength) {
    ELRS_PORT.write(packet, packetLength);
    if (isHalfDuplex) {
      // ⚠️ 半双工关键操作：必须 flush 等待发送完成
      // 否则后续读取会收到自己发出的数据（回显），导致解析错乱
      ELRS_PORT.flush(); 
      // 清除串口缓冲区中的回显数据
      while (ELRS_PORT.available() > 0) {
        ELRS_PORT.read(); // 丢弃所有回显字节
      }   
    } 
}

// ==================== 解析链路统计遥测包 ====================
// 帧类型: ELRS_LINK_STATS (0x14)
void CRSF::crsfParseLinkStatsPacket(uint8_t rxBuffer[]) {
  // 首次收到遥测时标记激活状态
  if (!telemetryActive) {
      telemetryActive = true;
  #ifdef CRSFDEBUG
      Serial.println("\n=================================");
      Serial.println("[INFO] -> TELEMETRY RECOVERED <-");
      Serial.println("=================================");
  #endif
  }
  lastLinkStatsFrameTime = millis(); // 刷新遥测活跃时间戳

  // 按 CRSF Link Statistics 协议解析各字段
  linkStats.uplinkRssi1   = (int8_t)rxBuffer[3];  // 天线1 RSSI (负dBm)
  linkStats.uplinkRssi2   = (int8_t)rxBuffer[4];  // 天线2 RSSI (负dBm)
  linkStats.uplinkLQ      = rxBuffer[5];           // 上行链路质量 (0-100%)
  linkStats.uplinkSNR     = (int8_t)rxBuffer[6];   // 上行信噪比 (dB)
  linkStats.activeAntenna = rxBuffer[7];           // 当前天线 (0/1)
  linkStats.rfMode        = rxBuffer[8];           // 空中速率索引
  linkStats.txPower       = rxBuffer[9];           // 发射功率等级
  linkStats.downlinkRssi  = (int8_t)rxBuffer[10];  // 下行遥测 RSSI
  linkStats.downlinkLQ    = rxBuffer[11];          // 下行链路质量
  linkStats.downlinkSNR   = (int8_t)rxBuffer[12];  // 下行信噪比
}

// ==================== 解析 ELRS 同步包（CRSFShot 时序对齐）====================
// 帧类型: ELRS_RADIO_ID (0x3A)，子类型: CRSF_SUBTYPE_OPENTX_SYNC (0x10)
// 用于让手台精确对齐 ELRS 的帧发送时序，减少抖动
void CRSF::crsfParseElrsSyncPacket(uint8_t rxBuffer[]) {
    // 验证包长度和子类型标识
    // rxBuffer[0]=同步字节 [1]=长度 [2]=类型0x3A [3]=源0xEE [4]=目标0xEF
    if (rxBuffer[1] >= 11 && rxBuffer[5] == CRSF_SUBTYPE_OPENTX_SYNC) {      
        // 提取 32 位大端格式的速率间隔（载荷字节 1-4）
        uint32_t rawInterval = ((uint32_t)rxBuffer[6] << 24) | 
                               ((uint32_t)rxBuffer[7] << 16) | 
                               ((uint32_t)rxBuffer[8] << 8)  | 
                                rxBuffer[9];
        // 提取 32 位有符号大端格式的相位偏移（载荷字节 5-8）
        int32_t rawShift = ((int32_t)rxBuffer[10] << 24) | 
                           ((int32_t)rxBuffer[11] << 16) | 
                           ((int32_t)rxBuffer[12] << 8)  | 
                            rxBuffer[13];
        // ExpressLRS 将微秒级遥测值放大了 10 倍传输，需除以 10 还原
        targetIntervalUs   = (uint32_t)((float)rawInterval * 0.1f);
        currentPhaseShift  = (int32_t)((float)rawShift * 0.1f);
        // 限制相位偏移不超过一个周期
        currentPhaseShift = constrain(currentPhaseShift, -(int32_t)targetIntervalUs, (int32_t)targetIntervalUs);

        // 相位偏移归一化：超过半周期的偏移转换为下一周期的等效偏移
        // 例：2000us 间隔下，+1650us 实际等价于下一周期的 -350us
        if (currentPhaseShift > (int32_t)(targetIntervalUs/2)) currentPhaseShift -= (int32_t)targetIntervalUs;
        // 反之：-1800us 实际等价于下一周期的 +200us
        else if (-currentPhaseShift > (int32_t)(targetIntervalUs/2)) currentPhaseShift += (int32_t)targetIntervalUs;
        
        syncPacketReceived = true;      
        lastSyncPacketTime = micros(); // 记录同步包到达时间

        // 累积相位偏移用于计算滑动平均值
        phaseShiftHistory[syncPacketCount++] = currentPhaseShift;
        if (syncPacketCount==0) cumulativePhaseShift=0;   // uint8_t 溢出回绕时重置累积值
        else cumulativePhaseShift += currentPhaseShift;

        // 每秒更新一次平均相位偏移并输出调试信息
        if (millis()>lastSyncPacketDisplay+1000) {
          averagePhaseShift = (syncPacketCount == 0) ? 0 : cumulativePhaseShift / (int16_t)syncPacketCount;
#ifdef CRSFDEBUG        
          Serial.println("\n============ [CRSFShot] ============");
          Serial.print("Received "); Serial.print(syncPacketCount); Serial.println(" packets in 1000ms");
          Serial.print("Last Sync Packet. Interval: ");Serial.print(targetIntervalUs);Serial.print(" Phase Shift: ");Serial.println(currentPhaseShift);
          Serial.print("Average shift: "); Serial.println(averagePhaseShift);
          Serial.print("Past packets : ");
          for (int i = 0; i < syncPacketCount; i++){
            Serial.print(phaseShiftHistory[i]);Serial.print(" "); 
          } Serial.println();
          Serial.println("====================================");
#endif
          // 重置统计数据，开始下一秒的采样
          memset(phaseShiftHistory, 0, sizeof(phaseShiftHistory));   
          cumulativePhaseShift = 0;
          syncPacketCount = 0;
          lastSyncPacketDisplay = millis();
        }
    }
}

// ==================== 获取下一帧发送间隔 ====================
// 返回动态调整后的帧间隔(us)，用于主循环精确定时
uint32_t CRSF::crsfNextInterval() {
        // 基础间隔：优先使用 ELRS 同步包报告的值，否则用硬编码默认值
        uint32_t baseInterval = (targetIntervalUs > 0) ? targetIntervalUs : CRSF_TIME_BETWEEN_FRAMES_US;
        
        // TODO: 基于 CRSFShot 同步实现动态相位校正
        // 当前修正量为 0（预留接口）
        int32_t correction = 0;
        
        // 返回下一次循环的动态间隔
        return baseInterval + correction;
}

// ==================== 遥测数据接收与解析状态机 ====================
// 在主循环中持续调用，非阻塞设计
void CRSF::crsfReadTelemetry() {
    // --- 阶段1: 从串口读取并组装数据包 ---
    while (ELRS_PORT.available() > 0) {
        // 快速过滤：接受手台(0xEA)、Lua脚本(0xEF)以及标准 CRSF 帧地址/同步字节(0xC8)
        // 飞控电池传感器遥测帧常以 0xC8 开头，否则会在这里被提前丢弃。
        if (ELRS_PORT.peek() != HANDSET_ADDRESS &&
            ELRS_PORT.peek() != LUA_SCRIPT_ADDRESS &&
            ELRS_PORT.peek() != SYNC_BYTE) {
            ELRS_PORT.read(); // 立即丢弃无效/失步字节
            continue;
        }

        if (ELRS_PORT.available() < 2) return;    // 至少需要2字节才能读取长度
        uint8_t addrByte = ELRS_PORT.read();      // 读取地址字节
        uint8_t payloadLength = ELRS_PORT.read(); // 读取长度字节

        // 长度合法性检查
        if (payloadLength < 2 || payloadLength > (CRSF_MAX_PACKET_SIZE - 2)) {
            continue; // 异常长度，继续冲刷缓冲区
        }

        // 非阻塞超时等待：等待剩余字节到达
        uint32_t startWait = micros();
        while (ELRS_PORT.available() < payloadLength) {
            if (micros() - startWait > 1000) { // 1ms 超时上限
                return; // 组包超时或碎片化，丢弃当前帧片段
            }
        }

        // 组装完整数据包到本地缓冲
        uint8_t localPacketBuffer[CRSF_MAX_PACKET_SIZE];
        localPacketBuffer[0] = addrByte;
        localPacketBuffer[1] = payloadLength;
        for (uint8_t i = 0; i < payloadLength; i++) {
            localPacketBuffer[2 + i] = ELRS_PORT.read();
        }
        uint8_t totalPacketBytesCount = payloadLength + 2; // 总字节数 = addr + len + payload

        // --- 阶段2: CRC 校验与分发 ---
        if (checkCrc(localPacketBuffer, totalPacketBytesCount)) {
          // 收到有效帧 → 刷新看门狗计时器
          moduleConnected = true;
          lastValidFrameTime = millis();
          
          uint8_t packetType = localPacketBuffer[2];
          switch (packetType) {
            case ELRS_LINK_STATS:
              // 链路统计：立即解析，不入队（实时性要求高）
              crsfParseLinkStatsPacket(localPacketBuffer);
              break;
            case ELRS_RADIO_ID:
              // 同步包：立即解析，不入队（时序敏感）
              crsfParseElrsSyncPacket(localPacketBuffer);
              break;
            case 0x08: // CRSF 电池传感器帧：电压为 16 位大端值，单位 0.1V
              // 帧布局：[地址][长度][类型][电压高字节][电压低字节]...[CRC]
              if (payloadLength >= 10) {
                uint16_t rawVoltage = ((uint16_t)localPacketBuffer[3] << 8) |
                                      (uint16_t)localPacketBuffer[4];
                aircraftVoltage = rawVoltage / 10.0f;
                aircraftBatteryValid = true;
                lastAircraftBatteryTime = millis();
              }
              break;
            default:
              // 其他遥测类型（如设置响应、设备信息等）入队等待上层处理
              telemetryQueue.push(localPacketBuffer, totalPacketBytesCount);
              break;
          }    
        }
    }

    // --- 阶段3: 超时检测与状态管理 ---
    uint32_t now = millis();
    
    // 遥测丢失检测：超过 1 秒未收到链路统计帧
    if (telemetryActive && (now - lastLinkStatsFrameTime > 1000)) {
      telemetryActive = false;
#ifdef CRSFDEBUG
      Serial.println("\n=================================");
      Serial.println("[ALERT] !!! TELEMETRY LOST !!!");
      Serial.println("=================================");
#endif
    }
    
    // 模块断连看门狗：超过 2 秒无任何有效帧 → 标记断连并重置同步
    if (moduleConnected && (now - lastValidFrameTime > 2000)) {
        moduleConnected = false;
        syncPacketReceived = false;
        Serial.println("[HANDSHAKE] Telemetry Timeout. Module connection Lost.");
    }   
}