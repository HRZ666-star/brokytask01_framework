# brokytask01_framework

基于 STM32 的嵌入式框架，BSP / Module / Application 三层结构，用于任务 01（Frameworks）。

## 硬件

| 用途 | 型号 |
|---|---|
| 主开发板 | STM32F103C8T6（Cortex-M3，64 KB Flash / 20 KB RAM） |
| 移植验证板 | STM32F407VET6（Cortex-M4F，512 KB Flash / 192 KB RAM） |
| 调试器 | J-Link + SEGGER Ozone |

## 工具链

- ARM GNU Toolchain 14.3（`arm-none-eabi-gcc/g++`）
- CMake + Ninja
- C++17（`-fno-exceptions -fno-rtti`）
- STM32CubeMX + STM32Cube_FW_F1 / FW_F4

## 计划目录

```text
app/          业务层
modules/      设备与算法模块
bsp/          外设接口 + STM32 适配
board/        f103（主）/ f407（移植）
docs/         架构、接口、移植、测试报告
tools/        构建、烧录、测试脚本
```

## 状态

仓库初始化完成，框架代码待提交。

## 文档

- 架构说明：待补
- 接口说明：待补
- 移植步骤：待补
- 测试报告：待补
