// FViewportCameraSettings.h
// 视口相机漫游的手感参数。集中在此定义，输入逻辑中不出现魔法数字；
// 子类可经 D3DApp::CameraController.GetSettings() 按场景尺度调整。

#pragma once

struct FViewportCameraSettings
{
    // 环视 / 环绕：每像素旋转弧度。
    float LookRadiansPerPixel = 0.003f;

    // 拖拽位移（LMB 前后、平移、Alt 推拉）：每像素移动距离 = 当前移动速度 × 该系数。
    // 与速度挂钩，使大场景调高速度后拖拽手感同步放大（与 UE 一致）。
    float DragDistancePerPixel = 0.01f;

    // 滚轮推拉：每格（WHEEL_DELTA）移动距离 = 当前移动速度 × 该系数。
    float WheelDollyPerNotch = 0.25f;

    // RMB 按住时滚轮调速：每格速度倍率乘 / 除该值，并钳制在 [Min, Max]。
    float SpeedScaleStep = 1.25f;
    float MinSpeedScale = 0.125f;
    float MaxSpeedScale = 8.0f;

    // Alt 环绕：开始环绕时枢轴取视线正前方该距离处；推拉不会越过最小距离。
    float OrbitDistance = 100.0f;
    float MinOrbitDistance = 1.0f;
};
