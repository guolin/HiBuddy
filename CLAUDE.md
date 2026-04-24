# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 协作偏好

- 默认使用中文与用户沟通，除非用户明确要求英文。
- 命令、代码、API 字段名、日志和标识符保留原文，必要时再补中文说明。

## 常用命令
PIO的路径为`~/.platformio/penv/bin/pio`

- 主要固件目标板是 `M5StickC`；`platformio.ini` 里也定义了 `M5StickC`。
- 构建固件：
  - `pio run -e M5StickS3`
  - `pio run -e M5StickC`
- 烧录固件：
  - `pio run -e M5StickC -t upload`
- 打开串口监视器：
  - `pio device monitor -b 115200`
  - 如有需要，补 `--port /dev/cu.usbmodem...` 或其它设备路径。
- 清理单个环境的构建产物：
  - `pio run -e M5StickC -t clean`
- 当前仓库没有提交自动化测试目录（`test/` 不存在），因此也没有单测/单文件测试工作流。

### Pet pack 工具链

- 安装 Python 依赖：
  - `python -m pip install -r tools/pet_pack_builder/requirements.txt`
- 生成不依赖外部 API 的 mock pack：
  - `python -m pet_pack_builder --input ./base.png --out-dir ./output/mock-pack --pack-id mock-pack --mode mock`
- 从已有 pack 重建预览图：
  - `python -m pet_pack_builder.preview --pack-dir ./output/chief-v1 --output ./output/chief-v1/reconstructed_preview.png`
- 从生成后的 pack 导出参考 C++ 头文件：
  - `python -m pet_pack_builder.export_cpp_header --pack-dir ./output/chief-v1 --output ./output/chief-v1/reference_pack_from_manifest.h`

## 本地配置

- `include/app_config.h` 里的 `BUDDY_API_BASE_URL` 默认是占位用的 Supabase Edge Functions URL。
- 本地 API 覆盖配置应放在未跟踪的 `platformio.local.ini` 中，不要直接改提交内默认值。
- 示例：

```ini
[env:M5StickS3]
build_flags =
    ${env:M5StickS3.build_flags}
    -D BUDDY_API_BASE_URL=\"https://your-project-ref.supabase.co/functions/v1\"
```

## 架构总览

- 这是一个运行在 `M5StickS3` 上的 ESP32-S3 固件仓库，技术栈是 PlatformIO + Arduino + M5Unified。
- 运行时入口在 `src/main.cpp`；主要编排逻辑集中在 `src/app/app.cpp` 里的 `buddy::BuddyApp`。
- `include/app_types.h` 中的 `AppModel` 是全局共享状态容器。大多数模块都围绕它读写，而不是各自维护独立的长期状态树。

### 核心状态边界

这一点也是 `docs/boundaries.md` 的核心约束：

- `Snapshot`：上游/MQTT 下发的工作事实层，包含 `pet_state`、`session_states`、`focus_title`、`requires_user`、token 计数等。
- `PetStats`：设备本地推导出的宠物状态层，主要是 mood / energy / focus，会被 snapshot、IMU 交互、睡眠/唤醒、本地操作影响。
- Presentation：UI 和动画表现层，由 `snapshot + petStats + system state` 在设备本地推导。

修改行为时不要把这三层重新揉成一层。

### 当前运行时流程

`BuddyApp::begin()` 当前主要做这些事：

1. 初始化板级硬件、屏幕和 IMU
2. 初始化基于 Preferences 的设置存储和基于 LittleFS 的资源存储
3. 读取持久化设置和宠物 energy
4. 如果 demo pet pack manifest 不存在，则自动写入内置 demo manifest
5. 启动 Wi-Fi、API client、MQTT client、UI router、pet runtimes
6. 执行在线 bootstrap（Wi-Fi portal / 配对 / bootstrap 接口 / MQTT 配置）

`BuddyApp::tick()` 是主循环，当前顺序大致是：

1. 轮询按键
2. tick Wi-Fi 管理和 captive portal
3. 处理 IMU 摇晃与扣桌面睡眠（face-down sleep）
4. 推进配对 / bootstrap 流程
5. 运行 demo snapshot 或消费 MQTT snapshot
6. 更新 pet runtime、pet stats runtime、snapshot runtime
7. 更新系统状态和屏幕休眠
8. 绘制 UI

当前代码不是 RTOS / 多任务架构，核心行为依赖这个单线程协作式主循环。

### 模块分布

- `src/board/board_init.cpp`：板级初始化、全局帧缓冲 sprite、背光控制。
- `src/board/input.cpp`：把 M5 的 A/B 键短按/长按转换成 `ButtonEvent`。
- `src/ui/router.cpp`：管理三个主页面（`Pet` / `Overview` / `Info`）、引导页，以及菜单/亮度/音量 overlay。
- `src/ui/status_lights.cpp`：绘制 session 状态灯。
- `src/pet/pet_runtime.cpp`：根据应用状态选择动画主状态，并在 demo frame 上叠加程序性位移和变体动作。
- `src/pet/pet_stats.cpp`：本地宠物模拟逻辑；mood / energy / focus 的调整都在这里。
- `src/runtime/snapshot_runtime.cpp`：处理 snapshot 过期检测，以及离线回退。
- `src/net/wifi_manager.cpp`：SoftAP 配网流程和 `192.168.4.1` 本地网页。
- `src/net/api_client.cpp`：`device-pair-code` 和 `device-bootstrap` 两个 HTTP 调用。
- `src/net/mqtt_client.cpp`：broker URL 解析、TLS MQTT 连接、重连/订阅、JSON snapshot 解析。
- `src/storage/settings_store.cpp`：用 Preferences/NVS 保存 Wi-Fi 凭据、device token、亮度、音量、默认页面、demo mode、持久化 pet energy。
- `src/storage/asset_store.cpp`：LittleFS 访问层；当前主要职责是确保 `/pet-pack/demo/manifest.json` 这份内置 demo manifest 存在。

### 重要现实情况

- 文档描述的目标系统比当前代码实现更完整。
- `docs/device-api.md` 里有 `device-pet-sync` 和 `device-heartbeat`，但当前固件代码只实际调用了 `device-pair-code` 和 `device-bootstrap`。
- 资源下载 / 安全切换目前更多还停留在设计文档层面；现有实现仍以内置 demo pack 为主。
- 当前固件唯一消费的实时输入是 MQTT snapshot；设备收到 snapshot 后，再在本地转换成宠物行为和 UI 表达。

## 做较大改动前建议先读的文档

- `docs/boundaries.md` —— snapshot、pet stats、presentation 的语义边界
- `docs/device-client/architecture.md` —— 目标设备端模块与运行时架构
- `docs/device-api.md` —— 设备控制面 API 协议
- `docs/mqtt.md` —— snapshot 传输模型和 topic 约定
- `docs/device-client/README.md` —— 设备文档阅读顺序

如果文档和代码不一致：以当前代码行为为准，把文档视为目标方向。