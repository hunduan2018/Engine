//*********************************************************
// Basic Direct3D 12 application framework.
// 窗口/设备/交换链/命令对象/消息路由的宿主框架；渲染逻辑由子类实现。
//*********************************************************

#include "../src/stdafx.h"
#include "D3DApp.h"
#include "d3dx12.h"

#include <algorithm>

using Microsoft::WRL::ComPtr;

D3DApp* D3DApp::mApp = nullptr;

D3DApp::D3DApp(HINSTANCE hInstance, const std::wstring& title, int clientWidth, int clientHeight)
    : mAppInstance(hInstance), mTitle(title),
      mWindowClassName(title + L"WindowClass"),
      mClientWidth(clientWidth), mClientHeight(clientHeight)
{
    mApp = this;
}

D3DApp::~D3DApp()
{
    // GPU 可能仍在执行已提交的命令并引用本类持有的资源，销毁前必须排空队列。
    if (mDevice && mQueueManager)
    {
        FlushCommandQueue();
    }

    if (mApp == this)
    {
        mApp = nullptr;
    }
}

D3DApp* D3DApp::GetApp()
{
    return mApp;
}

bool D3DApp::Initialize()
{
    // 窗口 -> 设备/交换链 -> 按客户区尺寸重建渲染目标。任一步失败即中止。
    if (!InitMainWindow() || !InitDirect3D())
    {
        return false;
    }

    OnResize();
    return true;
}

int D3DApp::Run()
{
    // 标准消息泵：有消息先处理；无消息且未暂停时才推进帧（暂停时计时器冻结，
    // 恢复后 DeltaTime 不会包含暂停时长）。
    mTimer.Reset();
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else if (!mAppPaused)
        {
            mTimer.Tick();
            CalculateFrameStats();
            Update(mTimer);
            Draw(mTimer);
        }
    }
    return static_cast<int>(msg.wParam);
}

float D3DApp::AspectRatio() const
{
    return mClientHeight > 0 ? static_cast<float>(mClientWidth) / mClientHeight : 0.0f;
}

void D3DApp::CalculateFrameStats()
{
    // 每满 1 秒刷新一次窗口标题的 FPS/帧时间（避免高频 SetWindowText 抖动）。
    const double deltaTime = static_cast<double>(mTimer.DeltaTime());
    mFrameStatsTime += deltaTime;
    ++mFrameStatsFrameCount;

    if (mFrameStatsTime >= 1.0)
    {
        mFramesPerSecond = static_cast<float>(mFrameStatsFrameCount / mFrameStatsTime);
        mMillisecondsPerFrame = mFramesPerSecond > 0.0f
            ? 1000.0f / mFramesPerSecond
            : 0.0f;

        wchar_t title[256] = {};
        swprintf_s(title, L"%s    FPS: %.1f    Frame Time: %.2f ms",
            mTitle.c_str(), mFramesPerSecond, mMillisecondsPerFrame);
        SetWindowTextW(mMainWnd, title);

        mFrameStatsTime = 0.0;
        mFrameStatsFrameCount = 0;
    }
}

void D3DApp::Set4xMsaaState(bool enabled)
{
    if (m4xMsaaState != enabled)
    {
        m4xMsaaState = enabled;
        if (mDevice)
        {
            OnResize();
        }
    }
}

bool D3DApp::InitMainWindow()
{
    // 注册窗口类：主窗口过程是静态入口，经 mApp 单例转发到虚函数 MsgProc。
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = mAppInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = mWindowClassName.c_str();

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    // 客户区尺寸换算为窗口外框尺寸（标题栏/边框由样式决定）。
    RECT rect = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    mMainWnd = CreateWindowExW(0, mWindowClassName.c_str(), mTitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, mAppInstance, this);
    if (!mMainWnd)
    {
        return false;
    }

    ShowWindow(mMainWnd, SW_SHOW);
    UpdateWindow(mMainWnd);
    return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(_DEBUG)
    // 调试层可经 MENGINE_DISABLE_DEBUG_LAYER=1 临时关闭（驱动与调试层组合问题的排查开关）。
    const bool bDisableDebugLayer = GetEnvironmentVariableW(L"MENGINE_DISABLE_DEBUG_LAYER", nullptr, 0) != 0;
    if (!bDisableDebugLayer)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
    }
