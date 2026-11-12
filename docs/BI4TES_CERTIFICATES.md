# BI4TES / AI Passport certificates (0.7.27 development)

Two certificate profiles are retained: BH1JSS M5 and BI4TES. New applications use the current local device key. A separately imported full Passport identity has its own seed and certificate. Importing a root alone does not grant a device identity or access to a relay.

In device web management → 本机设备证书 → 开始申请, choose the BI4TES form. Supply the same bare callsign as the current identity and the license photo required by BI4TES, then explicitly consent. The browser queries an existing application before submitting a new one. Existing pending/ready applications can resume retrieval without another upload. The device polls approximately once per minute when idle, including after reboot and with the browser closed.

The current certificate stays active while pending or on any validation/transport error. A successfully validated certificate is saved before activation. BH1JSS and BI4TES certificates have separate NVS slots (`cert_bh`, `cert_bi`), with current `cert` and legacy backup retained. The management page can select either available profile. Selection/rollback revalidates binding and expiry and cancels automatic retrieval. It does not establish that every relay accepts that issuer. Contact the relay administrator if login is rejected.

Stopping retrieval does not cancel the CA's application. Changing user clears both profiles, backups and pending retrieval before generating the new identity; Wi-Fi and relays remain.

A public certificate exported from a different Passport is bound to that device's key and is rejected on this M5. The inspected Passport web page exposes no private-key export. With the owner's physical USB access, this particular unencrypted ESP32-C3 permits a local full identity backup. Do not assume this works for other models or encrypted flash.

A full backup uses `format: fmo-device-identity-v1`, `deviceSeed` (base64url Ed25519 seed) and `bundle` (complete certificate chain). It contains a private key: keep outside repositories with mode 0600; never publish or log it. Import is permitted **only through the localhost USB management transport**, not the device's plain-HTTP Wi-Fi interface. The device validates key/certificate binding, callsign, pinned BI4TES root, signatures and validity before saving. Import does not activate the identity. Take the original Passport offline before selecting this profile; both otherwise share one UID and may disconnect each other.

Profile switching persists `profile_next` after the bank is saved; boot validates and replays the key/certificate pair before networking. Original BH1JSS key and certificate remain in `seed_bh` / `cert_bh`; imported BI4TES uses `seed_bi` / `cert_bi`. Interrupted writes are replayed on reboot. “Change user” clears these slots and pending transitions. BI4TES approval of new M5 applications remains subject to its administrator's decision.

Validation before switching refers to cryptographic certificate validation, not a prior successful login to a selected relay. Real issuance/automatic receipt requires user submission and administrator approval; offline tests do not replace that acceptance step.

Identity activation clears previous access-validation evidence and sets a persistent relay-refresh notice on both certificate and relay pages. Saved relay addresses remain. Discovery refresh is queued for an idle interval, and access checks use the new certificate fingerprint. Clicking refresh acknowledges the notice; it does not claim that every relay has already accepted the new identity.

Hardware acceptance (2026-11-12, development firmware): an existing owner-authorized Passport identity was imported over USB without changing the active BH1JSS key, authenticated to relay 1791, retrieved its already-issued BI4TES certificate through the adapter, and returned successfully to the retained BH1JSS identity. The original Passport was kept offline during duplicate-UID testing. A new license-photo application was not submitted; it remains a user-initiated flow subject to BI4TES review. The temporary raw NVS read was deleted after producing the restricted identity-only backup.

Storage recovery fix: the 24 KiB NVS can lack contiguous entries when a fourth certificate copy is written. Known profiles now use their own retained slots rather than another legacy backup. Boot transition validates the target first, releases redundant/active copies while the target journal and profile banks remain durable, then restores the active pair. Interrupted writes replay from the retained bank. A failed transition no longer aborts management startup. Regression tests cover full-store replacement and twelve write/commit interruption points.

Storage-fix hardware acceptance: recovered the real ESP_ERR_NVS_NOT_ENOUGH_SPACE boot failure without erasing Wi-Fi, relays or either profile; then verified BI4TES → BH1JSS → BI4TES, with certificate validity, audio readiness and relay login successful in each profile. Final device identity is BI4TES; the source Passport must remain offline while this copied identity is active.

### 当前体系与广播目录

证书页面以绿色“正在使用”标记当前生效的体系，并将另一套可用证书标记为“已保存，切换使用”。标记来自设备状态，不以按钮点击作为切换成功的依据。

网页“发现服务器”默认由浏览器加载广播汇总源的完整目录，可按名称、呼号或 UID 搜索，按状态筛选并使用页码下拉菜单；每页可显示 10、20 或 50 条。设备自身继续小批次验证接入，完整目录不写入设备 NVS，也不占用设备的音频缓冲区。

