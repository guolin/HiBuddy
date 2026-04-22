# Buddy 分层与状态约定

本文档只回答四个问题：

- 谁负责什么
- MQTT 传什么
- 宠物属性在哪里计算
- 宠物属性在哪里保存

## 1. 总原则

Buddy V1 只在中间层传递“工作快照”，不传“宠物演出细节”。

中间层传的是：

- 当前整体工作状态
- 多个 session 的压缩状态
- 当前焦点 session
- 简短文案
- token 汇总

中间层不传的是：

- 逐帧动画
- 眨眼、发呆、摇晃、头晕
- UI 布局细节
- 宠物本地随机行为

## 2. 分层职责

### 2.1 CLI

CLI 只负责“工作世界”。

CLI 负责：

- 监听 Codex / Claude 等上游事件
- 维护本地 session 真相
- 聚合出精简 `snapshot`
- 写入本地日志
- 后续发布到 MQTT

CLI 不负责：

- 宠物心情、精力、专注、亲密度
- 宠物随机行为
- 宠物逐帧动画

### 2.2 MQTT

MQTT 只负责传输，不负责业务判断。

MQTT 只传一类消息：

- `snapshot`

需要明确区分：

- Supabase 是控制面
- MQTT Broker 是消息面

CLI 不会通过 Supabase 转发 MQTT，而是先从 Supabase 获取发布参数，再直接连接 MQTT Broker。

### 2.3 设备

设备负责“宠物活态”和页面渲染。

设备负责：

- 根据 `snapshot` 决定当前显示
- 根据工作快照计算宠物短期状态
- 在本地实现发呆、眨眼、摇晃、睡眠等表现
- 处理用户本地互动

设备不负责：

- 重建原始 CLI 事件
- 维护账号和绑定关系

### 2.4 服务端 / Web

服务端只负责“身份、绑定、配置、资源、长期档案”。

服务端负责：

- 注册登录
- 账号与设备绑定
- 账号与 CLI 绑定
- 下发 MQTT endpoint、认证信息、topic 规则
- 下发可发布设备列表
- 存储设备配置
- 存储宠物资源包
- 存储宠物长期属性快照

服务端不负责：

- 实时宠物状态机
- 逐次生成设备动画指令

## 3. MQTT 快照

V1 只推送精简后的 `snapshot`。

它至少应表达三件事：

- 当前整体状态
- 当前焦点 session
- 当前整体工作量

### 3.0 正式字段定义

`snapshot` 建议固定为以下字段：

- `version`
- `updated_at`
- `pet_state`
- `session_states`
- `focus_title`
- `requires_user`
- `total_tokens`
- `today_tokens`

字段说明：

- `version`
  协议版本号，V1 固定为 `1`
- `updated_at`
  当前快照生成时间，使用 Unix 秒级时间戳
- `pet_state`
  当前宠物主状态编码
- `session_states`
  排序后的 session 状态编码数组
- `focus_title`
  当前最值得显示的一句摘要
- `requires_user`
  当前焦点是否需要用户介入
- `total_tokens`
  累计 token 数
- `today_tokens`
  当日 token 数

推荐示例：

```json
{
  "version": 1,
  "updated_at": 1776520000,
  "pet_state": 2,
  "session_states": [2, 1, 0, 0],
  "focus_title": "测试修复已完成，等你 review",
  "requires_user": true,
  "total_tokens": 120000,
  "today_tokens": 34000
}
```

字段含义：

- `pet_state`
  当前宠物应呈现的主状态
- `session_states`
  排序后的 session 状态数组
- `focus_title`
  当前最值得显示的一句摘要
- `requires_user`
  当前焦点是否需要用户介入
- `total_tokens`
  累计 token
- `today_tokens`
  当日 token

### 3.1 正式协议约束

V1 的 `snapshot` 协议补充约束如下：

- `snapshot` 同时作为本地 `device.log` 和 MQTT 的统一负载
- 协议层不裁剪 `session_states` 长度，应该保留完整排序结果
- `focus_title` 是 V1 唯一必需文案字段
- CLI 不生成 `sleep`，CLI 只有在无活跃 session 时生成 `idle`
- `sleep` 属于设备本地演出状态，由设备自行决定是否进入

### 3.2 pet_state 编码

`pet_state` 建议固定为以下编码：

- `0 = idle`
- `1 = thinking`
- `2 = busy`
- `3 = waiting`
- `4 = error`
- `5 = done`
- `6 = sleep`

含义：

- `idle`
  当前无活跃任务
- `thinking`
  有任务在进行，但强度较低
- `busy`
  有明显执行中的工作
- `waiting`
  当前最值得关注的是等待用户处理
- `error`
  当前最值得关注的是错误
- `done`
  刚完成，适合短时间展示完成态
- `sleep`
  长时间无活动或设备进入低活跃状态

补充约定：

- `sleep` 是保留给设备演出层的状态编码
- CLI 的正式 `snapshot` 不主动产出 `sleep`

### 3.3 session 状态数组

`session_states` 用数组表达多个 session 的压缩状态。

例如：

- `[0, 1, 0, 2]`

表示当前有 4 个 session，它们处于 4 个压缩状态。

建议先约定一组稳定编码，例如：

- `0 = thinking`
- `1 = busy`
- `2 = waiting`
- `3 = error`
- `4 = done`

排序规则建议固定：

- `waiting`
- `error`
- `busy`
- `thinking`
- `done`

同状态下按最近更新时间排序。

### 3.4 session_states 编码

