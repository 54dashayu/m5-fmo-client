# 广播发现目录

只读获取 https://map.bg5esn.com/api/stream 的首个公开快照，输出最少的服务器字段；不转发位置、聊天、设备个人资料，不向 APRS 发布内容，也不试连对方 MQTT。

当前来源验证使用汇总站的 `certStatus=Ok`、`sigStatus=Valid`、近期 `STATION` 状态；界面明确标注“来源验证通过”。这不是 M5 本地完整验签，也不证明对方接受 M5 的证书。本机成功记录只由真实 MQTT CONNECTED 产生，且仅用于本次开机、当前身份；来源证书失败和陈旧记录不开放新增。

此来源目前主要验证官方根。QFM/FRlink 等独立根的记录可能显示未验证或未收录；导入 QFM 根到 SAS 不会改变第三方汇总站的信任库。本版本不能声称发现了整个 FRlink 集群。

部署：系统 Python 3.10+，每120秒运行 `python3 catalog.py /var/lib/fmo-discovery/catalog.json`。失败保留旧文件，但浏览器在10分钟后将目录标为过期并禁用添加。HTTPS 静态入口需允许无凭据 CORS，设备网页只从固定的 `https://fmo.bh1jss.net/m5-discovery.json` 读取。
