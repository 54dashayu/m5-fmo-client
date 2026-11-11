# 广播发现目录

只读获取 https://map.bg5esn.com/api/stream 的首个公开快照，输出最少的服务器字段；不转发位置、聊天、设备个人资料，不向 APRS 发布内容，也不试连对方 MQTT。

当前来源验证使用汇总站的 `certStatus=Ok`、`sigStatus=Valid`、近期 `STATION` 状态；界面明确标注“来源验证通过”。这不是 M5 本地完整验签，也不证明对方接受 M5 的证书。本机成功记录只由真实 MQTT CONNECTED 产生，且仅用于本次开机、当前身份；来源证书失败和陈旧记录不开放新增。

此来源目前主要验证官方根。QFM/FRlink 等独立根的记录可能显示未验证或未收录；导入 QFM 根到 SAS 不会改变第三方汇总站的信任库。本版本不能声称发现了整个 FRlink 集群。

部署：系统 Python 3.10+，每120秒运行 `python3 catalog.py /var/lib/fmo-discovery/catalog.json`。失败保留旧文件，但浏览器在10分钟后将目录标为过期并禁用添加。HTTPS 静态入口需允许无凭据 CORS，设备网页只从固定的 `https://fmo.bh1jss.net/m5-discovery.json` 读取。

## 0.7.22 自动发现与认证探测（测试中）

新增 `trusted_catalog.py`，读取公开 APRS 原始日志，验证 STATION 的确定性 CBOR / Ed25519 签名、时间窗口及 Root → Intermediate → User 链。信任来源为服务器当前启用的 `/opt/fmo-sas/.sas/roots`；公开中间证书从 `/opt/fmo-m5-enrollment/public-certificates` 和 `/var/lib/fmo-discovery/intermediates` 读取。仅根证书无法恢复中间证书，缺失时不放行。不得根据名称/集群字符串推断信任。

官方中间证书 1001 来自 https://bg5esn.com/docs/fmo-formate/，保存前后均须通过已信任官方根的签名校验。CA 文件不包含私钥。独立 CA 的公开中间证书可放入 intermediates，重签或更换时会重新验签。

服务输出 `/var/lib/fmo-discovery/trusted.json`，HTTP/HTTPS 入口 `/m5-trusted-discovery.json`，正文以独立目录密钥 Ed25519 签名（payload 字符串 + signature），私钥只在服务器 `/var/lib/fmo-discovery/catalog-signing.key`，固件固定公开验签钥。每分钟刷新，最多 6 个近期候选（设备容量为 8 个，保留解析内存余量）；公开日志可能不完整，不保证发现全网。目录通过根验证仅证明对方身份，不等于接纳 M5。

M5 校验目录签名与时间戳，空闲 10 秒后读取目录，最多每分钟对一个节点使用短 TCP 连接做一次 MQTT CONNECT（复用联网工作任务，不新增发现或 MQTT 任务），不发布、不订阅语音。当前服务器或同地址服务器不额外探测。只有本机证书成功认证的结果显示在设备/网页发现列表；结果绑定本机完整证书、端点与服务器指纹，广播/目录过期或身份更换即失效。连接失败不标注“未信任”，因为也可能是网络或服务器暂时不可用。客户端仍由对方 SAS 检查准入和吊销，目录服务目前不额外拉取 CRL。

依赖：Python 3.10+、cryptography、cbor2（可放独立 vendor 目录，不修改语音服务环境）。systemd service/timer 见同目录；静态路由必须设置 no-store。停用时停止 timer，恢复旧 nginx include 即可；不涉及 SAS/EMQX 重启。