目录仅覆盖汇总源收到的记录，不保证包含尚未广播或汇总源未收录的节点。历史、过期或资料不完整的记录可查看，但不能直接添加。广播源报告验签通过，不等于对方接受本机证书；手动添加也不会自动切换中继或授予接入权限。浏览器需要能访问互联网；只有设备热点且无外网时，可查看本机自动验证候选。

### 根 CA 列表与额外体系（2026-11-12 开发）

网页以列表展示两套内置体系和用户导入的根证书：正在使用/当前中继已接入、设备证书已保存、仅有根 CA。连接状态只描述当前身份和当前中继，不代表对该 CA 的所有服务器均可接入。列表同时显示完整根公钥。

`POST /api/roots` 支持列表查询及显式确认后的 JSON 根证书导入。固件校验 FMO rootCA 格式、重复字段、Ed25519 自签名、名称关系、授权路径和有效期；未校时拒绝导入。按根公钥去重。额外体系目前最多 8 个、根文件最多 2 KiB，实际可保存数量仍受 NVS 空间限制；失败保留当前身份。

额外根保存在 `root_0..7`，对应本机密钥的设备证书和密钥在 `cert_p0..7/seed_p0..7`。完整证书链通过校验后保存，用户再从列表切换；沿用 `profile_next` 启动恢复，目标 3..10 对应额外体系。未知体系不自动推断申请网址或接入参数。当前仅 BI4TES 完整私钥迁移路径经过实机验收，额外体系只提供匹配本机当前公钥的证书导入。

更换使用者会清除所有额外设备身份，公开根证书保留。重复导入同根不会自动更新其元数据；若该根的证书更新或撤销政策变化，需要后续维护流程，不能以自签名有效替代向签发方核对。

本轮离线回归通过：额外根去重/存储失败/容量、错误根拒绝、身份清理 31 个存储中断点，以及 10 个身份槽位的启动事务恢复。页面模拟测试通过列表、导入确认及根证书不能直接启用身份。实机验收结果另行补充。

实机补充（2026-11-12，0.7.27-dev）：应用区刷写及哈希校验通过，设备网页实际返回根 CA 列表与导入入口。通过 USB 测试：篡改自签名的 QFM 根被拒绝；用户提供的 QFM-FMO Gateway Root 导入成功，重复导入去重；列表显示仅有根 CA、不可切换。重启以及 BI4TES → BH1JSS → BI4TES 后，QFM 根仍保留，两套原身份均完整。

在 1791 测试台，两套身份均认证成功、音频就绪。最初直接切换到 BH1JSS 时，所选 2421 节点未能接入；Wi-Fi 与证书有效性正常，不将其归为切换失败，也不推断对方拒绝该根的具体原因。最终已恢复用户原来的 BI4TES 身份与 2421 中继并确认在线。额外第三体系的设备证书真实签发/登录尚未实机验收；本次不发布正式版。

### M5 屏幕入口

设备菜单 → 设备身份 → 短按 B，打开根 CA 列表。A/C 上下选择、每页四项；当前体系标记“当前”，仅根条目标记“仅根”。只有已保存设备证书的非当前体系可进入确认页；A/C 取消，B 确认。确认后请求交给管理任务执行，不在屏幕/按键任务内校验证书或写 NVS；重复按 B 不重复提交。证书校验失败时显示具体错误。

切换沿用可恢复的启动事务。启动发现服务时，若持久化身份变更标志存在，清除旧接入证据并自动安排按新身份刷新候选。保存的中继地址不删除，也不会把自动发现的新节点全部加入收藏。目录需要网络且在空闲时处理，不能把刷新安排完成当作所有节点已验证。

屏幕入口构建与刷写记录：新增真实 UI 按键/分页逻辑主机回归通过，完整离线回归与固件构建通过。已刷入当前 M5 并校验哈希；读回 0.7.27-dev、BI4TES / 2421 在线、音频就绪，三项 CA 状态完整；自动发现已取得 6 个广播验签候选。实体按键和屏幕显示仍等待用户确认，不将主机测试当作物理操作验收。

## 0.7.27 中继隔离

每个体系用独立的索引保存收藏和选择位置；公共连接资料可复用，删除某体系收藏不删除其他体系条目。首次迁移仅复制旧地址，不复制验证结论。接入证据存储在独立体系键下，并绑定设备证书包 SHA256；换证后证据失效。成功和最近失败状态重启后保留，服务器仍会在每次连接验证原始证书链。发现候选切换后重新刷新，公共 APRS 目录不等于对方接受此身份。

各体系的发行审批由原签发方负责：BH1JSS 维持现有申请和人工审核；BI4TES 申请直接到原 HTTPS 服务，适配器仅查询已签发的公开文件；自定义根当前仅支持导入原签发方的完整证书。端侧验证三级签名、签发关系、授权范围、有效期和本机密钥绑定。此实现不宣称已实现各发行方所有动态吊销或额外业务规则，最终仍以服务器认证结果为准。
