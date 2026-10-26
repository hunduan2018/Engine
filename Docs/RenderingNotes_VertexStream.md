# 渲染问答整理：顶点流、实例化与输入布局

整理自 2026-10 的几轮讨论。MyEngine 部分基于提交 `1706eee`（Mesh Draw Command 架构）的代码；UE 部分基于 `C:\UnrealEngine5`（5.8.3），路径相对 `Engine/Source/Runtime/`，行号仅作提示，以 Grep 为准。

---

## 一、MyEngine 渲染器

### 1. `FDrawSubmitter` 里拷贝顶点缓冲视图的作用

位置：`Renderer/FDrawSubmitter.cpp:117-135`。

```cpp
if (CurrentVertexBufferCount != command.VertexStreamCount)
{
    commandList->IASetVertexBuffers(0, command.VertexStreamCount, command.VertexBufferViews);
    memcpy(CurrentVertexBuffers, command.VertexBufferViews, sizeof(CurrentVertexBuffers));
    CurrentVertexBufferCount = command.VertexStreamCount;
}
else { /* 逐流 memcmp，不同才重设并更新副本 */ }
```

- **它不是每次 Draw 都执行**：只在顶点流数量变化时走整组重设分支；当前场景全是单流，每帧只有第一条命令走这里。
- **作用是维护"当前绑定状态"的 CPU 副本**。D3D12 命令列表不提供查询当前绑定的接口，要实现"相同就跳过 `IASetVertexBuffers`"，CPU 必须自己记住上次设了什么。
- **两样东西要分清**：`command.VertexBufferViews` 是"这条命令想用什么"；`CurrentVertexBuffers` 是"命令列表此刻实际绑定了什么"。Draw 本身不带顶点缓冲参数，用的是最近一次 `IASetVertexBuffers` 留下的状态。
- **只要保留"相同就跳过"的缓存，这行拷贝就必须有**。设置了却不更新副本，副本会过期；过期副本碰巧与新命令相同时，会错误地跳过绑定：

  | 命令 | 流 | 分支 | GPU 实际绑定 | 副本（无 memcpy 时） |
  |---|---|---|---|---|
  | C1 | 1 流 VB_A | 流数 0→1 整组设 | [VB_A] | [0] |
  | C2 | 1 流 VB_B | 逐流比较，设 | [VB_B] | [VB_B] |
  | C3 | 2 流 VB_A, VB_C | 流数 1→2 整组设 | [VB_A, VB_C] | [VB_B, 0] |
  | C4 | 2 流 VB_B, VB_D | 流 0 副本 = VB_B，**误判相同而跳过** | [**VB_A**, VB_D] | |

- **两种写法**：
  - A. 每条命令都 `IASetVertexBuffers`：简单、正确，不需要副本。
  - B. 相同就跳过：需要副本，且每次设置后都要同步更新。
  - 现在每帧命令只有几条，B 省下的调用微乎其微，改成 A 完全合理；若改，要把副本数组、流数和比较分支**整体删除**，不能只删 memcpy。PSO / 根签名 / 根参数的过滤建议保留。

### 2. 实例化已经存在

- `Renderer/FScene.cpp:16-39`：`BuildMeshBatches` 把 (Mesh, Section, Material) 相同的图元合成一个 `FMeshBatch`，矩阵追加到 `batch.Instances`。
- `Renderer/FMeshPassProcessor.cpp:31-76`：一个批次一条命令，`InstanceCount = batch.Instances.size()`；实例矩阵写入实例缓冲，起始下标经根常量 b2 传给着色器（`SV_InstanceID` 不含 `StartInstanceLocation`）。
- 结果：16 个城市 = 1 次 `DrawIndexedInstanced`，每帧 BasePass 只有 2 条命令（调试输出 `commands=2`）。
- 顶点流状态缓存的适用场景是**无法合并**但共用顶点缓冲的命令：同一网格的不同 Section、同一网格配不同材质（如地面实体 / 线框）。实例合并与状态缓存是互补关系。

### 3. 地面的顶点数据并不每帧变化

- `Renderer/FMeshRenderData.cpp:29-41`：顶点缓冲在默认堆，初始化时上传一次；`VertexBufferView`（GPU 地址 / 大小 / 步长）固定不变。
- 每帧仍要绑定和比较，是因为**命令列表 `Reset` 后所有绑定恢复默认**，绑定状态不跨帧保留；`FRenderer::BeginFrame` 里的 `DrawSubmitter.ResetCache()` 与之对应。状态缓存省的是**同一帧内**相邻命令的重复绑定。

