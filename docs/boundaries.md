# Buddy 分层与状态边界

## 1. 总原则

Buddy V1 使用三层语义：

- `snapshot`：工作真相层
- `petStats`：设备本地宠物属性层
- `presentation`：动画与 UI 表现层

这三层必须分开，避免把“AI 在做什么”和“宠物现在演成什么样”混成一个字段。

## 2. Snapshot 层

`snapshot` 由上游生成，通过 MQTT 下发，表示当前 AI 工作事实。

V1 固定字段：

- `version`
- `updated_at`
- `pet_state`
- `session_states`
- `focus_title`
- `requires_user`
- `total_tokens`
- `today_tokens`

`snapshot` 不负责：

- `mood`
- `energy`
- `focus`
- 动画帧选择
- 面朝下睡眠
- 摇晃特效

也就是说，MQTT 不下发宠物属性。

## 3. PetStats 层

`petStats` 是设备本地运行态，负责宠物近期情绪和节奏。

V1 只保留三个属性：

- `mood`
- `energy`
- `focus`

以及少量运行时辅助字段：

- 最近交互时间
- 最近休息时间
- 最近一次友好摇晃时间
- 短时 heart 特效截止时间

来源：

- 最新 `snapshot`
- IMU 摇晃
- 面朝下 nap
- 屏幕睡眠 / 唤醒
- 本地按键和页面交互

## 4. Presentation 层

表现层由设备本地推导，不经过 MQTT。

表现层负责：

- 当前动画状态
- 动画播放节奏
- 是否触发 `heart`
- 是否进入 `sleep`
- 页面上的摘要信息

表现层消费：

- `snapshot`
- `petStats`
- 本地瞬时事件

## 5. 各模块职责

### 上游 / CLI

负责：

- 汇总工作状态
- 生成 `snapshot`
- 发布到设备 topic

不负责：

- 计算 `mood / energy / focus`
- 直接控制设备动画细节

### MQTT

负责：

- 传输 `snapshot`

不负责：

- 宠物属性同步
- 演出逻辑

### 设备

负责：

- 根据 `snapshot` 计算本地 `petStats`
- 根据 `snapshot + petStats` 选择动画和页面表达
- 处理 IMU、按键、睡眠和唤醒

## 6. 页面边界

### Pet 页

- 主视觉是宠物
- 只显示 session 点
- 不显示三属性 bar

### Overview 页

- 主视觉是 `mood / energy / focus`
- 三条 bar 表示强弱
- 不显示具体数字

### Info 页

- 只显示设备信息
- 不承担宠物摘要页职责

## 7. 持久化边界

V1 采用部分持久化：

- `energy` 持久化
- `mood` 不持久化
- `focus` 不持久化

原因：

- `energy` 和睡眠/休息行为关系最强
- `mood` 与 `focus` 更像短期波动
- 可以减少 NVS 写入频率和状态复杂度

## 8. Future Note

`affection` 不属于 V1 正式运行态。

如果后续要做长期养成感，可在 V2 引入：

- `bond`
- `affection`

但它应作为慢变量独立设计，不应塞回当前 `snapshot` 协议，也不应和 `mood` 混用。
