# Buddy Device Technical Implementation

本文档定义设备客户端的技术实现规范。

目标是回答这些问题：

- 代码应该放在哪个仓库和哪个目录
- 固件工程用什么工具链
- 配网应该怎么实现
- 本地存储和资源下载应该怎么组织
- 哪些方案符合当前阶段最稳妥的行业实践

## 1. 当前仓库定位

Buddy 当前已经是一个 monorepo。

设备端固件应继续保留在当前仓库中，而不是单独拆到新仓库。

原因：

- 设备协议与 `docs/`、`packages/shared/`、`supabase/` 强耦合
- 固件、控制面接口、资源格式需要一起演进
- hackathon 和早期产品阶段更适合单仓协作

## 2. 推荐仓库结构

当前建议继续采用如下结构：

```text
buddy/
  apps/
  docs/
  firmware/
    m5sticks3/
  packages/
    shared/
    asset-pack/
  supabase/
```

其中：

- `docs/`
  产品、架构、设备规格文档
- `firmware/m5sticks3/`
  设备固件工程
- `packages/shared/`
  共享协议定义
- `packages/asset-pack/`
  资源包格式与工具
- `supabase/`
  控制面接口和迁移脚本

## 3. 固件工程推荐技术栈

V1 推荐：

- 平台：`ESP32-S3`
- 设备：`M5StickS3`
- 语言：`C++`
- SDK 层：`Arduino on ESP32`
- 构建工具：`PlatformIO`
- 显示和板级封装：`M5Unified`
- 文件系统：`LittleFS`
- 消息协议：`MQTT`
- 控制面接口：`HTTPS + JSON`

## 4. 为什么 V1 继续使用 Arduino + PlatformIO

这不是“最底层最强大”的方案，但它是当前阶段最稳妥的方案。

原因：

- 当前仓库已经是 `PlatformIO + Arduino`
- `M5StickS3` 的板级支持成熟
- `M5Unified` 能比较顺手地拿到屏幕、按键、电源和 IMU
- 团队协作时 `PlatformIO` 比 Arduino IDE 更稳定
- 对当前 UI、动画、Wi-Fi、MQTT 需求已经足够

## 5. 什么时候考虑 ESP-IDF

如果未来出现这些需求，再考虑迁移或局部采用 `ESP-IDF`：

- 更严格的任务调度和资源控制
- 官方 `wifi_provisioning` 组件深度接入
- 更复杂的 OTA
- 更高强度的 BLE 配网
- 更复杂的电源和内存优化

V1 不建议一开始就切到纯 `ESP-IDF`，否则会明显增加复杂度。

## 6. firmware 目录建议细分

当前 `firmware/m5sticks3/` 只有一个 `src/main.cpp`，后续建议拆分成如下结构：

```text
firmware/m5sticks3/
  platformio.ini
  README.md
  include/
    app_config.h
    app_types.h
  src/
    main.cpp
    app/
      app.cpp
      app.h
    board/
      board_init.cpp
      board_init.h
      input.cpp
      input.h
      imu.cpp
      imu.h
    ui/
      router.cpp
      router.h
      screens_home.cpp
      screens_menu.cpp
      screens_system.cpp
      status_lights.cpp
    pet/
      pet_runtime.cpp
      pet_runtime.h
      pet_pack.cpp
      pet_pack.h
    net/
      wifi_manager.cpp
      wifi_manager.h
      api_client.cpp
      api_client.h
      mqtt_client.cpp
      mqtt_client.h
    storage/
      settings_store.cpp
      settings_store.h
      asset_store.cpp
      asset_store.h
```

这样拆分的好处：

- 模块边界清晰
- 更适合多人协作
- 更适合 AI 分工

## 7. 配网方案选择

ESP32 行业内比较常见的方案主要有两种：

- `SoftAP + Captive Portal`
- `BLE Provisioning`

### 7.1 V1 推荐方案

V1 推荐：

- `SoftAP + Captive Portal + 本地网页配置`

不推荐 V1 一开始就做：

- BLE Provisioning

### 7.2 为什么 V1 选 SoftAP

这是对当前产品形态最稳的方案。

原因：

- 用户不需要安装 App
- 用户只需要手机浏览器
- 更适合 hackathon 和快速 Demo
- 和“设备显示热点名 + 手机打开配置页”的体验匹配
- 调试和排障成本更低

### 7.3 为什么暂不优先 BLE Provisioning

虽然 ESP 官方提供了成熟的 BLE/SoftAP provisioning 组件，但在当前产品形态下，BLE 不是最省事的路径。

原因：

- BLE 配网更适合配合专门 App
- 浏览器端 BLE 兼容性和流程更复杂
- 调试难度更高
- 对当前“用户用手机网页配置”目标帮助有限

### 7.4 行业实践建议

按照 ESP 官方 provisioning 思路，无论使用 SoftAP 还是 BLE，都应该遵守以下原则：

- 配网流程和主业务流程解耦
- 设备端明确区分“未配网”和“已配网”
- 配网结束后释放多余服务和资源
- 凭据写入持久化存储
- 支持重置和重新配网