### 4. 地面动画为什么在着色器里做

位置：`Shaders/shader_tess_domain.hlsl:21-50`，`y = A·sin(x·k + t)·cos(z·k + t)`，时间来自 `g_ViewOriginTime.w`。

- **要动的顶点不在顶点缓冲里**：顶点缓冲只有 11×11 格、242 个 patch、726 个控制点；细分因子 1–16，因子 16 时约 6 万三角形，细分顶点只存在于 GPU 管线内部，只有域着色器能逐个处理。
- **只动控制点表达不出波形**：波长 2π/0.3 ≈ 21 单位，格子边长 20，控制点采样远低于所需密度；细分后在控制点之间线性插值只会得到平面。
- **适合 GPU**：位移是位置和时间的独立函数，法线用解析偏导数；CPU 每帧只传一个时间值。若在 CPU 改顶点缓冲，需要每帧槽一份缓冲 + 上传 + 屏障。
- **不适合着色器的情况**：CPU 需要变形结果（碰撞 / 物理 / 拾取），或多个通道要复用同一份昂贵的变形结果（用计算着色器先算好）。

### 5. 观察：实例矩阵每帧重新上传

`FMeshPassProcessor` 每帧把 16 个城市矩阵重新写入实例缓冲，再 `FlushInstanceCopies` 拷到默认堆，即使城市静止。当前规模可忽略；场景变大后可只上传变化的矩阵（对应 UE 的 GPUScene 做法）。

---

## 二、UE5 的顶点工厂与顶点流

### 6. 顶点工厂怎样处理顶点流

**核心思路：布局与绑定分离。** 布局（Vertex Declaration）在顶点工厂初始化时生成一次并进入 PSO；绑定（每个槽对应哪个缓冲、偏移）拷进每条 `FMeshDrawCommand`，提交时逐槽比对状态缓存。

**三层数据结构**

| 层级 | 类型 | 位置 | 含义 |
|---|---|---|---|
| 属性来源 | `FVertexStreamComponent` | RenderCore/Public/VertexStreamComponent.h:21 | 属性在哪个缓冲：缓冲、StreamOffset、元素内 Offset、Stride、类型、用途 |
| 去重后的流槽 | `FVertexFactory::FVertexStream` | RenderCore/Public/VertexFactory.h:819 | 实际绑定的槽，存在顶点工厂的 `Streams` 列表 |
| 每条命令的绑定 | `FVertexInputStream` | VertexFactory.h:48 | 槽号 + 偏移 + RHI 缓冲；命令里用内联容量 7 的数组存放 |

属性与槽是多对一：`AccessStreamComponent`（VertexFactory.h:799）为属性生成 `FVertexElement`，内部 `AddVertexStream` 用 `AddUnique` 分配槽，同一交错缓冲的属性共用一个槽。

**初始化生成三套流**（`Engine/Private/LocalVertexFactory.cpp:501` `FLocalVertexFactory::InitRHI`）

- 默认流：Position→ATTRIBUTE0、TangentX/Z→1/2、Color→3、TexCoords→4 起、PreSkinPosition→14、光照图 UV→15；声明经 `PipelineStateCache::GetOrCreateVertexDeclaration` 全局去重。
- PositionOnly / PositionAndNormalOnly：位置在单独缓冲时才生成，供深度、阴影通道少读带宽。
- PrimitiveId 流：开 GPUScene 时加一个 `Instancing` 类型、ATTRIBUTE13 的流，初始化绑占位缓冲 `GPrimitiveIdDummy`，提交时替换。

**用途标志 `EVertexStreamUsage`**（VertexStreamComponent.h:10）

| 标志 | 含义 |
|---|---|
| `Default` | 普通逐顶点流，走 IA |
| `Instancing` | 按实例步进；`OffsetInstanceStreams` 调整偏移 |
| `Overridden` | 由绘制元素临时提供，`GetStreams` 跳过，着色器绑定阶段补上（如逐组件覆盖顶点色 `GetColorOverrideStream`） |
| `ManualFetch` | 支持手动顶点读取（MVF）时着色器用 SRV 自读，`GetStreams` 跳过 |

