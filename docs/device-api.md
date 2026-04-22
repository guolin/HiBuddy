# Buddy Device Control Plane API

这份文档面向设备固件、嵌入式客户端和设备调试工具开发者。

目标是说明 Buddy 设备端当前可用的控制面接口，包括：

- 设备如何申请配对码
- 设备如何判断自己是否已配对
- 设备如何获取 MQTT 和宠物信息
- 设备如何周期同步宠物版本
- 设备如何上报心跳

## 1. Base URL

所有接口都部署为 Supabase Edge Functions。

生产环境调用形式：

```text
https://<your-project-ref>.supabase.co/functions/v1/<function-name>
```

当前项目中已经实现的设备接口：

- `POST /functions/v1/device-pair-code`
- `POST /functions/v1/device-bootstrap`
- `POST /functions/v1/device-pet-sync`
- `POST /functions/v1/device-heartbeat`

当前项目中已经实现的 CLI 控制面接口：

- `POST /functions/v1/issue-cli-key`
- `POST /functions/v1/cli-bootstrap`

## 2. 设备身份模型

设备端当前使用两层身份：

1. `device_id`
   设备的长期公开标识，例如 `demo`

2. `device_token`
   服务端为这台设备签发的设备访问 token

规则：

- 设备首次知道自己的 `device_id` 即可申请配对码
- 服务端会在申请配对码时返回 `device_token`
- 后续 `bootstrap`、`pet-sync`、`heartbeat` 都应携带 `device_token`
- 如果设备重新申请配对码，旧配对关系会被清掉
- 重新配对后，旧配对码立即失效

## 3. 典型调用流程

设备启动后的推荐顺序：

1. 调用 `device-pair-code`
2. 在屏幕上显示 `pairCode`
3. 用户在网页侧完成绑定
4. 设备轮询 `device-bootstrap`
5. 当返回 `paired=true` 后，读取 MQTT 和宠物配置
6. 设备连接 MQTT
7. 设备每 10 分钟调用一次 `device-pet-sync`
8. 设备每 1 到 3 分钟调用一次 `device-heartbeat`

CLI 启动后的推荐顺序：

1. 用户先在 Web 控制台登录
2. Web 调用 `issue-cli-key` 生成 CLI key
3. CLI 将 key 保存到本地配置
4. CLI 调用 `cli-bootstrap`
5. CLI 获取 MQTT 地址、用户名密码、topic、宠物摘要
6. CLI 连接 MQTT
7. CLI 向返回的 topic 发布 snapshot

## 4. API 1: `device-pair-code`

为设备申请一个新的短期配对码。

### 用途

- 设备首次配对
- 已配对设备主动重新配对
- 用户更换账号后重新认领设备

### 行为规则

- 重新申请配对码时，旧配对关系会被删除
- 重新申请配对码时，旧 `device_config` 会被清掉
- 服务端会保留或生成新的 `device_token`
- 新配对码默认 5 分钟过期

### Request

`POST /functions/v1/device-pair-code`

```json
{
  "device_id": "demo"
}
```

也可以带上旧 token：

```json
{
  "device_id": "demo",
  "device_token": "dev_xxx"
}
```

### Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceToken": "dev_8b031de306aaefdc53b5e651ca4db0a62360363565cbacbd",
  "pairCode": "038271",
  "pairUrl": "http://localhost:5173/auth?pair_code=038271&device_id=demo",
  "expiresAt": "2026-04-21T01:58:51.980035+00:00"
}
```

### 字段说明

- `deviceId`
  设备 ID
- `deviceToken`
  后续设备接口使用的访问 token
- `pairCode`
  屏幕上显示给用户输入的 6 位纯数字配对码
- `pairUrl`
  可选的快捷配对链接
- `expiresAt`
  配对码过期时间

### 失败响应

可能返回：

- `404 DEVICE_NOT_FOUND`
- `500 PAIR_CODE_CREATE_FAILED`
- `500 PAIR_CODE_EMPTY_RESULT`

## 5. API 2: `device-bootstrap`

设备启动后拉取当前运行所需的控制面信息。

### 用途

- 判断设备是否已配对
- 获取 MQTT 连接参数
- 获取当前宠物信息
- 获取设备显示配置

### Request

`POST /functions/v1/device-bootstrap`

```json
{
  "device_id": "demo",
  "device_token": "dev_8b031de306aaefdc53b5e651ca4db0a62360363565cbacbd"
}
```

### 未配对 Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceType": "demo-simulator",
  "paired": false,
  "lastSeenAt": null,
  "lastPetSyncAt": null,
  "mqtt": null,
  "config": null,
  "activePet": null
}
```

