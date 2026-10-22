// FViewportCameraController.cpp
// 操作映射与设计说明见头文件。

#include "../src/stdafx.h"
#include "FViewportCameraController.h"
#include "FCamera.h"
#include "MathHelper.h"

#include <cmath>

using namespace DirectX;

namespace
{
    // FCamera 约定 Y 轴向上（右手系）。
    FSimdVector GetWorldUp()
    {
        return XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }

    bool IsAltDown()
    {
        return (GetKeyState(VK_MENU) & 0x8000) != 0;
    }

    // 视线在水平面上的投影（LMB 拖拽"沿地面前后走"）；俯仰已被钳制，投影不会退化为零向量。
    FSimdVector GetHorizontalForward(const FCamera& Camera)
    {
        const FVector3 Forward = Camera.GetForwardVector();
        return XMVector3Normalize(XMVectorSet(Forward.x, 0.0f, Forward.z, 0.0f));
    }

    void TranslateCamera(FCamera& Camera, FSimdVector Offset)
    {
        FVector3 WorldOffset;
        XMStoreFloat3(&WorldOffset, Offset);
        Camera.Translate(WorldOffset);
    }
}

FViewportCameraController::FViewportCameraController(FCamera& InCamera)
    : Camera(InCamera)
{
}

void FViewportCameraController::SetEnabled(bool bInEnabled)
{
    if (!bInEnabled)
    {
        ResetInput();
    }
    bEnabled = bInEnabled;
}

void FViewportCameraController::Tick(float DeltaSeconds)
{
    if (!bEnabled)
    {
        return;
    }

    auto AxisInput = [this](WPARAM PositiveKey, WPARAM NegativeKey)
    {
        return (IsKeyDown(PositiveKey) ? 1.0f : 0.0f) - (IsKeyDown(NegativeKey) ? 1.0f : 0.0f);
    };

    // WASD / QE 仅在 RMB 环视时生效（UE5 默认，也避免与应用自身的单键快捷键冲突）；方向键始终可用。
    float ForwardInput = AxisInput(VK_UP, VK_DOWN);
    float RightInput = AxisInput(VK_RIGHT, VK_LEFT);
    float UpInput = 0.0f;
    if (DragMode == EDragMode::Look)
    {
        ForwardInput += AxisInput('W', 'S');
        RightInput += AxisInput('D', 'A');
        UpInput += AxisInput('E', 'Q');
    }

    const FVector3 Forward = Camera.GetForwardVector();
    const FVector3 Right = Camera.GetRightVector();
    const FSimdVector Direction =
        XMLoadFloat3(&Forward) * ForwardInput +
        XMLoadFloat3(&Right) * RightInput +
        GetWorldUp() * UpInput;

    const bool bHasMoveInput = XMVectorGetX(XMVector3LengthSq(Direction)) > 0.0f;
    if (!bHasMoveInput)
    {
        return;
    }

    // 多键组合时归一化方向，斜向移动不会更快。
    TranslateCamera(Camera, XMVector3Normalize(Direction) * (GetCurrentMoveSpeed() * DeltaSeconds));
}

void FViewportCameraController::OnMouseButtonChanged(WPARAM ButtonState, int X, int Y)
{
    if (!bEnabled)
    {
        return;
    }

    const EDragMode NewMode = ResolveDragMode(ButtonState, IsAltDown());
    if (NewMode == DragMode)
    {
        return;
    }

    const bool bWasDragging = IsDragging();
    const bool bEntersOrbit = IsOrbitMode(NewMode) && !IsOrbitMode(DragMode);
    DragMode = NewMode;

    if (!IsDragging())
    {
        CancelDrag();
        return;
    }

    // 拖拽中途增减按键（如 RMB 再按 LMB）只切换模式，锚点保持不变。
    if (!bWasDragging)
    {
        BeginDrag(X, Y);
    }

    // 无选中物体可聚焦，枢轴取视线正前方固定距离处。
    if (bEntersOrbit)
    {
        const FVector3 Position = Camera.GetPosition();
        const FVector3 Forward = Camera.GetForwardVector();
        XMStoreFloat3(&OrbitPivot,
            XMLoadFloat3(&Position) + XMLoadFloat3(&Forward) * Settings.OrbitDistance);
    }
}

