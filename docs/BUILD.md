# 编译、刷机与升级

## 在线刷写（电脑 USB）

打开 [在线刷写工具](https://app.bh1jss.net/m5-flasher/)，使用电脑 Chrome / Edge，确认硬件后选择“保留配置升级”或“首次安装”。升级会检查分区并只写应用，下载验 SHA256、写入验 MD5。手机浏览器用于刷完后的配网。浏览器真实 USB 刷写仍待实机验收，可使用下方已验证的命令行方式。

## 下载预编译版

在 [GitHub Releases](https://github.com/54dashayu/m5-fmo-client/releases/latest) 下载 v0.7.14 的 `m5-fmo-client-v0.7.14.zip`，解压并核对 `SHA256SUMS`。包内是通用程序，不包含 Wi‑Fi、设备私钥或用户证书。

需安装 Python 和 esptool：

```sh
python3 -m venv .flash-venv
. .flash-venv/bin/activate
pip install esptool==4.11.0
python -m esptool --help
```

关闭串口监视器 / USB 管理程序。下面将 `PORT` 替换为实际串口：macOS 通常为 `/dev/cu.usbserial-...`，Linux 为 `/dev/ttyUSB0`，Windows 为 `COM3` 等。不要同时连接多台却误选串口。

### 已运行本项目的设备：只更新应用

在解压目录运行：

```sh
python -m esptool --chip esp32 --port PORT --baud 115200 write_flash \
  0x10000 m5-fmo-client-v0.7.14-app.bin
```

此方式保留 NVS 中的 Wi‑Fi、设备密钥、证书和设置。仅适用于已有兼容分区、应用起始地址为 `0x10000` 且空间至少 3 MB 的设备。刷写结束必须看到校验成功；中途断线应重新完成刷写。

### 首次安装 / 从其他固件迁移

先备份原固件和有用配置。全新安装会写入引导程序和分区表，原固件的数据布局可能不兼容；不能承诺保留其他固件的数据。不要把整机 Flash 备份发布到网上。

```sh
python -m esptool --chip esp32 --port PORT --baud 115200 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x1000 bootloader.bin \
  0x8000 partition-table.bin \
  0x10000 m5-fmo-client-v0.7.14-app.bin
```

这使用 16 MB 硬件中的前 4 MB，不意味着硬件只有 4 MB。首次安装路径经过构建和分区检查；本次现有设备验收采用应用区升级，没有为了发布而清空实机做全新安装。

**不要默认执行 `erase_flash`。** 清除 NVS 会丢失本机密钥，旧的设备证书便不能继续匹配；需要以新公钥重新申请。回退相同分区布局的旧版本也应只写应用区，并保留自己的原版应用文件。

## 从源码构建

按 [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/get-started/)安装 **v5.5.1** 并启用其环境。然后：

```sh
git clone https://github.com/54dashayu/m5-fmo-client.git
cd m5-fmo-client
idf.py -C ui-preview-firmware build
```

`sdkconfig` 已包含屏幕、引脚、分区和缓冲参数；不要只复制简短的 `sdkconfig.defaults` 而丢掉板级配置。默认目标为 ESP32。生成文件位于 `ui-preview-firmware/build/`。

首次安装可使用 `idf.py -C ui-preview-firmware -p PORT -b 115200 flash`；已配置设备按上文仅刷 `build/m5-fmo-ui-preview.bin` 到 `0x10000`，避免不必要地更新分区表。工程二进制沿用开发期名称，Release 下载包采用产品名称。

## 主机回归测试

使用 Python **3.12** 或 **3.13/3.14**、C 编译器，并导出 `IDF_PATH`（设备证书测试需要 IDF 自带 cJSON 源码）。Python 3.13 及以上通过 `audioop-lts` 提供独立 IMA 参考解码器。

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python tools/run_tests.py
```

测试不连接服务器，不签发生产证书，不发射语音。覆盖按钮状态机、语音包和 IMA 参考、发送队列、MQTT 分片读取 / 锁等待、采样校准、证书校验、Wi‑Fi 保存和 USB 重开。

## UI 模拟器

```sh
python3 -m http.server 8773 --bind 127.0.0.1
```

打开 <http://127.0.0.1:8773/ui-simulator/>。仅用于布局和交互预览，网络、音量电平和来话均为模拟，不能用来确认音频或服务器接入。

## 中文字体

仓库已含生成好的 LVGL 字库，普通构建无需 Node.js。修改界面中文字后，可运行 `python ui-preview-firmware/tools/build_fonts.py`，它调用固定版本 `lv_font_conv@1.5.3` 生成字形。请保留 Noto 字体的 OFL 许可。
