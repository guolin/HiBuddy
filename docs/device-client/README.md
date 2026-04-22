# Buddy Device Client Docs

这组文档面向 `M5StickS3` 设备客户端的固件、设计、资源和联调工作。

目标不是重复已有的高层产品文档，而是把设备端真正要实现的内容写成可以开工的规格。

适用范围：

- 设备端固件实现
- 设备端 UI 和交互设计
- 宠物资源包和动画制作
- 设备控制面接口联调
- 设备本地缓存和异常回退

不覆盖：

- CLI 聚合逻辑细节
- Web 控制台页面细节
- 服务器数据建模细节

## 文档地图

- 总览与边界：
  [../device.md](../device.md)
- 现有设备 UI 总览：
  [../device-ui.md](../device-ui.md)
- 设备控制面接口：
  [../device-api.md](../device-api.md)

## 设备客户端细化文档

- 架构总览：
  [architecture.md](./architecture.md)
- 交互逻辑：
  [interaction.md](./interaction.md)
- 宠物资源与动画：
  [pet-animation.md](./pet-animation.md)
- 网络与运行时：
  [network-runtime.md](./network-runtime.md)
- 页面与文案规格：
  [screens.md](./screens.md)
- 技术实现规范：
  [technical-implementation.md](./technical-implementation.md)

## 推荐阅读顺序

1. 先读 [architecture.md](./architecture.md)
2. 再读 [interaction.md](./interaction.md)
3. 做资源和动画前读 [pet-animation.md](./pet-animation.md)
4. 做联网和同步前读 [network-runtime.md](./network-runtime.md)
5. 做具体工程落地前读 [technical-implementation.md](./technical-implementation.md)
6. 做页面实现和验收前读 [screens.md](./screens.md)

## V1 核心结论

- V1 设备是一个小屏 AI 宠物显示器，不是审批器，不是聊天终端。
- V1 的主视觉是一个 `64x64` 像素宠物，显示时可放大到 `128x128`。
- V1 的宠物行为由 `snapshot + 本地短期状态` 驱动。
- V1 设备有三种主界面：
  `Image`、`Mixed`、`Info`
- V1 使用 `A/B` 两个键，支持短按和长按。
- V1 宠物资源包通过网络下载到本地 Flash，不依赖 SD 卡。
- V1 默认尽量少文字，除菜单和设置页外，不依赖中文文案。
- V1 使用 session 状态灯表达多 session 状态，不在主界面显示状态码文字。
- V1 的 `waiting` 只表示“需要你处理”，默认不在设备上直接审批。
- 设备直接 `Approve / Deny` 是 V2 预留能力。