## 8. V1 配网实现建议

建议实现方式：

1. 设备无 Wi-Fi 配置时进入 `WIFI_PORTAL`
2. 打开热点，例如 `Buddy-AB12`
3. 启动本地 HTTP Server
4. 提供简单网页：
   - 扫描到的 SSID 列表
   - 密码输入框
   - 保存按钮
5. 用户提交后写入本地存储
6. 设备切回 STA 模式尝试联网
7. 成功后关闭热点和本地页面

### 8.1 Captive Portal 是否必须

V1 不要求必须做到完整系统级 captive portal 劫持体验。

最小可行实现：

- 设备显示热点名和访问地址
- 手机连上热点后打开一个固定地址

如果后续时间足够，再补真正的 captive portal 自动跳转。

## 9. 本地存储建议

推荐分层存储：

- `NVS`
  存少量配置和状态
- `LittleFS`
  存资源包和较大 JSON 文件

### 9.1 NVS 适合存什么

- Wi-Fi SSID
- Wi-Fi 密码
- `device_id`
- `device_token`
- 亮度设置
- 音量设置
- 最近 UI 模式

### 9.2 LittleFS 适合存什么

- `config.json`
- `snapshot.json`
- `pet-pack/manifest.json`
- `palette.bin`
- `frames.bin`
- 下载中的临时文件

## 10. 分区建议

V1 建议不要继续使用默认思路不清晰的临时分区，而是尽快明确一份设备专用分区表。

建议至少考虑这些空间：

- app 分区
- OTA 备用分区
- NVS
- LittleFS

如果 V1 还不做完整 OTA，也建议至少预留足够的 `LittleFS` 空间给宠物资源。

## 11. MQTT 客户端建议

V1 推荐遵守以下原则：

- 只订阅一个设备专属 topic
- `snapshot` 使用 retained
- 自动重连
- 重连自动订阅
- 不依赖历史事件回放

本地代码上建议封装成独立模块：

- 连接状态
- 最近消息时间
- 最近 topic
- 最近错误原因

## 12. HTTP API 客户端建议

设备端 HTTP 客户端建议统一封装：

- Base URL
- 请求超时
- JSON parse
- token 注入
- 错误码映射
- 指数退避重试

不要在页面代码里直接发 HTTP 请求。

## 13. 资源包下载建议

V1 下载策略建议为：

- 先拉 manifest
- 对比 `assetVersion`
- 再拉资源数据
- 下载到临时目录
- 校验通过后原子切换

建议支持：

- 中断恢复
- 旧版本回退
- 下载失败继续使用当前资源

## 14. 动画资源格式建议

V1 推荐最终运行时格式为自定义帧包，而不是 GIF。

建议：

- 索引色 palette
- 顺序帧数据
- JSON manifest

原因：

- 读取更直接
- 性能更稳定
- 状态切换更可控
- 比 GIF 更适合做轻量状态机

## 15. UI 代码实践建议

设备端 UI 不建议写成“全局 if-else 巨函数”。

建议拆成：

- Screen enum
- Router
- 每个页面一个 render 函数
- 每个页面一个 handleInput 函数

这样后续增加：

- 主界面
- 菜单
- 配网页
- 配对页
- 帮助页

都会更容易维护。

## 16. IMU 实现建议

V1 只把 IMU 用于轻交互：

- `shake`
- `face-down`
- `pick-up`

实现建议：

- 使用阈值和滞回，避免抖动误判
- 不要每帧直接触发动作
- 引入冷却时间

不要用 IMU 做：

- 审批
- 删除配置
- 高风险确认

## 17. 代码风格建议

建议统一以下规则：

- 模块按职责拆文件
- 避免过大类
- 避免在 ISR 或高频路径中做复杂 IO
- 状态结构体统一收口
- 页面层不直接访问底层网络细节
- 网络层不直接绘制 UI

## 18. 调试建议

V1 必备调试能力：

- 串口日志
- 当前全局状态输出
- Wi-Fi 状态输出
- MQTT 状态输出
- 当前 asset version 输出
- 当前 `snapshot` 时间戳输出

建议支持一个简单的 debug overlay 或 debug build。

## 19. 推荐实施顺序

建议按这个顺序实现：

1. `board + screen + input`
2. 主界面和路由
3. session 状态灯
4. SoftAP 配网
5. 绑定流程
6. bootstrap
7. 本地资源加载
8. MQTT 接入
9. IMU 轻交互
10. 资源下载和回退

## 20. V1 技术结论

V1 推荐的最稳妥技术路线是：

- 单仓 monorepo 继续推进
- 固件继续放在 `firmware/m5sticks3`
- `PlatformIO + Arduino + M5Unified`
- `SoftAP + 本地网页` 配网
- `NVS + LittleFS` 双层存储
- `HTTPS + MQTT` 双通道
- 资源包走自定义帧格式

这是当前阶段更接近行业成熟做法、同时也更符合你这个产品目标的实现方式。
