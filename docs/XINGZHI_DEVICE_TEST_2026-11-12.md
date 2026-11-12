# Xingzhi 真机验收记录（2026-11-12）

## 目标与隔离

作者确认 ESP32-S3 R8N16、1.54 英寸；USB 实测 S3 rev0.2、16MB Flash、8MB PSRAM。
只操作 USB 303A:1001、序列号 D8:85:AC:D5:B7:60，当前端口 /dev/cu.usbmodem101。每次操作重新验证唯一标识；未向 M5 写入。

## 原固件回退验证

原版本 2.0.38。完整 16MB 备份在本机私有目录：
`~/.local/share/fmo-device-backups/xingzhi-D885ACD5B760/2026-11-12-original-2.0.38/`

已完成：备份大小与 SHA256；原分区表 MD5；bootloader 与应用镜像校验和/摘要；设备全 Flash 摘要比对；原备份完整回写、写后 Hash verified；重新启动日志确认 App version 与 Current version 2.0.38。恢复脚本按 USB 唯一标识选择设备。原始备份含潜在凭据，不进入仓库或候选包。

## 候选安装

已清除新布局 NVS/PHY 区域，按新分区写入 bootloader、partition-table、0x30000 应用，全部写后校验通过。启动确认 0.7.29-s3.1，8MB PSRAM memory test OK，字体 missing=0。

首次真机 USB 状态回复暴露单次消息超过 2048 字节发送缓冲的问题，修为最大 128 字节分段发送（本地提交 1de48db）。USB 请求缓冲回归通过；仍沿用未发布的候选版本 0.7.29-s3.1。

尚待用户现场确认屏幕颜色/方向/布局、三键操作、测试音可闻和麦克风对人声的响应。日志启动成功不能代替这些验收。未配置 FMO 联网身份、未进行双向通联，未公开发布。

## 修正版 USB 与音频状态回读

修正版写入校验通过，USB 连续状态查询成功。设备报告 board=xingzhi-cube-1.54tft-wifi，version=0.7.29-s3.1，audioReady=true，dmaRateHz=7999，micRateHz=8000，outputErrors=0。这里的采样率是协议等效值；物理 I2S 麦克风 16kHz、扬声器 24kHz。未收到用户物理操作反馈，keyEvents=0，未播放测试音或进入麦克风测试，因此不能声明三键和声音验收通过。

修正版应用 SHA256：0f7dabf80e987ea6c01ae12b977d32ca52675a499e3514035c597ff2b6d5503f。源码提交：1de48db5a591212f5abc4c5cdc0e6d179b5315e3。