void FViewportCameraController::OnMouseMove(int X, int Y)
{
    if (!bEnabled || !IsDragging())
    {
        return;
    }

    // 光标被拉回锚点时也会产生一次 WM_MOUSEMOVE，其增量为 0，直接忽略。
    const int DeltaX = X - DragAnchor.x;
    const int DeltaY = Y - DragAnchor.y;
    if (DeltaX == 0 && DeltaY == 0)
    {
        return;
    }

    ApplyDrag(DeltaX, DeltaY);
    RecenterCursor();
}

void FViewportCameraController::OnMouseWheel(int WheelDelta)
{
    if (!bEnabled)
    {
        return;
    }

    const float Notches = static_cast<float>(WheelDelta) / WHEEL_DELTA;

    // UE5：按住 RMB 时滚轮调节飞行速度，而不是推拉。
    if (DragMode == EDragMode::Look)
    {
        SpeedScale = MathHelper::Clamp(SpeedScale * std::pow(Settings.SpeedScaleStep, Notches),
            Settings.MinSpeedScale, Settings.MaxSpeedScale);
        return;
    }

    const FVector3 Forward = Camera.GetForwardVector();
    const float Distance = Notches * GetCurrentMoveSpeed() * Settings.WheelDollyPerNotch;
    TranslateCamera(Camera, XMLoadFloat3(&Forward) * Distance);
}

void FViewportCameraController::OnKeyDown(WPARAM Key)
{
    if (bEnabled && Key < KeyCount)
    {
        KeyStates[Key] = true;
    }
}

void FViewportCameraController::OnKeyUp(WPARAM Key)
{
    if (Key < KeyCount)
    {
        KeyStates[Key] = false;
    }
}

void FViewportCameraController::CancelDrag()
{
    DragMode = EDragMode::None;
    SetCursorHidden(false);
}

void FViewportCameraController::ResetInput()
{
    KeyStates.fill(false);
    CancelDrag();
}

FViewportCameraController::EDragMode FViewportCameraController::ResolveDragMode(WPARAM ButtonState, bool bAltDown)
{
    const bool bLeft = (ButtonState & MK_LBUTTON) != 0;
    const bool bRight = (ButtonState & MK_RBUTTON) != 0;
    const bool bMiddle = (ButtonState & MK_MBUTTON) != 0;

    if (bAltDown)
    {
        if (bMiddle) return EDragMode::OrbitPan;
        if (bRight) return EDragMode::OrbitDolly;
        if (bLeft) return EDragMode::Orbit;
        return EDragMode::None;
    }

    if (bMiddle || (bLeft && bRight)) return EDragMode::Pan;
    if (bRight) return EDragMode::Look;
    if (bLeft) return EDragMode::TurnAndMove;
    return EDragMode::None;
}

bool FViewportCameraController::IsOrbitMode(EDragMode Mode)
{
    return Mode == EDragMode::Orbit || Mode == EDragMode::OrbitDolly || Mode == EDragMode::OrbitPan;
}

void FViewportCameraController::BeginDrag(int X, int Y)
{
    DragAnchor = { X, Y };
    SetCursorHidden(true);
}

