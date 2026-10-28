# 资源系统方案（Asset Layer / Render Resource Layer）

状态：方案已定（2026-10），**暂缓实施**，当前不需要改造。UE 参照基于 `C:\UnrealEngine5`（5.8.3），路径相对 `Engine/Source/Runtime/`。

## 1. 问题

`src/Main.cpp` 的 `InitializeRenderer` 里直接完成了：读磁盘（`d3dUtil::LoadBinary`）、按 `occcity.h` 的偏移解析文件格式、拼 `FStaticMesh`、创建 GPU 资源（`CreateMeshRenderData` / `CreateTextureFromBC1`）、手动传 `device / commandList / bindless / uploadContext`。问题：

- **职责混在应用层**：文件格式、CPU 数据、RHI 资源创建、生命周期全部散落在 TestApp 里，每加一个资源就复制一遍。
- **没有资源身份**：同一文件加载两次会创建两份 GPU 资源；没有"按路径取资源"的入口。
- **生命周期隐式**：资源靠 TestApp 的 `unique_ptr` 成员存活，销毁时机与 GPU 是否还在使用无关。
- **格式与使用耦合**：`occcity.bin` 一个文件里同时有 Texture、VB、IB，现在只能在应用层手工拆。

## 2. UE5 的做法（已核实）

| 层 | UE5 | 位置 | 要点 |
|---|---|---|---|
| 资产对象 | `UStaticMesh`、`UTexture2D` | Engine/Classes/Engine/StaticMesh.h:602、Texture2D.h:25 | 游戏线程上的资源身份与 CPU 侧数据 |
| Render Data | `FStaticMeshRenderData` → `FStaticMeshLODResources` | Engine/Public/StaticMeshResources.h:771、:417 | 资产持有 `TUniquePtr<FStaticMeshRenderData> RenderData`（StaticMesh.h:633）；VB / IB 各是一个 `FRenderResource` |
| 纹理资源 | `UTexture::CreateResource()` → `FTextureResource` | Texture2D.h:140、Texture.h:1702 `GetResource()` | 资产负责创建自己的渲染资源 |
| 创建 RHI 资源 | `UStaticMesh::InitResources` → `FStaticMeshRenderData::InitResources` → `FStaticMeshLODResources::InitResources` | Engine/Private/StaticMesh.cpp:4868 → :2795 → :1520 | 最终对每个缓冲 `BeginInitResource`（RenderCore/Public/RenderResource.h:218），在渲染线程 `InitRHI` 中真正创建 |
| 安全销毁 | `BeginDestroy` → `ReleaseResources` + `ReleaseResourcesFence.BeginFence()`；`IsReadyForFinishDestroy` 等 fence 完成 | StaticMesh.cpp:6137、:5306、:6189 | CPU 对象要等 GPU / 渲染线程不再使用才真正释放 |
| 加载与管理 | `UAssetManager`、`FStreamableManager`、`LoadObject` | Engine/Classes/Engine/AssetManager.h:83、StreamableManager.h:731 | 按路径（`FSoftObjectPath`）同步 / 异步加载，去重缓存 |
| 导入 vs 运行时 | 编辑器 `UFactory`（FBX → `UStaticMesh`）生成 .uasset；运行时 `Serialize` 读 cooked 数据 | — | 运行时不解析 FBX |

可借鉴的核心：**资产对象（身份 + CPU 数据）与 Render Resource（GPU 数据）分离；资产负责驱动自己的 Render Resource 的创建与释放；管理器只负责"按路径取、去重、生命周期"；文件格式解析由各自的加载器负责。**

## 3. MyEngine 方案

### 3.1 分层与依赖方向

```
src/（TestApp）
  │ 只调用 FAssetManager::Load<T>(Path)，拿到资产后交给 Scene / Material
  ▼
Asset/（新增，Asset Layer）
  FAssetManager ─ 路径 → 资产缓存（去重、引用计数）、按扩展名分派 Loader
  IAssetLoader  ─ FOccCityAssetLoader(.bin) / FFbxStaticMeshLoader(.fbx) / 以后 FDdsTextureLoader(.dds)
  MAsset        ─ 资产基类：Path、InitResources / ReleaseResources
  MStaticMesh / MTexture2D ─ CPU 数据 + 持有自己的 Render Resource
  │ InitResources(FRenderResourceContext&)
  ▼
Renderer/（已有，Render Resource Layer）
  FMeshRenderData（→ 演进为 FStaticMeshRenderData）、FTextureResource
  FRenderResourceContext（新增）─ Device + 录制中的 CommandList + UploadContext + Bindless
  FDeferredReleaseQueue（新增）─ 按 fence 值延迟释放 GPU 资源
```

依赖单向：`src → Asset → Renderer → RHI/D3D12`。Renderer 不认识资产，`FScene` 仍只接收 `FMeshRenderData*` 与 `FMaterial*`（对应 UE 中 Renderer 只见 Render Data / Scene Proxy，不见 UObject）。

### 3.2 类型与职责（一类型一文件）

