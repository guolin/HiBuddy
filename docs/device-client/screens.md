# Buddy Device Screen Specification

本文档详细定义设备端页面内容，包括：

- 三个主界面
- 配网页
- 配对页
- 菜单页
- 信息页
- 帮助页
- 错误与提醒页

目标是让 UI、固件、文案三方能基于同一份页面规格协作。

## 1. 设计原则

页面内容必须满足：

- 小屏可读
- 信息层级明确
- 先看宠物，再看重点
- 文案短
- 状态统一

V1 默认原则：

- 主界面尽量少文字
- 不依赖中文动态文案
- 用宠物、灯、角标替代句子

## 2. 主界面总规则

主界面永远只显示最重要的内容。

不要出现：

- 长日志
- 大段诊断信息
- 多层复杂表格

## 3. Image 页面

## 3.1 目标

适合远距离一眼看宠物状态。

## 3.2 页面内容

- 中央大尺寸宠物
- 右上角小状态点

V1 可不显示底部状态词。

## 3.3 建议布局

- 宠物显示区：主画面大部分区域
- 状态点：屏幕角落
- 底栏：默认留空，调试模式下可显示一行状态词

## 3.4 建议文案

- `Idle`
- `Thinking`
- `Busy`
- `Waiting`
- `Done`
- `Error`
- `Sleep`
- `Offline`

说明：

- 这些词主要用于开发调试版
- 正式版主界面可不显示

## 4. Mixed 页面

## 4.1 目标

作为默认页面，兼顾宠物和信息。

## 4.2 页面内容

- `128x128` 宠物显示区
- session 状态灯
- 小型网络角标

V1 推荐 Mixed 作为默认常驻页。

## 4.3 session 状态灯建议

建议最多显示前 `4` 个 session。

颜色映射：

- 灰色：空位
- 蓝色：thinking
- 黄色：busy
- 橙色：waiting
- 红色：error
- 绿色：done

灯效建议：

- `thinking`：常亮
- `busy`：缓慢呼吸
- `waiting`：明显脉冲
- `error`：快速闪烁
- `done`：短暂闪亮

## 5. Info 页面

## 5.1 目标

用于排查、确认配置和查看运行态。

## 5.2 页面内容

- `Pet State`
- `Session Lights`
- `Session Count`
- `Wi-Fi`
- `MQTT`
- `Device ID`
- `Firmware`
- `Asset Version`
- `Today Tokens`

## 5.3 建议分页

如果一屏放不下，按分页展示：

- 第 1 页：状态与摘要
- 第 2 页：网络信息
- 第 3 页：设备信息

## 6. 配网页

## 6.1 页面目标

指导用户完成 Wi-Fi 配置。

## 6.2 页面内容

- 标题：`Wi-Fi Setup`
- 热点名
- 配网页地址
- 当前步骤
- 操作提示

## 6.3 成功状态

显示：

- `Saved`
- `Connecting...`

## 6.4 失败状态

显示：

- `Connect failed`
- `Try again`

## 7. 配对页

## 7.1 页面目标

指导用户完成设备绑定。

## 7.2 页面内容

- 标题：`Pair Device`
- 六位绑定码
- 剩余有效时间
- 当前状态
- 简要步骤

## 7.3 状态枚举

- `Pending`
- `Bound`
- `Expired`
- `Refreshing`
- `Failed`

## 7.4 配对页示例文案

- `Open buddy web`
- `Enter this code`
- `Code expires in 04:32`

## 8. 菜单页

## 8.1 内容要求

菜单项必须：

- 短
- 易懂
- 不超过一行

## 8.2 菜单项列表

- `Brightness`
- `Volume`
- `Wi-Fi`
- `Pair`
- `Pet`
- `Info`
- `Help`
- `Restart`
- `Back`

## 8.3 选中态

建议：

- 当前项前加 `>`
- 当前项反色或高亮

## 9. Pet 页面

V1 即使只有一个宠物，也建议保留该页。

显示：

- 宠物名
- 资源版本
- 下载状态
- 最后同步时间

操作：

- `Re-download`
- `Back`

## 10. Info 子页

建议具体字段：

- `device_id`
- `firmware_version`
- `asset_pack_id`
- `asset_version`
- `wifi_ssid`
- `wifi_rssi`
- `mqtt_state`
- `last_snapshot_at`
- `last_heartbeat_at`

## 11. Help 页面

建议分三页：

- 配网说明
- 绑定说明
- 按键说明

示例：

- `A short: next`
- `A long: enter`
- `B short: back`
- `B long: system`

## 12. 提醒页

提醒页只在关键状态变化时出现。

### 12.1 waiting

显示：

- 宠物警觉态
- 强提醒灯效
- 可选极短图标

说明：

- V1 的 `waiting` 只负责提醒用户当前需要处理
- 不要求设备展示审批详情
- 不要求设备直接给出批准或拒绝

### 12.2 done

显示：

- 宠物开心态
- `Finished`
- 焦点摘要

### 12.3 error

显示：

- 宠物报错态
- `Problem found`
- 焦点摘要

### 12.4 stale

显示：

- 宠物低活跃态
- `No updates`

## 13. 离线页

离线不是致命错误，但要让用户明显感知。

显示：

- 离线宠物状态
- `Offline`
- 最近一次更新时间

## 14. 错误页

只有在关键错误时才进入全页错误。

适用情况：

- 资源损坏且无默认资源
- 系统初始化失败
- 本地存储不可用

必须显示：

- 错误标题
- 简短原因
- 下一步建议

## 15. 文案规范

文案必须满足：

- 短
- 明确
- 不拟人过头
- 不暴露内部术语

推荐：

- `Need review`
- `1 issue found`
- `Syncing`
- `Wi-Fi lost`

避免：

- `The MQTT bootstrap retry process failed`
- `Snapshot decode mismatch`

补充：

- V1 正式版主界面应尽量减少这些文案的实际出现频率
- 菜单和帮助页可以保留简短英文

## 16. 视觉层级

一屏内最多只保留三层优先级：

1. 宠物或核心状态
2. 当前最重要的一行文字
3. 辅助信息

不要在小屏上一口气展示过多信息。