### 已配对 Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceType": "demo-simulator",
  "paired": true,
  "lastSeenAt": null,
  "lastPetSyncAt": null,
  "mqtt": {
    "mqttBrokerUrl": "mqtts://ifa56b35.ala.cn-hangzhou.emqxsl.cn:8883",
    "mqttUsername": "buddy",
    "mqttToken": "hibuddy",
    "mqttClientId": "buddy-cli-demo",
    "mqttTopic": "buddy/v1/device/demo"
  },
  "config": {
    "petName": "Buddy",
    "displayProfile": "rich",
    "soundEnabled": true,
    "assetAutoUpdate": true
  },
  "activePet": {
    "petPackId": "chief-v1",
    "name": "Chief",
    "slug": "chief-v1",
    "assetVersion": "1.0.0",
    "assetManifestUrl": "https://buddy-assets.example.com/packs/chief-v1/manifest.json",
    "coverImageUrl": "https://buddy-assets.example.com/packs/chief-v1/cover.png",
    "petName": "Buddy"
  }
}
```

### 字段说明

- `paired`
  设备当前是否已经绑定到用户
- `mqtt`
  MQTT 连接参数，仅在已配对时返回
- `config.petName`
  设备上显示的宠物名
- `config.displayProfile`
  当前 UI 展示模式，取值为 `rich`、`compact`、`text-only`
- `activePet.assetManifestUrl`
  设备应拉取的宠物资源清单地址
- `activePet.assetVersion`
  设备当前目标宠物资源版本

### 失败响应

可能返回：

- `401 INVALID_DEVICE_TOKEN`
- `404 DEVICE_NOT_FOUND`
- `500 DEVICE_RUNTIME_LOOKUP_FAILED`

## 6. API 3: `device-pet-sync`

设备周期同步当前本地宠物版本。

推荐每 10 分钟调用一次。

### 用途

- 告诉服务端设备当前使用的宠物包版本
- 让服务端判断是否需要更新
- 写入 `last_pet_sync_at`

### Request

`POST /functions/v1/device-pet-sync`

```json
{
  "device_id": "demo",
  "device_token": "dev_8b031de306aaefdc53b5e651ca4db0a62360363565cbacbd",
  "current_pet_pack_id": "chief-v1",
  "current_pet_version": "0.9.0"
}
```

### Response

```json
{
  "ok": true,
  "paired": true,
  "hasUpdate": true,
  "syncedAt": "2026-04-21T01:53:54.747Z",
  "nextSyncAfterSeconds": 600,
  "activePet": {
    "petPackId": "chief-v1",
    "name": "Chief",
    "slug": "chief-v1",
    "assetVersion": "1.0.0",
    "assetManifestUrl": "https://buddy-assets.example.com/packs/chief-v1/manifest.json",
    "coverImageUrl": "https://buddy-assets.example.com/packs/chief-v1/cover.png",
    "petName": "Buddy"
  }
}
```

### 语义说明

- `paired=false`
  说明设备当前没有绑定用户，此时 `activePet` 为空
- `hasUpdate=true`
  表示设备当前持有的宠物包 ID 或版本和服务端不一致
- `nextSyncAfterSeconds`
  服务端建议的下一次同步间隔，当前固定为 `600`

### 设备端建议

收到 `hasUpdate=true` 后：

1. 下载 `activePet.assetManifestUrl`
2. 拉取资源包
3. 原子切换到新版本
4. 下次 `device-pet-sync` 上报新版本

### 失败响应

可能返回：

- `400 INVALID_REQUEST`
- `401 INVALID_DEVICE_TOKEN`
- `404 DEVICE_NOT_FOUND`
- `500 PET_SYNC_UPDATE_FAILED`

## 7. API 4: `device-heartbeat`

设备上报在线状态和运行状态。

推荐每 1 到 3 分钟调用一次。

### 用途

- 更新 `last_seen_at`
- 上报固件版本
- 上报最近一次 MQTT 连通状态
- 帮助后台判断设备是否在线

### Request

`POST /functions/v1/device-heartbeat`

```json
{
  "device_id": "demo",
  "device_token": "dev_8b031de306aaefdc53b5e651ca4db0a62360363565cbacbd",
  "firmware_version": "demo-fw-1.0.0",
  "ip": "192.168.31.207",
  "mqtt_connected": true,
  "current_pet_pack_id": "chief-v1",
  "current_pet_version": "1.0.0"
}
```

### Response

```json
{
  "ok": true,
  "receivedAt": "2026-04-21T01:53:55.422Z"
}
```

### 字段说明

- `firmware_version`
  当前设备固件版本
- `ip`
  当前局域网 IP 或网络侧标识
- `mqtt_connected`
  当前是否成功连到 MQTT
- `current_pet_pack_id`
  当前设备本地激活宠物包 ID
- `current_pet_version`
  当前设备本地资源版本

### 失败响应

可能返回：

- `400 INVALID_REQUEST`
- `401 INVALID_DEVICE_TOKEN`
- `404 DEVICE_NOT_FOUND`
- `500 HEARTBEAT_UPDATE_FAILED`

## 8. MQTT 字段说明

当前 `device-bootstrap` 返回的 MQTT 字段如下：

- `mqttBrokerUrl`
  MQTT over TLS 地址，例如 `mqtts://host:8883`