void FViewportCameraController::ApplyDrag(int DeltaX, int DeltaY)
{
    // 屏幕坐标 Y 向下：鼠标上移（DeltaY < 0）对应抬头 / 前进 / 上升。
    const float DeltaYaw = -DeltaX * Settings.LookRadiansPerPixel;
    const float DeltaPitch = -DeltaY * Settings.LookRadiansPerPixel;
    const float DragDistance = GetCurrentMoveSpeed() * Settings.DragDistancePerPixel;

    switch (DragMode)
    {
    case EDragMode::Look:
        Camera.AddRotation(DeltaYaw, DeltaPitch);
        break;
    case EDragMode::TurnAndMove:
        Camera.AddRotation(DeltaYaw, 0.0f);
        TranslateCamera(Camera, GetHorizontalForward(Camera) * (-DeltaY * DragDistance));
        break;
    case EDragMode::Pan:
        ApplyPan(DeltaX * DragDistance, -DeltaY * DragDistance, false);
        break;
    case EDragMode::Orbit:
        ApplyOrbit(DeltaYaw, DeltaPitch);
        break;
    case EDragMode::OrbitDolly:
        // 向右或向上拖 = 靠近枢轴。
        ApplyOrbitDolly((DeltaX - DeltaY) * DragDistance);
        break;
    case EDragMode::OrbitPan:
        // "抓取场景"：场景跟随光标，相机与枢轴一起反向移动。
        ApplyPan(-DeltaX * DragDistance, DeltaY * DragDistance, true);
        break;
    default:
        break;
    }
}

void FViewportCameraController::RecenterCursor() const
{
    if (!Window)
    {
        return;
    }

    POINT ScreenPoint = DragAnchor;
    ClientToScreen(Window, &ScreenPoint);
    SetCursorPos(ScreenPoint.x, ScreenPoint.y);
}

void FViewportCameraController::SetCursorHidden(bool bHidden)
{
    // ShowCursor 是引用计数，必须成对调用，否则光标会永久隐藏 / 无法隐藏。
    if (bCursorHidden == bHidden)
    {
        return;
    }
    ShowCursor(bHidden ? FALSE : TRUE);
    bCursorHidden = bHidden;
}

void FViewportCameraController::ApplyOrbit(float DeltaYaw, float DeltaPitch)
{
    // 保持与枢轴的距离：先旋转朝向，再把相机放回"枢轴 - 视线 × 距离"处。
    const FVector3 Position = Camera.GetPosition();
    const FSimdVector Pivot = XMLoadFloat3(&OrbitPivot);
    const float Distance = XMVectorGetX(XMVector3Length(Pivot - XMLoadFloat3(&Position)));

    Camera.AddRotation(DeltaYaw, DeltaPitch);

    const FVector3 Forward = Camera.GetForwardVector();
    FVector3 NewPosition;
    XMStoreFloat3(&NewPosition, Pivot - XMLoadFloat3(&Forward) * Distance);
    Camera.SetPosition(NewPosition);
}

void FViewportCameraController::ApplyOrbitDolly(float Distance)
{
    const FVector3 Position = Camera.GetPosition();
    const FVector3 Forward = Camera.GetForwardVector();
    const FSimdVector Pivot = XMLoadFloat3(&OrbitPivot);
    const float CurrentDistance = XMVectorGetX(XMVector3Length(Pivot - XMLoadFloat3(&Position)));
    const float NewDistance = MathHelper::Max(CurrentDistance - Distance, Settings.MinOrbitDistance);

    FVector3 NewPosition;
    XMStoreFloat3(&NewPosition, Pivot - XMLoadFloat3(&Forward) * NewDistance);
    Camera.SetPosition(NewPosition);
}

void FViewportCameraController::ApplyPan(float RightDistance, float UpDistance, bool bMovePivot)
{
    const FVector3 Right = Camera.GetRightVector();
    const FSimdVector Offset = XMLoadFloat3(&Right) * RightDistance + GetWorldUp() * UpDistance;
    TranslateCamera(Camera, Offset);

    if (bMovePivot)
    {
        XMStoreFloat3(&OrbitPivot, XMLoadFloat3(&OrbitPivot) + Offset);
    }
}

bool FViewportCameraController::IsKeyDown(WPARAM Key) const
{
    return Key < KeyCount && KeyStates[Key];
}

float FViewportCameraController::GetCurrentMoveSpeed() const
{
    return Camera.GetMoveSpeed() * SpeedScale;
}
