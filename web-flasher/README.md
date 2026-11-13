# FMO 多硬件在线刷写

入口保持 https://app.bh1jss.net/m5-flasher/ 。项目介绍与刷写页面支持 M5 FIRE 类、Xingzhi Cube 1.54 TFT 小智的明确选择，并提供实机照片。

构建前端：`npm ci && npm run build`。固件：`python tools/build_board.py m5` 和 `python tools/build_board.py xingzhi`；提交源码后 `python tools/package_multiboard_release.py` 生成两套版本目录、独立 manifest、ZIP 与校验和，不连接设备、不部署。`--preview` 仅供本地预览，不代表发布已通过门禁。

两套清单分别在 firmware/m5-fire/manifest.json 和 firmware/xingzhi-cube-1.54tft-wifi/manifest.json。所有文件在对应 v版本 目录；旧目录保留可回退。生产发布遵守 docs/RELEASE.md 的确切提交及标签双门禁，之后才更新默认清单和页面。

桌面 Chrome/Edge Web Serial。准备阶段检查下载SHA256、芯片及现有分区；最终确认后才写入，取消不写，已有相册时升级只写应用，旧标准布局会新增相册分区。首次安装另需确认。S3 原厂迁移采用全FF initial-settings.bin 初始化新NVS/PHY范围；此文件不含任何设备数据，仅首次安装写入，升级不写。禁止 eraseAll。

esptool-js 0.7.1 重启策略缺少 RTS 置位，网页补充 DTR释放与RTS脉冲；S3使用200ms，原ESP32使用120ms。支持芯片检查不能自动识别同芯片的不同接线，必须由用户核对板型。

测试覆盖两种板型的升级、首次安装、准备不写、取消不写、芯片错误及分区不兼容拒绝、硬件切换清除确认、复位脉冲。模拟测试不代表真实浏览器USB刷写验收。照片由用户授权发布。
