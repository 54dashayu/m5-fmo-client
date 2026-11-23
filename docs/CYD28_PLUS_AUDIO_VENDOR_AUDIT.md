# Plus 板 DAC / 功放资料核对（2026-11-23）

结论：当前 GPIO26 / DAC2、GPIO4 低有效及 PCM→无符号 DAC 高8位格式，与可核对的 Plus 专用示例和 IDF 驱动一致。未找到这块白色 Plus 的完整原理图，不能核定功放输入耦合、反馈增益、供电或去耦元件。持续劈声根因仍未知。本轮只读取与审计，未改固件、未刷写、未改硬件、未发布。

## 资料范围与版本

- 厂商产品页：https://www.zlxchina.com/zh/product/esp32-2432s028r-plus-development-board
- 厂商开发说明：https://zlxchina.com/zh/resource/esp32-2432s028r-plus-2 （仅3张产品/接口/套装图片）。背面图与用户白色 Plus PCB布局一致。
- 厂商刷写页：https://www.zlxchina.com/zh/flasher （仅查看文字，未执行刷写）。说明 GPIO26 DAC2 → AUDIO_IN，GPIO4 LOW 开启8002。
- 厂商仓库：https://github.com/qmaakdfq/ESP32-WROOM-32E-UE-Module-Development-Board ，审计树 3194965694b7cf40d533da7bdd7ad43ca873cdee。
- 该树包含 Plus 专用蓝牙音频示例、通用08 PWM示例、8页中英文产品说明、6页32UE英文说明；说明书为参数/接口/GPIO表，不含功放原理图或BOM。32UE资料不是用户32E模块的独立电路证明；资料中0248/028混用也不能当成精确硬件修订标识。
- AudioTools 上游当前树 cf52ad7cb778c9daf651af04fc9724a93daeb889，仅用来追踪依赖实现。厂商 platformio.ini 对平台及两个Git音频库均未锁定版本，因此不能证明厂商当时实测的底层驱动、默认时钟和版本。
- 本地 IDF 5.5.1 的 i2s_legacy.c、dac_common_legacy.c、dac_ll.h、ledc.h / ledc.rst。
- 原始文件、PDF提取文本、关键页渲染、源码树及 SHA256 清单：output/cyd28-plus-20261123/vendor-audit/。

## 对照结果

| 项目 | 厂商专用示例 / 资料 | 当前 dev5t2 | 结论 |
|---|---|---|---|
| 音频引脚 | 注释明确GPIO26/DAC2接8002B AUDIO_IN | I2S LEFT_EN / DAC_CHAN_1 指向GPIO26 | 一致；AUDIO_IN是功放输入，不应照GPIO表误设成ADC |
| 功放控制 | GPIO4 LOW，循环每秒再写LOW | GPIO4 LOW，诊断切换期间HIGH静音 | 极性一致，无已知其他任务反复改GPIO4 |
| 通用08示例 | AMP_EN_ACTIVE_HIGH=true，GPIO26用LEDC方波 | 不使用此通用极性与PWM音频 | 与Plus专用示例矛盾，不能作为修正依据 |
| DAC数据格式 | AnalogAudioStream接收有符号PCM，底层转换为无符号DAC格式 | voice_dac_sample返回(q+128)<<8；左右槽复制同一PCM，只启用DAC2 | 没发现符号中点或高低字节颠倒 |
| 声道 | set_mono_downmix(true)；库默认可启用两个DAC | 只启用DAC2 | 不照搬双DAC：GPIO25在本项目用于触摸SPI时钟 |
| 采样/驱动 | A2DP协商采样，依赖库接管初始化；平台版本未锁定 | FMO8kHz；legacy I2S+APLL，诊断音使用RC硬件发生器 | 存在差异，但RC测试亦劈声，不能把APLL定为唯一原因 |
| 音量 | 示例音量127，代码提示失真时改100或90 | FMO数字音量，硬件诊断最低1/8幅度 | 示例警示不等于本机过载证据；1/8是幅度档不是瓦数 |
| 背光 | GPIO21直接数字HIGH | LEDC 10bit/5kHz，100%写1023 | 真实差异；100%并非恒高，仍有窄关断脉冲 |
| 触摸 | 此蓝牙示例没有触摸驱动 | XPT2046轮询GPIO25等 | 是并行外设差异，未证实耦合或引脚冲突 |
| 功放输入/反馈/电源 | 已公开资料未提供电路网表或器件值 | 软件无法确认板上连接 | 不能套用黄色CYD或LCDWIKI E32R28T原理图 |

## 背光差异的定量核对

当前 cyd28_plus_display.c：p*1023/100，10bit，5000Hz。IDF说明10bit占空比完整周期为1024，因此显示100%对应1023/1024（99.90234375%），每200微秒周期理论有约0.1953125微秒低脉冲。较低亮度时开关幅度更明显。该计算已离线核对，见audit-checks.json；实际波形与音频耦合尚未测量，不能宣称为劈声根因。

最小下一步软件对照：在Plus诊断期间使背光真正恒高或关闭，保持音源、功放和喇叭不变，再听同一5秒硬件音。应优先采用受支持的LEDC停止并设置idle level或正确满占空比；完整覆盖正常/调暗/熄屏/唤醒恢复。此建议尚未实施，设备仍是dev5t2。

## 原理图缺口

仍缺与用户这块PCB修订对应的：GPIO26至8002B IN-的耦合及输入阻值、Rf反馈阻值、BYPASS电容、VDD来源与去耦、GPIO4与SHUTDOWN连接、SPK两端连接。照片不能代替连通性测量；完整原理图需要卖家/厂商提供。本轮未联系第三方，也未要求用户改线或焊接。

## 已确认的听感边界

用户确认4Ω与8Ω单元、电池供电、PCM/DMA输出及独立RC硬件音均出现相同劈声。仅能说明该现象不依赖PCM解码/DMA/APLL路径；共用DAC模拟输出、功放、并行外设及供电链路仍未被独立排除。不能据此断定板坏或阻抗不匹配。


## 后续实物对照结论

用户已分别确认：backlight=0关灯播放无劈声；backlight=1恒高、停止PWM播放也无劈声。两次音源、功放和喇叭相同，均由USB触发并观测播放完成。证据支持背光PWM开关为触发条件；具体经供电/地/走线耦合的路径未测量，不能把机理说成已测定。

据此dev5t4只修正Plus背光：任何非零亮度恒高、零亮度恒低，均通过ledc_stop设置idle level，完全停止输出PWM，空闲不再PWM调暗。保留熄屏/触摸唤醒；UI回报实际亮灭两档。音频/DAC/功放处理保持dev5t3一致。测试覆盖所有请求亮度、熄屏/唤醒及诊断结束恢复。升级及听感验收以专项DELIVERY.md为准。