| 类型 | 文件 | 职责 |
|---|---|---|
| `MAsset` | Asset/MAsset.h | 资产基类：`GetPath()`、`virtual InitResources(FRenderResourceContext&)`、`virtual ReleaseResources(FDeferredReleaseQueue&)`、`bResourcesInitialized` |
| `MStaticMesh` | Asset/MStaticMesh.h/.cpp | 持有 `FStaticMesh`（CPU 源数据，可选择上传后丢弃）与 `std::unique_ptr<FMeshRenderData> RenderData`；`GetRenderData()` |
| `MTexture2D` | Asset/MTexture2D.h/.cpp | 持有像素数据描述（格式、尺寸、mip、数据）与 `FTextureResource`；`GetResource()` |
| `IAssetLoader` | Asset/IAssetLoader.h | `SupportsExtension()`、`Load(Path, FAssetManager&)`：解析文件，产出一个或多个资产 |
| `FAssetManager` | Asset/FAssetManager.h/.cpp | `Load<T>(Path)` 同步加载，返回 `std::shared_ptr<T>`（缓存存 `weak_ptr`，命中直接返回）、`CreateTransient<T>(Name, ...)` 注册程序生成的资产、`RegisterLoader` |
| `FOccCityAssetLoader` | Asset/Loaders/FOccCityAssetLoader.* | 把 `occcity.h` 的偏移解析从 Main.cpp 挪到这里，产出 `occcity.bin#City`（MStaticMesh）与 `occcity.bin#Diffuse`（MTexture2D） |
| `FFbxStaticMeshLoader` | Asset/Loaders/FFbxStaticMeshLoader.* | 包装现有 `MeshBuilder::LoadFromFBX`（Assimp 已能源码编译） |
| `FRenderResourceContext` | Renderer/FRenderResourceContext.h | 把现在散落在参数里的 `device / commandList / uploadContext / bindless / vertexFactory` 打包 |
| `FDeferredReleaseQueue` | Renderer/FDeferredReleaseQueue.* | `Enqueue(Resource, FenceValue)`，每帧 `ReleaseCompleted(CompletedFence)`；对应 UE 的 `ReleaseResourcesFence` |

### 3.3 资源路径

- 普通文件：`"Models/Chair.fbx"`（相对资源根目录，根目录在一处配置，不在逻辑中写死）。
- 一个文件含多个资产：`"occcity.bin#City"`、`"occcity.bin#Diffuse"`，`#` 后是子对象名，对应 UE 的 `Package.ObjectName`。Loader 解析整个文件，一次注册全部子对象。
- 程序生成：`CreateTransient<MStaticMesh>("Transient/Ground", ...)`，对应 UE 的 Transient 对象。

### 3.4 生命周期

```
Load(Path) ─ 命中缓存 → 返回
          └ 未命中 → Loader 解析磁盘数据 → 构造 MStaticMesh / MTexture2D（CPU 数据）
                   → InitResources(Context)：录制上传命令，创建 FMeshRenderData / FTextureResource
初始化结束 → FRenderer::FlushInitUploads()（沿用现有流程）
资产不再被引用 → ReleaseResources → GPU 资源进入 FDeferredReleaseQueue（记当前 fence）
每帧 → DeferredReleaseQueue.ReleaseCompleted(GPU 已完成的 fence) → 真正释放
```

v1 与现状一致：只在初始化期加载、同步上传。运行期加载（需要独立的上传批次与 fence）和异步加载（对应 `FStreamableManager`）在接口上预留，不在第一步实现。

### 3.5 改造后的 TestApp

```cpp
std::shared_ptr<MStaticMesh> CityMesh = Assets.Load<MStaticMesh>("occcity.bin#City");
std::shared_ptr<MTexture2D> CityTexture = Assets.Load<MTexture2D>("occcity.bin#Diffuse");

CityMaterial->DiffuseSrvArrayIndex = CityTexture->GetResource().SrvArrayIndex;
Scene.AddPrimitive(CityMesh->GetRenderData(), CityMaterial.get(), LocalToWorld);
```

`d3dUtil::LoadBinary`、`occcity.h` 偏移、`CreateMeshRenderData` 等调用从 Main.cpp 消失。

## 4. 分步实施（每步一个提交，行为不变可验证）

1. `FRenderResourceContext` + `MAsset` / `MStaticMesh` / `MTexture2D` + `FAssetManager` + `FOccCityAssetLoader`；Main.cpp 的城市改用 `Load`。画面不变。
2. 地面网格、棋盘格纹理改为 `CreateTransient`。
3. `FFbxStaticMeshLoader`，加一个 FBX 网格验证。
4. `FDeferredReleaseQueue` + 资产卸载。
5. 以后：材质资产（`MMaterial` 引用 `MTexture2D`）、运行期 / 异步加载、自定义 cooked 格式。

## 5. 设计决定（已全部确定）

| # | 问题 | 结论 / 选项 |
|---|---|---|
| 1 | 资产类型前缀 | **已定：`M` 前缀**（`MAsset`、`MStaticMesh`、`MTexture2D`、`MMaterial`），表示由 `FAssetManager` 托管的资产对象；与 UE 的 `U`（UObject，带 GC / 反射）区分。管理器、加载器等普通类仍用 `F` / `I` |
| 2 | 所有权 | **已定：`shared_ptr` 自动管理**。`Load` 返回 `std::shared_ptr<T>`，`FAssetManager` 缓存只存 `weak_ptr`；最后一个引用释放时，资产析构调用 `ReleaseResources`，GPU 资源进入 `FDeferredReleaseQueue`，等 fence 完成再真正释放 |
| 3 | 目录 | **已定：新建顶层 `Asset/`**。依赖方向 `src → Asset → Renderer`，`Common/` 不反向依赖渲染器 |
| 4 | 一个文件多个资产 | **已定：`文件#子对象` 路径**（如 `occcity.bin#City`、`occcity.bin#Diffuse`）。Loader 一次解析整个文件并注册全部子对象，不改数据文件 |
