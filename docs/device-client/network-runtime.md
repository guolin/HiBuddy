# Buddy Device Network And Runtime

本文档定义设备端的联网、配对、同步、MQTT、缓存和异常回退。

## 1. 网络职责

设备端网络模块负责：

- Wi-Fi 配网和重连
- 控制面 HTTP 调用
- 资源包下载
- MQTT 连接与订阅
- 周期心跳和宠物版本同步

## 2. 启动联网顺序

推荐顺序：

1. 加载本地 Wi-Fi
2. 尝试连接 Wi-Fi
3. 若失败则进入配网页
4. 联网成功后检查绑定状态
5. 调用 `device-bootstrap`
6. 获取 MQTT 和宠物配置
7. 检查资源包版本
8. 如有需要下载资源包
9. 连接 MQTT
10. 进入运行态

## 3. Wi-Fi 配网流程

推荐流程：

1. 设备进入 `WIFI_PORTAL`
2. 开启热点，例如 `Buddy-XXXX`
3. 显示提示用户用手机连接
4. 用户输入 SSID 和密码
5. 设备保存配置
6. 切回 STA 模式尝试联网
7. 成功后进入下一阶段

### 3.1 Wi-Fi 页面必须显示

- 当前状态
- 热点名
- 配网页地址
- 连接成功或失败反馈

### 3.2 失败回退

- 配网失败回到配网页
- 多次失败后给出清晰提示
- 保留重新输入入口

## 4. 绑定流程

绑定依赖两个核心值：

- `device_id`
- `device_token`

### 4.1 首次绑定流程

1. 调用 `device-pair-code`
2. 保存返回的 `device_token`
3. 屏幕显示 6 位 `pairCode`
4. 提示用户去网页完成绑定
5. 周期轮询 `device-bootstrap`
6. 当 `paired=true` 时进入同步

### 4.2 重新绑定流程

1. 用户从菜单明确触发
2. 再次调用 `device-pair-code`
3. 旧绑定关系失效
4. 旧配置缓存清理
5. 新绑定成功后重新拉配置

## 5. Bootstrap 流程

`device-bootstrap` 是设备运行的关键入口。

设备要从中读取：

- 是否已绑定
- MQTT 连接参数
- 显示配置
- 当前激活宠物

### 5.1 未配对

若返回未配对：

- 不连接 MQTT
- 不进入正常运行页
- 进入绑定引导页

### 5.2 已配对

若返回已配对：

- 保存配置缓存
- 读取 `activePet`
- 进入资源校验

## 6. 宠物资源同步

设备应比较：

- 当前本地 `assetVersion`
- 服务端返回目标 `assetVersion`

### 6.1 不需要更新

- 直接使用本地资源

### 6.2 需要更新

- 下载 manifest
- 下载资源数据
- 校验成功后替换

### 6.3 更新失败

- 保持使用旧版本
- 若没有旧版本，则回退到内置默认宠物

## 7. MQTT 运行模型

V1 设备只订阅一个 topic。

推荐：

- `buddy/v1/device/{device_id}/snapshot`

要求：

- 使用 retained 消息
- 自动重连
- 重连后自动重新订阅

V1 不要求设备通过 MQTT 发送审批动作。

## 8. Snapshot 接收规则

收到 `snapshot` 后设备要做：

1. 更新最后接收时间
2. 写入本地缓存
3. 更新宠物主状态
4. 更新主界面文案
5. 判断是否需要触发提醒页

如果 `snapshot` 表达了 `requires_user = true`，设备应进入 `waiting` 提醒逻辑。

V1 的默认行为是提醒，而不是直接在设备端执行操作。

## 9. Snapshot 到宠物状态的映射

推荐业务映射如下：

- `idle` -> `idle`
- `thinking` -> `thinking`
- `busy` -> `busy`
- `waiting` -> `waiting`
- `done` -> `done`
- `error` -> `error`

设备本地还可二次推导：

- 长时间无更新 -> `offline`
- 很长时间静默且无交互 -> `sleep`

## 10. 本地缓存策略

设备至少缓存以下内容：

- `wifi.json`
- `device.json`
- `config.json`
- `snapshot.json`
- `pet-pack/`

### 10.1 缓存意义

- 重启快速恢复
- 断网仍可运行
- 资源更新失败可回退

## 11. 周期任务

推荐周期任务：

- Wi-Fi 状态检查：`2s ~ 5s`
- MQTT 状态检查：`1s`
- heartbeat：`60s ~ 180s`
- pet-sync：`600s`
- bootstrap 重试：关键阶段 `5s ~ 15s`

## 12. 心跳

`device-heartbeat` 用于向控制面声明设备仍在线。

建议心跳中至少包含：

- `device_id`
- `device_token`
- 当前 firmware version
- 当前 asset version
- 当前网络状态

如果接口当前未要求全部字段，也建议本地保留这套模型。

## 13. 离线降级

以下情况进入 `OFFLINE_DEGRADED`：

- Wi-Fi 断开较久
- MQTT 持续不可用
- bootstrap 长时间失败

降级策略：

- 继续显示本地宠物
- 使用最后一份 `snapshot`
- 页面明显标出离线
- 后台持续重试网络

## 13.1 V2 审批扩展预留

如果未来加入设备侧审批，建议单独增加动作通道，不直接把审批结果塞回 `snapshot`。

可以是：

- 单独 MQTT topic
- 单独 HTTP action endpoint

最小动作模型建议包含：

- `device_id`
- `device_token`
- `prompt_id`
- `action = approve | deny`
- `sent_at`

如果走 MQTT，本质上只是发送一条动作消息。

但该能力属于 V2，因为设备若看不到完整内容就直接同意，体验和安全性都不理想。

## 14. 错误处理原则

### 14.1 配网失败

- 明确显示失败
- 提供重试
- 不应死循环黑屏

### 14.2 绑定超时

- 配对码标记 `expired`
- 提供刷新码

### 14.3 bootstrap 失败

- 显示同步失败
- 使用本地缓存

### 14.4 资源下载失败

- 保持旧资源
- 显示简短提示

### 14.5 MQTT 失败

- 不清空宠物状态
- 进入离线降级

## 15. 运行时实现建议

建议尽量避免阻塞式长操作直接卡住主循环。

例如：

- 网络下载分段执行
- 菜单操作立即响应
- 动画即使在联网时也尽量保持更新

## 16. V1 边界

V1 不要求：

- 多 topic 订阅
- 复杂 QoS 策略设计
- 历史消息回放
- 设备间同步
- 设备侧审批动作发送