#endif

    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory))))
    {
        return false;
    }

    // 适配器选择：按高性能偏好枚举，跳过软件适配器，用"试建设备"探测可用性。
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0;
         factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++index)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapter.Reset();
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&mDevice))))
        {
            break;
        }
        adapter.Reset();
    }

    // 无可用硬件适配器时退回 WARP 软件渲染。
    if (!mDevice)
    {
        ComPtr<IDXGIAdapter> warp;
        D3D12_THROW_IF_FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        D3D12_THROW_IF_FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&mDevice)));
    }

    // 各堆类型的描述符步长（依硬件而异，句柄运算前必须缓存）。
    mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 4xMSAA 质量等级查询（渲染器可选用的上限）。
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = {};
    levels.Format = mBackBufferFormat;
    levels.SampleCount = 4;
    D3D12_THROW_IF_FAILED(mDevice->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels)));
    m4xMsaaQuality = levels.NumQualityLevels;

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();
    return true;
}

void D3DApp::CreateCommandObjects()
{
    // 每个后备缓冲一个分配器（与帧槽一一对应：录制中/执行中的帧互不干扰）+ 一条复用的命令列表。
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        D3D12_THROW_IF_FAILED(mDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc[i])));
    }
    D3D12_THROW_IF_FAILED(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        mDirectCmdListAlloc[0].Get(), nullptr, IID_PPV_ARGS(&mCommandList)));
    // 创建即 Close：D3D12 约定新建列表处于录制态，Reset 前必须先关闭。
    D3D12_THROW_IF_FAILED(mCommandList->Close());
    mQueueManager = std::make_unique<Direct3DQueueManager>(mDevice.Get());
}

void D3DApp::CreateSwapChain()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> factory;
    D3D12_THROW_IF_FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)));

    // FLIP_DISCARD 3 缓冲；未启用 ALLOW_TEARING，窗口模式下呈现受 DWM 合成节流
    //（帧时间为 60Hz 周期整数倍，非渲染性能指标）。
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = mClientWidth;
    desc.Height = mClientHeight;
    desc.Format = mBackBufferFormat;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = SwapChainBufferCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    //desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    D3D12_THROW_IF_FAILED(factory->CreateSwapChainForHwnd(
        mQueueManager->GetGraphicsQueue()->Get(), mMainWnd, &desc, nullptr, nullptr, &swapChain));
    D3D12_THROW_IF_FAILED(swapChain.As(&mSwapChain));
    // 交换链建在图形队列上（Present 与绘制同队列保证顺序）；禁用 Alt+Enter 自动全屏切换。
    D3D12_THROW_IF_FAILED(factory->MakeWindowAssociation(mMainWnd, DXGI_MWA_NO_ALT_ENTER));
    mCurrentBackBuffer = mSwapChain->GetCurrentBackBufferIndex();
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    // CPU 侧非着色器可见堆即可（渲染目标/深度模板视图不参与着色器绑定）。
    mRtvHeap = std::make_unique<DescriptorHeap>(
        mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SwapChainBufferCount, false);
    mRtvHeap->SetName(L"RTVDescriptorHeap");
    mDsvHeap = std::make_unique<DescriptorHeap>(
        mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    mDsvHeap->SetName(L"DSVDescriptorHeap");
}