- `mqttUsername`
  MQTT 用户名
- `mqttToken`
  MQTT 密码或 token
- `mqttClientId`
  设备连接 broker 时使用的 client id
- `mqttTopic`
  设备应订阅的 snapshot topic

当前项目约定：

- 设备只订阅自己的 topic
- topic 格式为 `buddy/v1/device/{device_id}`
- CLI 或控制端负责向该 topic 发布 retained snapshot

## 9. CLI 认证模型

CLI 当前不直接使用用户登录态访问 MQTT。

而是采用两段式模型：

1. Web 登录用户后生成 `cliKey`
2. CLI 使用 `cliKey` 向服务端换取 MQTT bootstrap 配置

规则：

- 一个用户当前只保留一个有效 CLI key
- 重新生成 CLI key 时，旧 key 会失效
- CLI key 只用于调用 `cli-bootstrap`
- CLI 真正连接 MQTT 时使用的是 `cli-bootstrap` 返回的 MQTT 参数

## 10. API 5: `issue-cli-key`

为当前登录用户生成一把新的 CLI key。

### 用途

- CLI 首次接入
- 用户主动轮换 CLI key
- 旧 key 泄漏后的重新签发

### 鉴权方式

必须带用户登录态 `Authorization: Bearer <access_token>`。

这个接口是给 Web 控制台或已登录客户端调用的，不是给裸设备调用的。

### Request

`POST /functions/v1/issue-cli-key`

请求体为空：

```json
{}
```

### Response

```json
{
  "ok": true,
  "cliKey": "buddy_0123456789abcdef0123456789abcdef0123",
  "createdAt": "2026-04-21T01:03:39.000Z"
}
```

### 字段说明

- `cliKey`
  CLI 应保存到本地配置文件中的控制面 key
- `createdAt`
  该 key 的签发时间

### 行为规则

- 服务端会先删除该用户旧的 CLI key
- 然后插入新的 CLI key
- 因此这个接口天然是“轮换 key”语义，不是“追加多个 key”

### 失败响应

可能返回：

- `401 UNAUTHORIZED`
- `500 CLI_KEY_DELETE_FAILED`
- `500 CLI_KEY_CREATE_FAILED`

## 11. API 6: `cli-bootstrap`

CLI 使用 `cli_key` 换取当前可用的 MQTT 连接参数和绑定设备信息。

### 用途

- 校验 CLI key 是否有效
- 查询当前用户绑定的设备
- 获取 MQTT broker 地址、用户名密码、topic
- 获取当前宠物摘要，供 CLI 日志或状态展示使用

### Request

`POST /functions/v1/cli-bootstrap`

```json
{
  "cli_key": "buddy_0123456789abcdef0123456789abcdef0123"
}
```

### Response

```json
{
  "ok": true,
  "device_id": "demo",
  "mqtt_broker_url": "mqtts://ifa56b35.ala.cn-hangzhou.emqxsl.cn:8883",
  "mqtt_client_id": "buddy-cli-demo",
  "mqtt_username": "buddy",
  "mqtt_token": "hibuddy",
  "snapshot_topic": "buddy/v1/device/demo",
  "active_pet_pack_id": "chief-v1",
  "asset_version": "1.0.0",
  "pet_name": "Buddy",
  "expires_at": "2026-04-21T02:03:39.705Z"
}
```

### 字段说明

- `device_id`
  当前 CLI 绑定的目标设备 ID
- `mqtt_broker_url`
  CLI 要连接的 MQTT over TLS 地址
- `mqtt_client_id`
  CLI 连接 broker 时使用的 client id
