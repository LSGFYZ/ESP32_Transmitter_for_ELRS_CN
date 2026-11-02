ESP32_Transmitter_for_ELRS_CN

ESP32 Transmitter for ELRS — Simplified Chinese Edition

本项目基于原作者 dbloemhard 的 ESP32 Transmitter for ELRS 项目进行汉化和功能扩展。

Original Project:
https://github.com/dbloemhard/ESP32_Transmitter_for_ELRS

Original Author: dbloemhard

主要改进

OLED 界面简体中文汉化

增加 ELRS 接收机电池电压回传

增加接收机电压显示

OLED 显示布局优化

本项目保留原项目版权声明，并遵循 GPL-3.0 许可证的适用要求。

ESP32 C3 Nano TX

ESP32 C3 Nano 尺寸的 ExpressLRS 发射机，最初基于 Simple TX Arduino 发射机项目改造，并增加了一些额外功能。

Simple TX 是由 Kkbin505 开发的一款基于 Arduino 的 ELRS 遥控发射机。他设计了一款软硬件都非常简单易用的 RC 发射机。

我将其重写为运行在 ESP32 C3 模块上，并搭配了一块在速卖通（AliExpress）上只需几美元就能买到的 OLED 屏幕。它的体积足够小巧，甚至比我之前的 TinyTX 项目还要小，而且还内置了 OLED 屏幕！目前我已经实现了从 ELRS 模块读取遥测数据，并完整移植了用于配置 ELRS 和手台的 ELRS Lua 脚本功能。

⚠️ 这是一个活跃维护的仓库！代码和资源仍在持续改进中。
https://youtube.com/shorts/-rPwNI36CTY?feature=share

这款遥控器已经可以正常工作，我对它非常满意。但我确信仍有一些需要修复或改进的地方。现在有了 PCB，组装变得超级简单；3D 打印的外壳也做到了在保证可用性的前提下尽可能小巧。我想研究一下折叠摇杆臂之类的方案以提升握持手感，但机械设计确实有难度。STEP 文件已开源，欢迎有能力的大佬接手优化！

在代码方面，后续主要会聚焦于 Bug 修复，或者如果大家有酷炫的新功能想法（前提是不破坏 UI 的简洁性），也欢迎提出。

核心特性：
真正可放入口袋：不像 Radio Master Pocket 那样带出去还会引人侧目……它是真的能轻松装进口袋。
PS5 霍尔摇杆：即使体积这么小，也不会牺牲磁感应带来的超高精度。
0.42 英寸 OLED 屏幕：显示清晰，UI 简洁易读。
1S 锂电池供电：和你飞 Whoop 穿越机用的是同一种电池。虽未严格测试，但预计使用 1S 300mAh 电池续航可超过 1 小时。
模块化设计：专为使用 100mW 遥测 Nano 接收机作为 TX 模块（RX 改 TX）而设计。同时也支持 ExpressLRS 2.4G 外置 TX 模块，不过那样体积可能比遥控器本身还大。注意：外置模块通常需要独立供电，因为大多数外置模块的工作电压要求在 7V 以上。
最高 500Hz 刷新率：甚至可能支持 FLRC 1000Hz？不过谁会用那么高的频率呢……
4 个模拟通道
3 个 AUX 辅助通道：可通过摇杆按键触发（左摇杆按下切换解锁/Arm，右摇杆按下切换通道 6，长按切换通道 7）。
BLE 蓝牙手柄功能：延迟比直接使用 ELRS 模块自带的 BLE 手柄模式更低。

目前有两种组装方案可选：一种是原始的飞线版（将摇杆导线直接焊接到 ESP32 焊盘上），另一种是 PCB 版（大幅简化了组装流程和时间）。如果你选择飞线版，请注意内部空间非常紧凑，务必确保所有导线长度合适，必要时进行修剪。除非你特别排斥从 PCBWay 或 JLCPCB（嘉立创）打板，否则我强烈建议制作 PCB 版。现在打板真的很方便！

