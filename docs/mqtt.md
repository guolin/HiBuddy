# Buddy MQTT

## 当前定位

MQTT 是后续接入的传输层，不负责状态计算。

当前已实现的第一条链路仍是本地：

- CLI 写入 `~/.buddy/device.log`
- 本地 reader 读取该日志做预览

后续接入 MQTT 时，只替换传输方式，不改变语义模型。

也就是说：

- 本地调试模式生成 `snapshot` 并写入 `device.log`
- 发布模式生成同一个 `snapshot`，再额外发到 MQTT
- MQTT 是可切换输出通道，不是另一套状态协议

## 传输内容

V1 只传一类消息：

- `snapshot`

MQTT 不传：

- 宠物逐帧动画
- 宠物本地随机行为
- 复杂会话原始事件

## 推荐 topic

- `buddy/v1/device/{device_id}/snapshot`

规则：

- CLI 发布到设备 topic
- 设备只订阅自己的 topic
- `snapshot` 使用 retained

## 谁负责什么

- CLI 负责生成精简 `snapshot`
- MQTT 负责转发 `snapshot`
- 设备负责根据 `snapshot` 渲染
- 服务端负责下发 MQTT 连接信息和可发布设备列表

## CLI 接入顺序

CLI 与 Supabase、MQTT 的顺序应明确分开：

1. CLI 先请求 Supabase 控制面
2. Supabase 返回绑定的 `device_id`
3. Supabase 返回 MQTT Broker 地址、认证信息和 topic
4. CLI 再直接连接 MQTT Broker
5. CLI 向 `buddy/v1/device/{device_id}/snapshot` 发布精简快照

也就是说：

- Supabase 不负责代发 MQTT
- MQTT Broker 不负责账号和绑定关系
- CLI 先拿配置，再发消息

## 稳定原则

即使接入 MQTT，下列原则也不变：

- CLI 仍负责工作状态聚合
- 设备仍只消费精简快照
- 服务端仍是控制面，不是实时状态机
- 本地调试与 MQTT 发布使用同一种 `snapshot` 协议