- `mqtt_username`
  MQTT 用户名
- `mqtt_token`
  MQTT 密码或 token
- `snapshot_topic`
  CLI 应发布 snapshot 的 topic
- `active_pet_pack_id`
  当前设备激活宠物包 ID
- `asset_version`
  当前宠物资源版本
- `pet_name`
  当前用户给宠物设定的显示名
- `expires_at`
  这次 bootstrap 信息的建议失效时间

### 语义说明

- `cli-bootstrap` 不负责设备绑定
- `cli-bootstrap` 只返回“这个 CLI key 当前可控制哪台设备”
- 如果用户没有绑定设备，CLI 不应继续发 MQTT

### 失败响应

可能返回：

- `400 INVALID_REQUEST`
- `401 INVALID_CLI_KEY`
- `404 DEVICE_NOT_BOUND`
- `500 CLI_KEY_LOOKUP_FAILED`
- `500 DEVICE_LOOKUP_FAILED`

## 12. CLI 本地接入建议

CLI 本地至少应保存：

- `apiBaseUrl`
- `cliKey`
- 最近一次 `cli-bootstrap` 返回

推荐策略：

1. CLI 启动时先调用 `cli-bootstrap`
2. 若返回成功，则建立 MQTT 连接
3. 发布 snapshot 时使用 `snapshot_topic`
4. 如果 broker 连接失败，可重新拉一次 `cli-bootstrap`
5. 如果返回 `INVALID_CLI_KEY`，提示用户重新生成 key

## 13. 错误码约定

设备端开发时应优先根据 `code` 判断错误，而不是只看 `error` 文案。

常见错误码：

- `INVALID_REQUEST`
  请求体字段缺失或格式错误
- `DEVICE_NOT_FOUND`
  设备 ID 不存在
- `INVALID_DEVICE_TOKEN`
  设备 token 不合法或已过期
- `PAIR_CODE_CREATE_FAILED`
  服务端生成配对码失败
- `DEVICE_RUNTIME_LOOKUP_FAILED`
  服务端查询设备控制面数据失败
- `PET_SYNC_UPDATE_FAILED`
  写入宠物同步状态失败
- `HEARTBEAT_UPDATE_FAILED`
  写入心跳失败
- `INTERNAL_ERROR`
  未分类服务端错误

标准错误响应格式：

```json
{
  "ok": false,
  "error": "Invalid device token.",
  "code": "INVALID_DEVICE_TOKEN"
}
```

## 14. 设备本地缓存建议

设备至少应在本地缓存以下内容：

- `device_id`
- `device_token`
- 最近一次 `bootstrap` 返回
- 当前宠物资源包
- 当前宠物版本
- 最近一次成功连接的 MQTT 参数

推荐策略：

- `bootstrap` 成功后覆盖缓存
- 网络失败时优先使用缓存启动
- 资源下载失败时继续使用旧版本
- 如果 `device_token` 失效，重新走 `device-pair-code`

## 15. 当前限制

当前实现已经支持设备侧 4 个接口，但仍有两个现实边界：

1. `pairUrl` 已经返回，但 Web 侧尚未完成“用户打开链接后自动消费 `pair_code` 完成配对”的闭环
2. MQTT 凭证当前由 Supabase secrets 下发，还是项目级共享凭证，不是每台设备独立账号

因此当前推荐接入方式是：

- 设备显示 `pairCode`
- 用户仍可通过现有 Web 控制面完成设备认领
- 配对成功后设备通过 `device-bootstrap` 获取 MQTT 参数

## 16. Curl 示例

### 申请配对码

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/device-pair-code" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"demo"}'
```

### 拉取 bootstrap

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/device-bootstrap" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"demo","device_token":"dev_xxx"}'
```

### 生成 CLI key

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/issue-cli-key" \
  -H "Authorization: Bearer <user-access-token>" \
  -H "Content-Type: application/json" \
  -d '{}'
```

### CLI bootstrap

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/cli-bootstrap" \
  -H "Content-Type: application/json" \
  -d '{"cli_key":"buddy_xxx"}'
```

### 宠物同步

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/device-pet-sync" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"demo","device_token":"dev_xxx","current_pet_pack_id":"chief-v1","current_pet_version":"1.0.0"}'
```

### 心跳上报

```bash
curl -X POST "https://<project>.supabase.co/functions/v1/device-heartbeat" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"demo","device_token":"dev_xxx","firmware_version":"fw-1.0.0","mqtt_connected":true}'
```
