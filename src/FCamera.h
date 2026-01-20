//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include <windows.h>
#include "../Common/MathTypes.h"

using namespace DirectX;

class FCamera
{
public:
    FCamera();

    void Init(FVector3 position);

    void Update(float elapsedSeconds);
    FSimdMatrix GetViewMatrix();
    FSimdMatrix GetProjectionMatrix(float fov, float aspectRatio, float nearPlane = 1.0f, float farPlane = 1000.0f);

    void SetMoveSpeed(float unitsPerSecond);
    void SetTurnSpeed(float radiansPerSecond);

    // Mouse look
    void SetMouseSensitivity(float radiansPerPixel);
    void OnRButtonDown(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseMove(int x, int y);

    // Keyboard
    void OnKeyDown(UINT8 key);
    void OnKeyUp(UINT8 key);

private:
    void Reset();

    struct KeysPressed
    {
        bool w;
        bool a;
        bool s;
        bool d;

        bool left;
        bool right;
        bool up;
        bool down;
    };

    FVector3 m_initialPosition;
    FVector3 m_position;
    float m_yaw;                // Relative to the +z axis.
    float m_pitch;              // Relative to the xz plane.
    FVector3 m_lookDirection;
    FVector3 m_upDirection;

    float m_moveSpeed;          // Speed at which the camera moves, in units per second.
    float m_turnSpeed;          // Speed at which the camera turns, in radians per second.

    float m_mouseSensitivity;   // Radians per pixel.
    bool m_isRButtonDown;
    int m_lastMouseX;
    int m_lastMouseY;

    KeysPressed m_keysPressed;
};