无顶点色时绑定**步长为 0 的空颜色流**：每顶点浪费 4 字节，换取顶点工厂排列不翻倍。

**进入命令与提交**

- `FMeshPassProcessor::BuildMeshDrawCommands`（Renderer/Public/MeshPassProcessor.inl:57）：按通道特性选流类型 → 声明进 PSO → `GetStreams` 拷入 `MeshDrawCommand.VertexStreams` → 记录 `PrimitiveIdStreamIndex` → 逐元素 `GetElementShaderBindings` 追加覆盖流。
- `FMeshDrawCommand::SubmitDrawBegin`（Renderer/Private/MeshPassProcessor.cpp:1311-1325）：PrimitiveId 槽每次都 `SetStreamSource` 到当前通道的实例 Id 缓冲；其余槽与 `FMeshDrawCommandStateCache` 比较，不同才设置；PSO 变化时清空缓存。思路与 MyEngine 的 `FDrawSubmitter` 相同。

### 7. MVF 的 SRV 在哪里绑定；为什么仍声明切线 / 颜色 / UV

**SRV 绑定链路**（不在 `GetVertexElements`）

1. `Engine/Private/StaticMesh.cpp:1119-1147` 渲染线程：`BindPositionVertexBuffer` / `BindTangentVertexBuffer` / `BindPackedTexCoordVertexBuffer` / `BindColorVertexBuffer` 填 `FDataType`，然后 `SetData` + `InitResource`。
   - 以切线为例（`Rendering/StaticMeshVertexBuffer.cpp:511`）：同时填 `Data.TangentsSRV`（MVF 用）和带 `ManualFetch` 标记的流描述（IA 用）。位置流**不带** `ManualFetch`，始终走 IA。
2. `FLocalVertexFactory::InitRHI` → `CreateLocalVFUniformBuffer`（LocalVertexFactory.cpp:549）→ `GetLocalVFUniformShaderParameters`（:82）：平台支持 MVF 时把各 SRV 填进顶点工厂 Uniform Buffer，否则填空缓冲。
3. 生成命令：`GetElementShaderBindingsBase`（:156）`ShaderBindings.Add(LocalVF 参数, VertexFactoryUniformBuffer)`（:177）进 `FMeshDrawCommand::ShaderBindings`；元素可提供覆盖用的 UB（覆盖顶点色）。
4. 提交：`ShaderBindings.SetOnCommandList`（MeshPassProcessor.cpp:1327）；着色器经 `LocalVF.VertexFetch_*` 按 `SV_VertexID` 读取。

**为什么仍声明**（LocalVertexFactory.cpp:596-603）

```cpp
#if !WITH_EDITOR
if (!bSupportsManualVertexFetch)
#endif
{ ... 切线 / 颜色 / UV 的 Element ... }
```

- 打包版本：支持 MVF 时不声明，声明只剩 ATTRIBUTE0 与 ATTRIBUTE13。
- 编辑器：条件被编译掉，**无条件声明**。原因（UE-165187）：编辑器可切 ES3.1 预览，该特性级别不支持 MVF，着色器从 IA 读 ATTRIBUTE1~7；顶点工厂按桌面最高特性级别创建，声明缺这些属性会导致 PSO 创建失败。
- 不会白绑：`GetStreams` 按**本次绘制的特性级别**跳过 `ManualFetch` 流（VertexFactory.cpp:267-271）；MVF 变体的着色器输入里没有 ATTRIBUTE1~7（`LocalVertexFactory.ush:54-96`），多出的声明无人读取。

### 8. `FD3D12VertexDeclarationKey` 的语义名

位置：`D3D12RHI/Private/D3D12VertexDeclaration.cpp:12-112`。

- 语义名**有给**，只是延后：构造元素时只设 `SemanticIndex = AttributeIndex`（:63），对整个 `D3D12_INPUT_ELEMENT_DESC` 数组做内存 CRC 后，才统一 `SemanticName = "ATTRIBUTE"`（:106-110）。
- 延后原因：`SemanticName` 是指针，按字节哈希会把地址算进去，地址每次启动可能不同；名字对所有元素都一样，无需参与哈希。
- 着色器对应：HLSL `ATTRIBUTE13` 被解析为名字 `ATTRIBUTE` + 索引 13。对应关系完全由**编号约定**决定，由顶点工厂的 C++ 与其 `.ush` 共同维护（`IMPLEMENT_VERTEX_FACTORY_TYPE` 把二者绑定）。
- 统一名字的好处：Vulkan / Metal / OpenGL 只有位置编号，UE 着色器编译器把 `ATTRIBUTEn` 翻译为编号 n；属性含义由顶点工厂决定；声明缓存只需比较编号、格式、槽、偏移、步长。

