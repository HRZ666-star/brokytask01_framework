# Task 02 · 自稳云台（阶段成果）

本目录记录任务 02 的阶段结果，**不是完整自稳云台**。

## 结论

- 已完成：机械装配、双 SG90、PA6/PA7 两路 50 Hz PWM、中位 1500 µs、±30° 开环扫角、两轴联动。
- 未完成：MPU 闭环、pitch 自稳、蓝牙指向、上位机三轴曲线、串级 PID、安全策略、指标测试。
- 原因：MPU 安装面积不足、塑料舵盘刚度/回差、杜邦线牵拉、舵机电源/地线干扰；现有装配条件不足以支撑可用闭环。
- 详细说明见 [STATUS.md](STATUS.md)。

## 代码与构建

```powershell
cd C:\Work\brokytask01_framework
git checkout task02
cmake --build build-servo-both -j
```

烧录 `build-servo-both/firmware.elf`，两轴先回中，再分别 ±30° 扫角。

## 证据

视频不放进 Git，保存在工作区 `videos/`：

```text
task02_g1_pa6.mp4
task02_g1_pa7.mp4
task02_gimbal_sweep_a.mp4
task02_gimbal_sweep_b.mp4
```

这些视频只证明机械、双舵机和开环运动，不证明自稳。

## 与 Task 01 的关系

Task 01 的 `main` / `v1.0` 保持独立，不混入本阶段代码。任务 02 的阶段成果保留在 `task02` 分支。
