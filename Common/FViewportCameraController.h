// FViewportCameraController.h
// UE5 风格的视口相机漫游：把 Win32 键鼠输入翻译为 FCamera 的位移 / 旋转。
// 由 D3DApp 持有并在消息路由与每帧 Run 循环中驱动，所有 D3DApp 子类自动获得同一套漫游。
//
// 操作映射（对照 UE5 透视视口默认行为）：
//   RMB 拖拽                 : 环视（Yaw / Pitch）；按住期间 WASD 飞行、Q / E 世界下 / 上、滚轮调速
//   LMB 拖拽                 : 左右转向 + 沿水平面前后移动
//   MMB 拖拽 或 LMB+RMB 拖拽 : 平移（左右 + 世界上下）
//   滚轮                     : 沿视线前后推拉
//   Alt+LMB / Alt+RMB / Alt+MMB : 绕枢轴环绕 / 朝枢轴推拉 / 平移相机与枢轴
//   方向键                   : 无需按鼠标，前后移动与左右平移
// 拖拽期间隐藏光标并锁定在按下位置（可无限拖拽），松开后在原处恢复显示。

#pragma once

#include "MathTypes.h"
#include "FViewportCameraSettings.h"

#include <windows.h>

#include <array>
#include <cstdint>

class FCamera;

class FViewportCameraController
{
public:
    explicit FViewportCameraController(FCamera& InCamera);

    // 光标锁定需要窗口做客户区 -> 屏幕坐标换算；窗口创建后由 D3DApp 设置。
    void SetWindow(HWND InWindow) { Window = InWindow; }

    // 关闭后不再响应任何输入（自定义相机逻辑的子类可关闭）。
    void SetEnabled(bool bInEnabled);
    bool IsEnabled() const { return bEnabled; }

    FViewportCameraSettings& GetSettings() { return Settings; }
    bool IsDragging() const { return DragMode != EDragMode::None; }

    // 每帧调用：按当前按键状态推进键盘移动。
    void Tick(float DeltaSeconds);

    // ---- Win32 消息入口：坐标为客户区坐标，ButtonState 为消息附带的 MK_* 标志 ----
    void OnMouseButtonChanged(WPARAM ButtonState, int X, int Y);
    void OnMouseMove(int X, int Y);
    void OnMouseWheel(int WheelDelta);
    void OnKeyDown(WPARAM Key);
    void OnKeyUp(WPARAM Key);

    // 丢失鼠标捕获：只结束拖拽（恢复光标），保留键盘状态。
    void CancelDrag();
    // 窗口失焦：清空按键并结束拖拽，避免"粘键"与光标保持隐藏。
    void ResetInput();

private:
    enum class EDragMode : uint8_t
    {
        None,
        Look,           // RMB
        TurnAndMove,    // LMB
        Pan,            // MMB 或 LMB+RMB
        Orbit,          // Alt+LMB
        OrbitDolly,     // Alt+RMB
        OrbitPan,       // Alt+MMB
    };

    static EDragMode ResolveDragMode(WPARAM ButtonState, bool bAltDown);
    static bool IsOrbitMode(EDragMode Mode);

    void BeginDrag(int X, int Y);
    void ApplyDrag(int DeltaX, int DeltaY);
    void RecenterCursor() const;
    void SetCursorHidden(bool bHidden);

    void ApplyOrbit(float DeltaYaw, float DeltaPitch);
    void ApplyOrbitDolly(float Distance);
    void ApplyPan(float RightDistance, float UpDistance, bool bMovePivot);

    bool IsKeyDown(WPARAM Key) const;
    float GetCurrentMoveSpeed() const;   // 相机基础速度 × 滚轮调出的速度倍率

    FCamera& Camera;
    FViewportCameraSettings Settings;
    HWND Window = nullptr;
    bool bEnabled = true;

    EDragMode DragMode = EDragMode::None;
    POINT DragAnchor = {};               // 拖拽起点（客户区坐标），光标每次移动后被拉回此处
    bool bCursorHidden = false;

    FVector3 OrbitPivot = {};
    float SpeedScale = 1.0f;

    static constexpr size_t KeyCount = 256;   // Win32 虚拟键码范围
    std::array<bool, KeyCount> KeyStates = {};
};
