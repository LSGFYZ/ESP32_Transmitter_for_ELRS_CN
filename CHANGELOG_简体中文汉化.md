# 汉化改动记录

- 新增 `locale_zh_cn.h`，集中管理常见本机菜单、选项、状态和常见 ELRS 参数翻译。
- `oled.cpp` 增加本地化绘制与宽度计算，使用 U8g2 GB2312 中文字库和 UTF-8 绘制 API。
- 本机设置键（例如 `Stick Mode`、`ADC Filter`、`Volt Scale`）保持原英文，避免改变 Preferences/NVS 中的键名。
- CRSF/ELRS 协议数据不翻译、不改包；仅在 OLED 绘制层做映射。
- 尚未实际编译或实机测试，不能视为已验证固件。
