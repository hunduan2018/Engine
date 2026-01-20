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

#include "stdafx.h"
#include "FCamera.h"

// Define the pitch limit margin to prevent singularities
// When pitch approaches ±π/2, cos(pitch) approaches 0, causing numerical instability
constexpr float PITCH_LIMIT_MARGIN = 0.01f;
constexpr float MAX_PITCH = XM_PIDIV2 - PITCH_LIMIT_MARGIN;
constexpr float MIN_PITCH = -XM_PIDIV2 + PITCH_LIMIT_MARGIN;

FCamera::FCamera() :
    m_initialPosition(0, 0, 0),
    m_position(m_initialPosition),
    m_yaw(XM_PI),
    m_pitch(0.0f),
    m_lookDirection(0, 0, -1),
    m_upDirection(0, 1, 0),
    m_moveSpeed(20.0f),
    m_turnSpeed(XM_PIDIV2),
    m_mouseSensitivity(0.003f),
    m_isRButtonDown(false),
    m_lastMouseX(0),
    m_lastMouseY(0),
    m_keysPressed{}
{
}

void FCamera::Init(FVector3 position)
{
    m_initialPosition = position;
    Reset();
}

void FCamera::SetMoveSpeed(float unitsPerSecond)
{
    m_moveSpeed = unitsPerSecond;
}

void FCamera::SetTurnSpeed(float radiansPerSecond)
{
    m_turnSpeed = radiansPerSecond;
}

void FCamera::SetMouseSensitivity(float radiansPerPixel)
{
    m_mouseSensitivity = radiansPerPixel;
}

void FCamera::OnRButtonDown(int x, int y)
{
    m_isRButtonDown = true;
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void FCamera::OnRButtonUp(int x, int y)
{
    (void)x;
    (void)y;
    m_isRButtonDown = false;
}

void FCamera::OnMouseMove(int x, int y)
{
    if (!m_isRButtonDown)
    {
        return;
    }

    const int dx = x - m_lastMouseX;
    const int dy = y - m_lastMouseY;

    // Windows screen coordinates: y increases downward.
    // Match current keyboard scheme: left => yaw+, right => yaw-.
    m_yaw -= dx * m_mouseSensitivity;
    m_pitch -= dy * m_mouseSensitivity;

    // Clamp pitch to prevent singularities at ±π/2
    // Using std::clamp for better readability
    m_pitch = min(m_pitch, MAX_PITCH);
    m_pitch = max(m_pitch, MIN_PITCH);

    m_lastMouseX = x;
    m_lastMouseY = y;
}

void FCamera::Reset()
{
    m_position = m_initialPosition;
    m_yaw = XM_PI;
    m_pitch = 0.0f;
    m_lookDirection = { 0, 0, -1 };
}

void FCamera::Update(float elapsedSeconds)
{
    // Calculate the move vector in camera space.
    FVector3 move(0, 0, 0);

    if (m_keysPressed.a)
        move.x -= 1.0f;
    if (m_keysPressed.d)
        move.x += 1.0f;
    if (m_keysPressed.w)
        move.z -= 1.0f;
    if (m_keysPressed.s)
        move.z += 1.0f;

    if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
    {
        FSimdVector vector = XMVector3Normalize(XMLoadFloat3(&move));
        move.x = XMVectorGetX(vector);
        move.z = XMVectorGetZ(vector);
    }

    const float moveInterval = m_moveSpeed * elapsedSeconds;
    const float rotateInterval = m_turnSpeed * elapsedSeconds;

    if (m_keysPressed.left)
        m_yaw += rotateInterval;
    if (m_keysPressed.right)
        m_yaw -= rotateInterval;
    if (m_keysPressed.up)
        m_pitch += rotateInterval;
    if (m_keysPressed.down)
        m_pitch -= rotateInterval;

    // Clamp pitch to prevent singularities at ±π/2
    // When pitch approaches ±π/2, cos(pitch) approaches 0, causing numerical instability
    m_pitch = min(m_pitch, MAX_PITCH);
    m_pitch = max(m_pitch, MIN_PITCH);

    // Move the camera in model space.
    const float x = move.x * -cosf(m_yaw) - move.z * sinf(m_yaw);
    const float z = move.x * sinf(m_yaw) - move.z * cosf(m_yaw);
    m_position.x += x * moveInterval;
    m_position.z += z * moveInterval;

    // Determine the look direction.
    const float r = cosf(m_pitch);
    m_lookDirection.x = r * sinf(m_yaw);
    m_lookDirection.y = sinf(m_pitch);
    m_lookDirection.z = r * cosf(m_yaw);
}

FSimdMatrix FCamera::GetViewMatrix()
{
    return XMMatrixLookToRH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_lookDirection), XMLoadFloat3(&m_upDirection));
}

FSimdMatrix FCamera::GetProjectionMatrix(float fov, float aspectRatio, float nearPlane, float farPlane)
{
    return XMMatrixPerspectiveFovRH(fov, aspectRatio, nearPlane, farPlane);
}

void FCamera::OnKeyDown(UINT8 key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = true;
        break;
    case 'A':
        m_keysPressed.a = true;
        break;
    case 'S':
        m_keysPressed.s = true;
        break;
    case 'D':
        m_keysPressed.d = true;
        break;
    case VK_LEFT:
        m_keysPressed.left = true;
        break;
    case VK_RIGHT:
        m_keysPressed.right = true;
        break;
    case VK_UP:
        m_keysPressed.up = true;
        break;
    case VK_DOWN:
        m_keysPressed.down = true;
        break;
    case VK_ESCAPE:
        Reset();
        break;
    }
}

void FCamera::OnKeyUp(UINT8 key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = false;
        break;
    case 'A':
        m_keysPressed.a = false;
        break;
    case 'S':
        m_keysPressed.s = false;
        break;
    case 'D':
        m_keysPressed.d = false;
        break;
    case VK_LEFT:
        m_keysPressed.left = false;
        break;
    case VK_RIGHT:
        m_keysPressed.right = false;
        break;
    case VK_UP:
        m_keysPressed.up = false;
        break;
    case VK_DOWN:
        m_keysPressed.down = false;
        break;
    }
}