`session_states` 建议固定为以下编码：

- `0 = thinking`
- `1 = busy`
- `2 = waiting`
- `3 = error`
- `4 = done`

说明：

- `session_states` 只表达 session 级状态，不包含 `idle`
- 当没有活跃 session 时，`session_states` 为空数组

### 3.5 session 排序规则

`session_states` 必须按固定规则排序，避免设备端理解不一致。

排序优先级建议如下：

1. `waiting`
2. `error`
3. `busy`
4. `thinking`
5. `done`

同优先级下：

- 按最近更新时间降序排列

### 3.6 active session 判定

CLI 必须先判断哪些 session 仍然算“激活”。

推荐规则：

- session 只有在最近 2 小时内收到过更新，才算 active
- 超过 2 小时没有任何更新的 session，不再计入 active session
- 失活 session 不应继续参与 `pet_state`、`session_states`、`focus_title` 和 `requires_user` 计算

这样可以避免历史残留 session 长时间污染当前设备状态。

### 3.7 生成规则

CLI 生成 `snapshot` 时建议遵循以下规则：

- `pet_state` 取当前整体最高优先级状态
- `session_states` 只保留当前 active 或刚完成的压缩状态
- `focus_title` 取当前焦点 session 的一句摘要
- `requires_user = true` 时，焦点必须是用户需要处理的 session
- `total_tokens` 和 `today_tokens` 始终为非负整数

补充：

- 本地调试模式和 MQTT 发布模式必须生成完全相同的 `snapshot`
- 调试模式只是不发送 MQTT，不应改变 `snapshot` 结构或计算逻辑

### 3.8 使用规则

设备消费 `snapshot` 时建议遵循以下规则：

- `pet_state` 决定当前主状态
- `session_states` 只用于辅助显示整体工作分布
- `focus_title` 作为主界面文案的第一来源
- `requires_user` 用于决定是否提高提醒等级
- 如果缺少未知字段，设备应忽略
- 如果 `version` 不支持，设备应回退到兼容模式或显示错误提示

## 4. MQTT topic 规则

建议按设备维度分发：

- `buddy/v1/device/{device_id}/snapshot`

其中：

- CLI 发布到设备 topic
- 设备只订阅自己的 topic
- `snapshot` 使用 retained

## 5. CLI 如何知道发到哪里

CLI 不应在本地硬编码 topic 或设备列表。

推荐流程：

1. CLI 用本地保存的 `cli_key` 请求 Supabase 控制面接口
2. Supabase 校验 `cli_key`
3. Supabase 返回当前绑定的 `device_id`
4. Supabase 返回 MQTT Broker 地址、认证信息、topic 规则和有效期
5. CLI 直接连接 MQTT Broker
6. CLI 向被授权设备的 `snapshot` topic 发布消息

### 5.1 推荐启动接口

建议提供一个统一入口，例如：

- `POST /cli/bootstrap`

最小输入：

- `cli_key`

最小输出：

- `device_id`
- `mqtt_broker_url`
- `mqtt_username`
- `mqtt_password` 或 `mqtt_token`
- `mqtt_client_id`
- `snapshot_topic`
- `active_pet_pack_id`
- `asset_version`
- `pet_name`
- `expires_at`

该接口的作用不是转发消息，而是为 CLI 提供本次发布所需的控制面配置。

## 6. 宠物属性

V1 推荐先只保留四个宠物属性：

- `mood`
- `energy`
- `focus`
- `affection`

### 6.1 属性含义

- `mood`
  最近整体情绪
- `energy`
  当前精力和疲惫程度
- `focus`
  当前专注程度
- `affection`
  与用户的长期亲密度

## 7. 宠物属性在哪里计算

不是所有宠物数据都放在设备端，也不是交给 CLI。

推荐方案是：

- CLI 计算工作快照
- 设备计算短期宠物状态
- 服务端保存长期宠物档案

### 7.1 CLI 提供输入

CLI 只提供宠物属性计算所需的工作输入，例如：

- 当前是否 busy / waiting / error
- 当前 session 数量
- 当前焦点是否需要用户介入
- token 增量
- 是否刚完成任务

### 7.2 设备计算短期状态

设备根据两类输入更新宠物属性：

1. CLI 下发的工作 `snapshot`
2. 设备本地事件

例如：

- 摸一下宠物
- 长时间 idle
- 本地睡眠 / 唤醒
- 按键互动

设备适合计算的是短期变化，例如：

- `mood` 的即时波动
- `energy` 的即时消耗与恢复
- `focus` 的即时升降
- `affection` 的小幅互动变化

## 8. 宠物属性在哪里保存

服务端保存长期档案，设备保存运行态。

### 8.1 设备本地运行态

设备本地维护：

- 当前动画阶段
- 当前是否发呆 / 眨眼 / 摇晃
- 当前短期 `mood`
- 当前短期 `energy`
- 当前短期 `focus`

这些状态可以不上云，或低频同步。

### 8.2 服务端长期档案

服务端保存：

- `mood` 基线
- `energy` 基线
- `focus` 最近值或基线
- `affection`
- 最后同步时间
- 当前激活宠物和资源配置

也就是说：

- 设备负责“活着”
- 服务端负责“记住它”

## 9. 当前推荐结论

当前最稳的 V1 方案是：

- MQTT 只传精简 `snapshot`
- CLI 只负责工作状态
- 设备负责宠物短期状态和表现
- 服务端负责宠物长期档案
- 宠物属性先用 `mood`、`energy`、`focus`、`affection`
