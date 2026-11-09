# 第三方来源和许可

保留各源码目录中的原始版权和许可；顶层 MIT 不覆盖第三方许可条款。

| 组件 | 来源 / 版本 | 许可文件 |
| --- | --- | --- |
| M5 驱动基础 | [54dashayu/esp32-fmo-companion](https://github.com/54dashayu/esp32-fmo-companion)，基线 7619bf99d5355611d4d103b42bd140e34acd0393；原版权 BI8SIG | [MIT](ui-preview-firmware/LICENSE) |
| LVGL | 8.3.11，随项目固定源码 | [MIT](ui-preview-firmware/components/lvgl/LICENCE.txt) |
| lvgl_esp32_drivers | 随硬件适配基础保留 | [MIT](ui-preview-firmware/components/lvgl_esp32_drivers/LICENSE) |
| ESP-MQTT | 6af4446a48ea7fa54948edc4e62277ed70abb6e1，项目本地修改 | [Apache-2.0](ui-preview-firmware/components/mqtt/esp-mqtt/LICENSE) |
| Espressif led_strip | 3.0.3，参考 M5 Core V2.0 伴侣的 GPIO15 / GRB 配置 | [Apache-2.0](ui-preview-firmware/components/led_strip/LICENSE) |
| Opus | 1.5.2，fixed-point ESP-IDF 集成 | [COPYING](ui-preview-firmware/components/opus/COPYING) |
| Monocypher | 随项目固定的 Ed25519 实现 | [BSD-2-Clause / CC0](ui-preview-firmware/components/monocypher/LICENCE.md) |
| Noto Sans CJK SC Bold | 原字库及 LVGL 子集 | [SIL OFL](ui-preview-firmware/assets/fonts/OFL.txt) |

ESP-IDF v5.5.1 由开发者单独安装；发布二进制还使用其组件，许可证可查阅 [ESP-IDF LICENSE](https://github.com/espressif/esp-idf/blob/v5.5.1/LICENSE)。固件 ZIP 附带所用组件许可文本。

FMO 格式参考：[数字证书格式](https://bg5esn.com/docs/fmo-digital-certificate-format/)、[语音格式](https://bg5esn.com/docs/fmo-voice-codec-spec/)。本项目不隶属于官方 FMO 或 AI Passport 项目。

## 在线刷写工具

esptool-js 0.7.1 (Apache-2.0)、js-md5 0.8.3 (MIT) 及其依赖由 npm 锁文件固定，运行时打包在本站。完整许可见 `web-flasher/public/licenses.txt`。
