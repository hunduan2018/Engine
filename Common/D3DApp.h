// D3DApp.h
// D3D12 应用宿主框架：窗口、设备、交换链、每帧命令对象与消息路由。
// 渲染逻辑由子类实现（Update/Draw/输入虚函数），当前唯一子类见 src/Main.cpp。
// 内置 UE5 风格视口相机漫游（Camera + CameraController），子类直接使用 Camera 取视图矩阵。

#pragma once

#include "d3dUtil.h"
#include "GameTimer.h"
#include "D3D12QueueManger.h"
#include "DescriptorHeapManagement.h"
#include "FCamera.h"
#include "FViewportCameraController.h"
#include "FFrameResource.h"
#include "FrameConfig.h"

#include <Windowsx.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXColors.h>

#include <string>
#include <memory>

class D3DApp
{
public:
    static const int SwapChainBufferCount = 3;

    D3DApp(HINSTANCE hInstance, const std::wstring& title, int clientWidth, int clientHeight);
    D3DApp(const D3DApp&) = delete;
    D3DApp& operator=(const D3DApp&) = delete;
    virtual ~D3DApp();

public:
	// Accessors
    static D3DApp* GetApp();
    // Initialization
    virtual bool Initialize();
    int Run();

    HWND MainWindow() const { return mMainWnd; }
    HINSTANCE AppInstance() const { return mAppInstance; }
    float AspectRatio() const;
    bool Is4xMsaaEnabled() const { return m4xMsaaState; }
    void Set4xMsaaState(bool enabled);
    float FramesPerSecond() const { return mFramesPerSecond; }
    float MillisecondsPerFrame() const { return mMillisecondsPerFrame; }

    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void OnResize();
    virtual void Update(const GameTimer& timer) = 0;
    // 调用时框架已完成 BeginFrame：本帧槽的 GPU 工作已结束、命令列表处于录制状态、
    // 后备缓冲已转为 RENDER_TARGET。子类只录制本帧命令，提交与 Present 由 EndFrame 完成。
    virtual void Draw(const GameTimer& timer) = 0;

    virtual void OnKeyDown(WPARAM key) {}
    virtual void OnKeyUp(WPARAM key) {}
    virtual void OnMouseDown(WPARAM buttonState, int x, int y) {}
    virtual void OnMouseUp(WPARAM buttonState, int x, int y) {}
    virtual void OnMouseMove(WPARAM buttonState, int x, int y) {}
    virtual void OnMouseWheel(int wheelDelta) {}
    virtual void OnPause() {}
    virtual void OnResume() {}
    virtual void CalculateFrameStats();

	ID3D12Resource* CurrentBackBuffer() const { return mSwapChainBuffer[mCurrentBackBuffer].Get(); }
	ID3D12Resource* DepthStencilBuffer() const { return mDepthStencilBuffer.Get(); }
    const FFrameResource& GetCurrentFrame() const { return FrameResources[CurrentFrameIndex]; }

    void FlushCommandQueue();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateRtvAndDsvDescriptorHeaps();

    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

    std::unique_ptr<Direct3DQueueManager> mQueueManager;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    // 在飞帧资源：帧槽按 CurrentFrameIndex 轮转，与交换链后备缓冲索引解耦。
    FFrameResource FrameResources[NumFramesInFlight];
    uint32_t CurrentFrameIndex = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain;
    std::unique_ptr<DescriptorHeap> mRtvHeap;
    std::unique_ptr<DescriptorHeap> mDsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    D3D12_VIEWPORT mScreenViewport = {};
    D3D12_RECT mScissorRect = {};
    UINT mRtvDescriptorSize = 0;
    UINT mDsvDescriptorSize = 0;
    UINT mCbvSrvUavDescriptorSize = 0;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT mCurrentBackBuffer = 0;

    bool mAppPaused = false;
    bool mMinimized = false;
    bool mMaximized = false;
    bool mResizing = false;
    bool m4xMsaaState = false;
    UINT m4xMsaaQuality = 0;
    GameTimer mTimer;
    double mFrameStatsTime = 0.0;
    UINT mFrameStatsFrameCount = 0;
    float mFramesPerSecond = 0.0f;
    float mMillisecondsPerFrame = 0.0f;

    // 通用视口相机：输入先交给控制器再转发子类虚函数；不需要漫游的子类可
    // CameraController.SetEnabled(false)。声明顺序保证 Camera 先于控制器构造。
    FCamera Camera;
    FViewportCameraController CameraController{ Camera };

protected:
    bool InitMainWindow();
    bool InitDirect3D();

    void CalculateWindowSize(UINT clientWidth, UINT clientHeight, RECT& windowRect) const;

private:
    // 帧生命周期由框架独占，子类无法打乱"先等 fence、再复用"的顺序。
    void BeginFrame();
    void EndFrame();

protected:
	static D3DApp* mApp;

    HINSTANCE mAppInstance = nullptr;
    HWND mMainWnd = nullptr;
    std::wstring mTitle;
    std::wstring mWindowClassName;
    int mClientWidth = 0;
    int mClientHeight = 0;
};
