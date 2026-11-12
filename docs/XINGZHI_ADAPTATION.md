# 星智 Cube 1.54 Wi-Fi FMO 适配候选

日期：2026-11-12。基线为 M5 FMO 0.7.29、提交 8de2456。小智候选标识为 0.7.29-s3.1；这是本地适配构建，不是新正式版。独立本地副本及分支 codex/xingzhi-cube-fmo，未修改原 M5 工作区、未刷机、未发布。

## 设备与来源

只读 USB 已确认 ESP32-S3 QFN56 revision 0.2、16 MB Flash、8 MB 封装 PSRAM、40 MHz 晶振、原生 USB Serial/JTAG。原小智固件报告 2.0.38。

板型参考 geek-alpha/xiaozhi-esp32-music 提交 f5352ac5a6bf921299c555f84e02cdb242d276f9 下的 main/boards/xingzhi-cube-1.54tft-wifi/config.h 及板级初始化。仓库地址：https://github.com/geek-alpha/xiaozhi-esp32-music 。芯片资源来自实机读取，引脚来自对应板型源码；物理连线和最终听感尚待刷写后的独立验收。

## 本轮实现

- 独立板型 Kconfig、配置头、S3 sdkconfig defaults、分区、依赖锁及构建目录。M5 仍使用原 ESP32 音频、ILI9341、灯带与电量驱动。
- ST7789 SPI 屏幕，240×240，RGB565、反色设置，双 DMA 缓冲、40 MHz SPI，GPIO13 PWM 背光。保留 300 秒空闲降亮及来话、按键恢复。
- 方屏主页按原生字号重新布局，双历史呼号分两行；菜单长文本滚动。物理音量减映射 A、BOOT 映射 B、音量加映射 C。B 短按发话/停止，长按停止；A 长按中继，C 长按菜单。
- GPIO21 电源保持。未启用自动断电。
- 两路标准 I2S：麦克风 16 kHz、扬声器 24 kHz、32 位单声道左槽；FMO PCM 维持 8 kHz。47 抽头低通负责 16→8 kHz 抽取和 8→24 kHz 插值，数字音量独立处理，不套用模拟 ADC/DAC 算法。
- 保留 RADPCM 发送、RADPCM/Opus 接收；本轮没有增加 Opus 发送。初始化检测采集和播放节拍，超时及短读报错；发送保留原 60 秒上限与收发互斥。
- USB 管理接入原生 USB Serial/JTAG，兼容现有 @M5 管理协议；网页及 USB 状态增加 board 字段和适配版本标识。
- 保留 0.7.29 多 CA 身份档案、申请/领取、授权导入、切换/回退与按体系中继默认项。切换身份仍清除旧接入证据；协议或服务器名称不构成互信证明。
- 新板灯光和电量暂未启用。电量为未知，不能显示为 0%；不能把 GPIO15 当作 M5 灯带引脚，因为它是本板扬声器时钟。

## 引脚

| 功能 | GPIO |
| --- | --- |
| 显示 MOSI / SCLK / DC / CS / RESET | 10 / 9 / 8 / 14 / 18 |
| 背光 / 电源保持 | 13 / 21 |
| 麦克风 BCLK / WS / DIN | 5 / 4 / 6 |
| 扬声器 BCLK / WS / DOUT | 15 / 16 / 7 |
| A 音量减 / B BOOT / C 音量加 | 39 / 0 / 40 |

## 构建与候选打包

激活 ESP-IDF 5.5.1 后，在此独立副本根目录运行：

```sh
python tools/build_board.py xingzhi
python tools/build_board.py m5
python tools/run_tests.py
python tools/package_xingzhi_candidate.py
```

离线测试需安装 requirements.txt，并设置 IDF_PATH。S3 构建在 build-xingzhi，M5 回归构建在 build-m5-regression。不会连接设备。候选包只写 artifacts/xingzhi-cube-1.54tft-wifi/0.7.29-s3.1，不更新在线刷写站。每包提供 manifest 和 SHA256SUMS；源码保存在独立本地分支 codex/xingzhi-cube-fmo，候选 manifest 记录 sourceCommit；不能仅检出 0.7.29 基线提交复现。

## 分区与首次安装约束

| 区域 | 起始 | 大小 |
| --- | --- | --- |
| NVS | 0x9000 | 96 KiB |
| PHY | 0x20000 | 4 KiB |
| 工厂应用 | 0x30000 | 5 MiB |

首次安装不是原小智固件的保留配置升级。本候选分区与原小智及 M5 均不兼容，不提供 OTA 或自动迁移原小智数据。不得向 M5 写此候选，不得只将本应用写到旧固件应用地址。

刷写前需用户明确授权，并先准备原厂/当前固件恢复方案。若用户需要整机备份，必须作为含潜在凭据的私人文件处理，不能混入本候选包或上传仓库。默认新建 FMO 设备身份；授权身份迁移遵循已有 USB 限制、密钥匹配与源设备离线要求。

## 验证记录与待验项目

已完成 ESP32-S3 与 ESP32 两目标构建；原有离线回归通过，包括多 CA、身份中断恢复、证书校验、中继、USB、音频协议和按键状态机。

数字重采样新增测试通过：分块与连续处理一致、静音、直流增益、饱和、6 kHz 相对 1 kHz 输入混叠抑制约 70.5 dB；AddressSanitizer 与 UndefinedBehaviorSanitizer 检查通过。该结果不是实机音质结论。

已于 2026-11-12 完成原小智 2.0.38 全 Flash 备份、校验与实际恢复启动验证，并安装本候选；启动、PSRAM、字体、USB 状态回读及音频驱动初始化通过，详细证据见 XINGZHI_DEVICE_TEST_2026-11-12.md。

尚未完成现场验收：真实显示颜色/方向/字体可读性、三键、电源保持、麦克风增益/槽位、扬声器播放音量与起止爆音、双向通联、USB 管理与日志并发、Wi-Fi/证书/身份切换、300 秒背光边界、断电恢复及长期稳定性。以上逐项通过前不能标为已支持硬件或发布正式版。

建议首次验收顺序：启动及屏幕 → USB/网页管理 → 本地扬声器/麦克风测试 → 独立身份申请及测试中继 → 双向通联 → 双身份及断电恢复 → 省电与长期运行。未取得授权前不发射语音、不刷写。