📋 当前愿望清单 / 待办任务：
将 OLED 更新改为多任务处理，为 ELRS 协议代码释放更多主循环时间
实现 USB Passthrough 功能，支持通过 USB 直接更新 ELRS 发射机固件

本项目使用 Arduino IDE 编写，倾注了心血以及少量 AI 建议（发现 AI 写的代码最后还是得自己全部重写）。

🔧 编译与烧录指南

方式一：Arduino IDE 编译上传
通过开发板管理器安装 ESP32 Boards
安装 Oliver 开发的 U8g2 库
安装 LemmingDev 开发的 bleGamepad 库（搜索 lemmingdev，系统会提示安装依赖的 NimBLE 库）
选择开发板为 ESP32C3 Dev Module，并选择对应的 COM 端口
将 USB CDC On Boot 设置为 "Enabled"
编译并上传

方式二：网页端一键烧录（无需编译）
访问 https://espressif.github.io/esptool-js/
点击 Connect（选择 ESP32 对应的 COM 端口）
将 Flash Address 修改为 0x0000，点击 Choose File 选择 Assets 文件夹中的 ESP32C3-TX.bin
其他选项保持默认，点击 Program
⚠️ 注意：烧录完成后，你需要手动拔插 USB 重启 ESP32（它似乎不会像烧录工具提示的那样自动复位……）

🎥 完整组装视频（飞线版）
https://www.youtube.com/watch?v=7jazVEKMIew

🛒 物料清单 (BOM)：
ESP32 C3 + 0.42寸 OLED 一体板 https://ja.aliexpress.com/item/1005009157602185.html （或类似产品）
PS5 霍尔摇杆 https://ja.aliexpress.com/item/1005006282848536.html （或类似产品）
Nano 接收机（用作 TX 模块） https://ja.aliexpress.com/item/1005010228737836.html （或类似产品）
5V 升压模块：https://ja.aliexpress.com/item/1005011803028198.html <- 不再推荐此款。该稳压器会产生噪声干扰 ESP32，导致摇杆/电压读数异常。如果你已经用了这款，我很抱歉，它当初看参数确实没问题……
✅ 推荐替代款：https://ja.aliexpress.com/item/1005009770893985.html （红色 PCB 版本）。它刚好能塞进 ELRS RX 和 ESP32 之间的缝隙里。仍在寻找更小尺寸的完美替代品……
M1.2 x 6mm 自攻螺丝 https://ja.aliexpress.com/item/1005008724193768.html
分压电阻（用于电池电压检测电路） https://ja.aliexpress.com/item/1005009412772665.html （10k+20k / 15k+30k / 16k+33k 任选一组）
AWG 32 或 30 硅胶线，多色可选（建议备齐 10 种颜色以便区分） https://ja.aliexpress.com/item/4000485605478.html
摇杆帽：可 3D 打印或直接购买成品 https://ja.aliexpress.com/item/1005006734634193.html - 我在日亚买的，看图片像是 PS4 版本的，但两者都能通用。
如果你和我一样需要延长杆，可以打印带 M2 螺纹孔的摇杆帽。DJI RC 3 的原装杆头堪称完美，但没有第三方配件且单价 10 对这个项目来说太贵了。我已验证这款 DJI RC Pro 的第三方杆头可以完美适配我设计的打印帽 https://ja.aliexpress.com/item/1005004437467902.html （颜色选：for DJI RC PRO）。虽然比 RC 3 原装稍短，但完全够用。我还提供了热熔螺母的 STL 文件，你可以嵌入 M3 螺母来兼容 Radiomaster Pocket 的杆头。
BT2.0 插头电池尾线（10cm 长），以及 3D 打印外壳。

💰 总成本约 15-20 美元

本项目基于 kkbin505 的 SimpleTX 项目二次开发，但目前代码已几乎完全重写，其中 90% 为手工编写（LLM 仅用于提供代码建议）。
⚖️ 免责声明：发布本项目的初衷是希望它能对大家有所帮助，但不提供任何明示或暗示的担保。

LSGFYZ在2026年10月29日汉化并增加的电压回传显示