void D3DApp::OnResize()
{
    if (!mDevice || !mSwapChain)
    {
        return;
    }
    mClientWidth = mClientWidth > 0 ? mClientWidth : 1;
    mClientHeight = mClientHeight > 0 ? mClientHeight : 1;

    // GPU 可能仍引用旧的后备缓冲/深度缓冲：先排空队列再释放重建。
    FlushCommandQueue();
    D3D12_THROW_IF_FAILED(mCommandList->Reset(
        mDirectCmdListAlloc[mCurrentBackBuffer].Get(), nullptr));
    for (auto& buffer : mSwapChainBuffer)
    {
        buffer.Reset();
    }
    mDepthStencilBuffer.Reset();

    // 所有缓冲释放后才能 ResizeBuffers（这是交换链重建的隐含前提）。
    DXGI_SWAP_CHAIN_DESC oldDesc = {};
    D3D12_THROW_IF_FAILED(mSwapChain->GetDesc(&oldDesc));
    D3D12_THROW_IF_FAILED(mSwapChain->ResizeBuffers(SwapChainBufferCount,
        mClientWidth, mClientHeight, mBackBufferFormat, oldDesc.Flags));
    mCurrentBackBuffer = mSwapChain->GetCurrentBackBufferIndex();

    // 重建 RTV 视图（新缓冲对象，旧描述符全部失效）。
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        D3D12_THROW_IF_FAILED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
        std::wstring bufferName = L"SwapChainBuffer[" + std::to_wstring(i) + L"]";
        D3D12_THROW_IF_FAILED(mSwapChainBuffer[i]->SetName(bufferName.c_str()));
        mDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr,
            mRtvHeap->GetCpuHandle(i));
    }

    // 重建深度缓冲与 DSV（优化清屏值；初态 COMMON，经屏障进入 DEPTH_WRITE）。
    D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        mDepthStencilFormat, mClientWidth, mClientHeight, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = mDepthStencilFormat;
    clearValue.DepthStencil = { 1.0f, 0 };
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_THROW_IF_FAILED(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &depthDesc, D3D12_RESOURCE_STATE_COMMON, &clearValue,
        IID_PPV_ARGS(&mDepthStencilBuffer)));
    D3D12_THROW_IF_FAILED(mDepthStencilBuffer->SetName(L"DepthStencilBuffer"));
    mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr,
        mDsvHeap->GetCpuHandle(0));

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE));
    // 执行屏障并排空：队列实现自行 Close 列表。
    ID3D12CommandList* lists[] = { mCommandList.Get() };
    mQueueManager->GetGraphicsQueue()->ExecuteCommandLists(1, lists);
    FlushCommandQueue();

    mScreenViewport = { 0.0f, 0.0f, static_cast<float>(mClientWidth),
        static_cast<float>(mClientHeight), 0.0f, 1.0f };
    mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

void D3DApp::FlushCommandQueue()
{
    // Signal + CPU 阻塞等待：返回后 GPU 必然空闲，可安全 Reset 资源/分配器。
    Direct3DQueue* graphicsQueue = mQueueManager->GetGraphicsQueue();
    const uint64 fenceValue = graphicsQueue->Signal();
    graphicsQueue->WaitForFenceCPUBlocking(fenceValue);
}

LRESULT CALLBACK D3DApp::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 静态入口：窗口注册需要普通函数指针，转发给单例的虚函数处理。
    return mApp ? mApp->MsgProc(hwnd, msg, wParam, lParam)
                : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // 失焦暂停/复焦恢复：暂停期间 Run() 不推进帧，计时器冻结。
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            mAppPaused = true;
            mTimer.Stop();
            OnPause();
        }
        else
        {
            mAppPaused = false;
            mTimer.Start();
            OnResume();
        }
        return 0;

    // 尺寸变化：最小化只暂停；最大化/还原立即重建。拖动边框期间不重建
    //（见 WM_ENTERSIZEMOVE），松手后由 WM_EXITSIZEMOVE 统一处理，避免高频 ResizeBuffers。
    case WM_SIZE:
        mClientWidth = LOWORD(lParam);
        mClientHeight = HIWORD(lParam);
        if (mDevice)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                mAppPaused = true;
                mMinimized = true;
                mMaximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                mAppPaused = false;
                mMinimized = false;
                mMaximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED && !mResizing)
            {
                OnResize();
            }
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        mAppPaused = true;
        mResizing = true;
        mTimer.Stop();
        return 0;
    case WM_EXITSIZEMOVE:
        mAppPaused = false;
        mResizing = false;
        mTimer.Start();
        OnResize();
        return 0;

    // 键鼠输入转发到子类虚函数（拖拽期间捕获鼠标，保证窗口外释放也能收到 UP）。
    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;
    case WM_KEYUP:
        OnKeyUp(wParam);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SetCapture(hwnd);
        OnMouseDown(wParam,
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        ReleaseCapture();
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
