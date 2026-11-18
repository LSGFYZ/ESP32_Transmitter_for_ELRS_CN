# ESP32 Transmitter for ELRS — 简体中文增强版

当前版本：**CN1.3** · 更新日期：2026-11-18

这是基于 ESP32-C3 Nano TX 项目继续汉化和功能扩展的 ExpressLRS 遥控器发射机固件。项目保留上游工程的硬件设计与核心功能，并在此基础上持续改进中文界面、遥测显示、通道控制和混控功能。

## 项目来源与致谢

本项目沿着以下开源项目逐步发展而来：

1. **最初项目 — Kkbin505：Simple TX**
   - 项目：[kkbin505/Arduino-Transmitter-for-ELRS](https://github.com/kkbin505/Arduino-Transmitter-for-ELRS)
   - Kkbin505 最初开发了基于 Arduino 的 Simple TX 遥控器项目，包含 ELRS 外置发射模块控制相关代码、硬件设计及外壳文件，是这条项目路线的起点。

2. **第二阶段 — dbloemhard：ESP32 C3 Nano TX**
   - 项目：[dbloemhard/ESP32_Transmitter_for_ELRS](https://github.com/dbloemhard/ESP32_Transmitter_for_ELRS)
   - dbloemhard 在 Simple TX 的基础上进行了大量重写和移植，使其运行在 ESP32-C3 模块上，并加入 OLED 屏幕、ELRS 模块遥测读取以及 ELRS Lua 配置脚本等功能。
   - 该项目 README 也明确注明其工作基于 Kkbin505 的 SimpleTX。

3. **第三阶段 — LSGFYZ：简体中文增强版**
   - 当前项目：[LSGFYZ/ESP32_Transmitter_for_ELRS_CN](https://github.com/LSGFYZ/ESP32_Transmitter_for_ELRS_CN)
   - 本项目以 dbloemhard 的 ESP32-C3 版本为基础，继续进行简体中文汉化、功能扩展、显示修复和使用体验优化。
   - 本项目的新增修改属于在上游成果基础上的再开发，并非从零开始重新设计；在此向 Kkbin505 和 dbloemhard 两位原作者致谢。

## 版本说明

| 版本 | 主要内容 |
| --- | --- |
| 上游基础版本 | Kkbin505 的 Arduino Simple TX，以及 dbloemhard 后续的 ESP32-C3 OLED 版本 |
| **CN1.2** | 完成主要中文汉化，加入接收机电池电压遥测回传与 OLED 显示，并持续完善通道控制和界面功能 |
| **CN1.3（当前）** | 在 CN1.2 基础上继续修复显示问题、优化刷新体验，并加入双电机差速混控 |

## 功能与改进历程

### 第一阶段：简体中文汉化

- 将遥控器 OLED 菜单和常用选项进行简体中文汉化。
- 统一常用菜单、状态和参数的中文显示。
- 尽量只处理 OLED 显示层，不改变 CRSF/ELRS 协议数据及设备内部设置键名，避免影响原有配置和通信。

### 第二阶段：接收机电池电压回传

- 增加接收机电池电压遥测回传功能。
- 通过 CRSF 电池传感器数据（0x08）接收电压信息，并在遥控器 OLED 界面显示。
- 增加遥测超时处理：一段时间未收到有效电压数据时，显示 -.-，避免继续显示过期数值。

### 第三阶段：通道控制方式扩展

- 为第五通道（AUX1）增加可选控制方式。
- 除原有的按键点击切换方式外，还可以使用两段式滑动开关直接控制高、低电平状态。
- 这样既保留按键切换的使用习惯，也支持实体滑动开关与通道状态直接对应。具体使用方式取决于固件中的控制模式配置。

### 第四阶段：显示问题修复与界面优化

- 修正部分菜单文字、参数及显示异常，改善中文界面的可读性。
- 修复 OLED 画面旋转 180° 后出现的显示异常。
- 修复参数编辑页面左上角 THR、EXPO 等短英文标签被错误处理后出现乱码或异常字符的问题，短标签保留原始英文绘制。
- 恢复菜单浏览页面的方向导航箭头，同时保留参数编辑页面的调节箭头和边框。

### 第五阶段：显示刷新率优化

- 优化通道输出页面的刷新调度。
- Channel Output 页面刷新周期调整为约 33ms，目标约 30 FPS，使通道数值变化显示得更加及时、流畅。
- 刷新调度与主页显示节奏保持一致；实际帧率仍会受到主循环执行和 OLED 通信耗时影响。

### 第六阶段：双电机差速混控（CN1.3）

新增 Differential 差速混控，适用于履带车、差速车等左右两侧独立驱动的双电机车辆。

进入遥控器菜单 **Mixing（混控）** 后，可以分别设置：

| 菜单项 | 功能 |
| --- | --- |
| Mix Mode | Off：关闭；Differential：差速混控；Custom：预留选项，当前尚未实现 |
| Mix Thr In | 油门输入通道，可选 CH1–CH12 |
| Mix Steer In | 转向输入通道，可选 CH1–CH12 |
| Mix Left Out | 左电机输出通道，可选 CH1–CH12 |
| Mix Right Out | 右电机输出通道，可选 CH1–CH12 |
| Mix Thr Rate | 油门比例，0–100% |
| Mix Steer Rate | 转向比例，-100% 至 +100% |
| Mix Spin | 是否允许左右电机反向输出，实现原地旋转 |

差速计算方式：

- 左电机输出 = 油门 + 转向
- 右电机输出 = 油门 − 转向

混控结果会限制在 CRSF 通道有效输出范围内。

#### 只使用 CH1、CH2 控制左右电机

如果只想使用两个通道，可以这样设置：

| Mixing 设置 | 选择 |
| --- | --- |
| Mix Mode | Differential |
| Mix Thr In | CH1 |
| Mix Steer In | CH2 |
| Mix Left Out | CH1 |
| Mix Right Out | CH2 |

程序会先读取输入通道数据，再计算并写入左右电机输出，因此输入通道和输出通道可以重合。左右电机输出通道必须选择不同通道。

**注意：** 当前源码默认油门输入为 CH3、转向输入为 CH4，左右输出为 CH1、CH2。若要按上面的方式只使用 CH1、CH2，需要在 Mixing 菜单中手动调整。油门正反转行为还需要结合 **Center Throt** 设置；首次测试请架空驱动轮，确认电机方向和转向逻辑后再实际行驶。

## 当前版本状态

- 当前版本：**CN1.3**
- 差速混控：项目使用者已在实际设备上测试，并反馈功能正常。
- OLED 标签、菜单箭头及通道输出刷新修改：已合并至主分支；不同屏幕和硬件组合仍建议实机确认。
- 本项目持续基于上游工程进行维护和扩展，具体功能表现可能受硬件接线、通道映射、接收机及电调设置影响。

## 编译与工程说明

本仓库目前主要提供源码。当前仓库没有独立的 PlatformIO 工程配置，也没有包含预编译的网页烧录 BIN 文件；请勿将 GitHub 源码 ZIP 误认为可直接通过网页工具烧录的固件包。

使用 Arduino IDE 编译时，需要安装 ESP32 开发板支持及项目依赖库，包括 U8g2、bleGamepad 和其依赖的 NimBLE。中文字体需确认本机安装的 U8g2 版本包含源码所引用的 u8g2_font_wqy12_t_gb2312 字体。开发板选择和 USB CDC 设置可参考上游项目的编译说明。

上游硬件介绍、物料清单及原始组装资料请参阅：
- [dbloemhard 原始 ESP32-C3 项目](https://github.com/dbloemhard/ESP32_Transmitter_for_ELRS)
- [Kkbin505 Simple TX 原始项目](https://github.com/kkbin505/Arduino-Transmitter-for-ELRS)

## 许可证与致谢

本项目基于上游 GPL-3.0 开源项目继续开发。请保留原作者署名及相应许可证信息，并遵守 GPL-3.0 的适用要求。

感谢：
- **Kkbin505** — Simple TX 最初项目作者。
- **dbloemhard** — ESP32-C3 OLED 版本的移植与大量重写工作。

感谢所有为 ExpressLRS 开源生态作出贡献的开发者。
