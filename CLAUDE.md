# MyEngine

C++ / D3D12（Agility SDK）渲染引擎，CMake 构建，目标平台 Windows x64。

## 工程结构与构建

- 构建：`cmake -S . -B build`（Visual Studio 17 2022 生成器），`cmake --build build --config Debug`；可执行文件输出到 `bin/x64/<Config>/`
- 目录职责：
  - `src/`：应用入口（`Main.cpp` 中的 TestApp）与旧 DXSample 示例代码
  - `Common/`：跨应用共享基础设施——`D3DApp` 宿主框架、`FCamera` 与 UE5 风格视口漫游 `FViewportCameraController`、队列 / 描述符堆管理、数学工具
  - `Renderer/`：Mesh Draw Command 渲染架构，入口 `FRenderer`
  - `RHI/`：RHI 抽象层（实验中）
  - `Shaders/`：HLSL 源码，构建后由 dxc 编译到输出目录
  - `cmake/`：第三方库接入脚本（如 `ThirdPartyAssimp.cmake`）
  - `ThirdParty/`：第三方依赖
  - `Docs/`：设计说明与技术问答整理
- 新应用继承 `D3DApp` 即自带相机漫游：直接使用基类的 `Camera` 取视图矩阵，手感参数见 `FViewportCameraSettings`，不需要时 `CameraController.SetEnabled(false)`
- `CMakeLists.txt` 中着色器编译段的中文注释是 GBK 编码：编辑时保持原编码（按字节替换），不要整文件按 UTF-8 重写

## 第三方依赖

- 原则：有源码的依赖一律从源码编译（便于调试时单步进入），不引入预编译二进制
- Assimp：源码直接入库于 `ThirdParty/assimp`（上游 6.0.2，commit `2e0dfde`），经 `cmake/ThirdPartyAssimp.cmake` 编为静态库；只编需要的 importer（缓存变量 `MENGINE_ASSIMP_IMPORTERS`，默认 `FBX`）。升级时整体替换该目录并同步更新此处版本号
- 例外（仅二进制发布）：D3D12 Agility SDK（`ThirdParty/D3D12Agility`）、WinPixEventRuntime（可选）、dxc.exe（着色器编译工具）
- `d3dx12.h` 辅助头使用 Agility SDK 自带、与 SDK 版本匹配的 `ThirdParty/D3D12Agility/include/d3dx12/`，工程内不另放副本

## 编码规则

以下 4 条规则适用于本项目所有新增与修改的代码：

### 1. 编码规范统一 — 采用 UE（Unreal Engine）编码规范

- 类型命名 PascalCase，并带 UE 类型前缀：`F`（普通类/结构）、`U`（运行时托管对象）、`A`（Actor 类）、`I`（接口）、`S`（独立结构）、`E`（枚举）、`T`（模板）
- 函数、成员、局部变量均用 PascalCase；bool 变量以 `b` 开头（如 `bIsReady`）
- 不使用 `m_`、`s_`、匈牙利等前缀；成员与局部变量靠命名区分
- 一类型一文件，文件名与主类型同名（如 `FCamera.h` / `FCamera.cpp`）
- 头文件用 `#pragma once`；缩进沿用现有 4 空格
- 官方标准参照：https://dev.epicgames.com/documentation/en-us/uefn/uefn-coding-standard

### 2. 编码逻辑清晰

- 函数单一职责、保持短小；命名表达意图，让调用处可读
- 避免深层嵌套，优先早返回（early return）
- 复杂逻辑先写一段意图注释（为什么这么做），再写实现；不写复述代码的"是什么"注释
- 保持分支、循环条件简单直白，复杂布尔表达式提取成命名良好的临时变量或函数

### 3. 问题不扩散

- 修 bug 只改与该问题直接相关的代码，不顺手重构、重排或"美化"无关代码
- 就地解决根因，禁止把同一 workaround 复制到多处来绕过问题
- 提交最小化：一个提交只解决一件事；改动范围与问题范围一致

### 4. 架构易扩展

- 模块间依赖保持单向，通过接口解耦，避免循环依赖
- 新增功能尽量以新增代码完成，而非修改既有代码（开闭原则）
- 硬编码常量、魔法数字、平台路径集中到配置或常量定义处，不散落在逻辑中

## 存量代码

旧代码不做一次性大规模重排；在因功能改动被触碰时，逐步对齐上述 UE 规范（与规则 3 保持一致：不扩散改动）。