### 9. D3D12 的语义匹配规则

名字可以任意取，但要满足：

1. 顶点着色器输入签名用到的每个语义（名字 + 索引），输入布局都必须有，否则 PSO 创建失败；布局多出的元素会被忽略。
2. 按名字匹配，与元素顺序无关。
3. 名字不区分大小写。
4. 编号放 `SemanticIndex`，不要拼进名字（HLSL 会把末尾数字解析为索引）。
5. 不能用 `SV_` 前缀（系统值语义，由硬件生成，不来自输入布局）。
6. 格式与着色器类型兼容：`*_UINT` ↔ `uint`、`*_SINT` ↔ `int`、浮点 / UNORM / SNORM ↔ `float` / `half`；分量数可不同，缺省分量补 (0, 0, 0, 1)。
7. 使用输入布局时，根签名必须带 `D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`。

维护难点是 C++ 布局与 HLSL 输入两处不同步，问题到创建 PSO 才暴露；集中定义属性编号可降低风险。

### 10. 为什么根签名要有 IA 标志

- 输入布局（PSO 里）描述**顶点数据格式**；根签名标志决定**要不要为顶点缓冲绑定预留空间**。
- 根签名定义整条管线外部绑定的总布局。不少 GPU 上 IA 由驱动在顶点着色器前插入取数代码实现，顶点缓冲地址占用与根参数共享的有限绑定资源，所以编译根签名时要提前知道。
- PSO 带非空输入布局而根签名无此标志 → 创建失败；有标志而不用布局 → 合法，只浪费少量空间。
- UE 按需设置：`D3D12RHI/Private/D3D12Util.cpp:1419` `bAllowIAInputLayout = BSS.VertexDeclarationRHI != nullptr`；`D3D12RootSignature.cpp:377-380` 据此加标志；计算 / 光追根签名断言为假（D3D12Util.cpp:1512、:1559）。

### 11. 不设 IA 标志：计算管线、图形管线与性能

- **计算管线**：没有 IA 阶段，不应设置。
- **图形管线不设标志**：输入布局必须为空，顶点着色器只能拿到 `SV_VertexID` / `SV_InstanceID`，顶点数据要自己从缓冲读取（Vertex Pulling / MVF）或程序生成（全屏三角形）。
- **"不启用 IA"不准确**：标志只涉及顶点缓冲与输入布局。索引缓冲 + `DrawIndexedInstanced` 仍可用（此时 `SV_VertexID` 为索引值加 `BaseVertexLocation`），拓扑照常生效，按索引复用的变换结果缓存照常生效。
- **性能**：
  - 去掉标志本身：部分硬件上节省一点根参数空间，收益很小。
  - IA 改为手动读取：取决于硬件，没有稳定优势；手动读取要在着色器里解码格式。
  - 改为手动读取的真正理由是**灵活性**：顶点格式不进 PSO（组合更少，UE 的主要动机）、自由压缩 / bindless、为网格着色器铺路。UE 开 MVF 后**位置仍走 IA**。

---

## 三、MyEngine 可选改进清单（均未实施）

| 项 | 内容 | 动机 |
|---|---|---|
| 顶点流提交 | 顶点流改为每条命令都设置，删除副本与比较分支 | 简化；当前命令数少，过滤收益很小 |
| 属性编号约定 | `FVertexFactory` 集中定义属性编号，HLSL 用 `ATTRIBUTEn` | 多顶点工厂 / 多后端时减少布局与着色器不同步 |
| 精简流 | 深度 / 阴影通道用只含位置的流 | 加深度预通道或阴影后节省带宽 |
| 实例数据常驻 | 只上传变化的实例矩阵 | 场景变大后减少每帧上传 |
| 无 IA 的根签名 | 全屏 / 计算 / 网格着色器通道单独用不带 IA 标志的根签名 | 收益很小，锦上添花 |
| 手动顶点读取 | bindless 网格或网格着色器阶段再考虑 | 架构灵活性，不是性能 |
