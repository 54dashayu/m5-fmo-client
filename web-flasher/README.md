# 在线 USB 刷写页面

发布位置：https://app.bh1jss.net/m5-flasher/

```
npm ci
npm run build
```

在仓库根运行 `python3 tools/package_release.py` 生成固件、ZIP、校验和及 `public/firmware/manifest.json`，将 `public/` 部署到 HTTPS 静态目录。

支持桌面 Chrome / Edge Web Serial。所有运行时依赖在本站打包，不依赖外部 CDN。下载先验 SHA256，写入后 esptool-js 校验 MD5。升级先读取分区表并检查 NVS / factory 地址与大小，只写 0x10000 应用。首次安装写三个分段，不写 NVS；均禁止 eraseAll。

不自动读取或上传证书、私钥、Wi-Fi 配置。串口选择必须由用户点击。首装改变分区，其他固件数据不保证兼容；原版 ESP32 芯片检测不代替用户核对 M5 FIRE 硬件。

已验证构建和浏览器流程。浏览器真实 USB 写入与首次安装仍需实机验收；已有设备应用区升级已用 esptool CLI 验证。

刷写分两步：选择串口、校验下载和设备分区后显示确认区；仅“确认开始刷写”调用 writeFlash，取消则退出并尝试重启原系统，不写 Flash。
