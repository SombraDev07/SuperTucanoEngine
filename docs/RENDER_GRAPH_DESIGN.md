# Render Graph Design (Milestone 6)

Study notes from DAGOR engine (`TucanoEngine-main/`, reference only) and design
for SuperTucanoEngine's declarative render pass graph.

## 1. DAGOR Reference — `daFG` (daFrameGraph)

Location: `prog/gameLibs/publicInclude/render/daFrameGraph/`

### Core concepts observed

- **`dafg::NameSpace`**: hierarchical namespace for nodes/resources. `dafg::root()`
  returns the root namespace. Nodes are registered per namespace, enabling
  modular graphs (e.g. game-specific passes can live in a sub-namespace).
- **`dafg::NodeHandle`**: opaque handle returned by `register_node(name,
  source_location, callback)`. The callback receives a `Registry` and declares
  the node's resource reads/writes + the execute lambda.
- **`Registry`** (inherits `BaseRegistry`): fluent API to declare resource usage
  within a node:
  - `read(name).texture().atStage(Stage::PS).bindToShaderVar(var)` — declare
    a texture read bound to a shader variable.
  - `create(name).texture(Texture2dCreateInfo)` — declare a created resource.
  - `draw(shader, DrawPrimitive::TRIANGLE_LIST).primitiveCount(n)` — declare a
    draw with a shader.
  - `postFx(shader_name)` — convenience for fullscreen post passes.
- **Multiplexing** (`multiplexing::Mode`, `multiplexing::Extents`): allows the
  same graph to run at multiple resolutions/slices (e.g. stereo VR, dynamic
  resolution, history frames for TAA). Resources tagged with a history mode
  persist across frames for temporal reprojection.
- **History** (`history.h`, `invalidate_history()`): temporal resources are
  preserved automatically across graph recompilations; can be invalidated when
  the graph structure changes.
- **External state** (`update_external_state(ExternalState)`): viewport, camera,
  resolution etc. pushed in each frame without triggering recompilation.
- **Resource slots** (`nameSpaceRequest.h`, `fill_slot`): plugin points where a
  game can inject a resource into a slot defined by the engine (e.g. a custom
  depth texture, a scene color target).
- **`run_nodes()`**: compiles (if needed) and executes the graph for the frame.
  Compilation is incremental and cached.

### Pass ordering in DAGOR (observed from `renderEvent.h`, `drawScene.cpp`)

Events/hooks broadcast via ECS:
- `BeforeDraw` (persp, frustum, camPos, dt) — prepass setup.
- `OnCameraNodeConstruction` (nodes_storage, slot_nodes_storage, flags) — each
  camera constructs its own subgraph; flags: `hasOpaquePrepass`,
  `giNeedsReprojection`, `needDepthHistory`, `hasMotionVectors`.
- `RenderPostFx` (downsampledColor, prevRTColor, closedDepth, targetDepth,
  zNear/zFar/fovScale) — post processing hook.
- `BeforeDrawPostFx`, `ChangeRenderFeatures`, `SetFxQuality`, `SetResolutionEvent`.

Passes implied by the tree (`prog/daNetGame/render/`):
depth prepass, cascaded shadows (`shadowsManager`, `cascadeShadows`,
`toroidalStaticShadows`, `voxelShadows`), GBuffer capture (`captureGbuffer`),
GI (`giObjectsES`), anti-aliasing (`antiAliasing.cpp`, FSR, XeSS, DLSS),
motion blur, film grain, FSR/XeSS upscalers, water, grass, skies
(`skies.cpp`/`DaSkies2`), volumetrics/weather (`weather/`).

### Shadow system reference (`world/shadowsManager.h`)

- `ShadowsQuality` enum: MIN/LOW/MEDIUM/HIGH/ULTRA_HIGH/STATIC_ONLY — quality
  presets map to cascade count and resolution.
- `CSM_MAX_CASCADES = 4`, `CSM_DYNAMIC_ONLY_CASCADE = 3`,
  `MAX_NUM_STATIC_SHADOWS_CASCADES = 2` — splits static and dynamic cascades.
- `IShadowInfoProvider` interface decouples shadow manager from world renderer:
  provides camera params, sun direction (STATIC vs CSM), rendinst visibilities,
  render feature flags, static shadow bbox, water/camera height, level settings,
  shadow frame counts, and callbacks `renderStaticSceneForShadowPass` /
  `renderDynamicsForShadowPass`. Comment: "Should be replaced with FG
  eventually" — confirms DAGOR is itself mid-migration to FG.
- Uses `gpuVisibilityTest`, `variance.h` (VSM/EVSM), `toroidalStaticShadows`
  (streaming toroidal shadow atlas for large static worlds), `voxelShadows`
  (for large-scale occlusion).

### Driver abstraction (`engine/drv/`)

- Multi-backend: `drv3d_DX11`, `drv3d_DX12`, `drv3d_vulkan`, `drv3d_Metal`,
  `drv3d_null`, `drv3d_stub`.
- Vulkan backend (`drv3d_vulkan/`) is extensive (~200+ files): explicit
  `execution_state`, `pipeline_barrier`, `render_pass_resource`, subpass
  dependency tracking, bindless, timeline semaphores, FSR/DLSS/XeSS integration
  stubs, raytrace AS resources, debug UI for memory/sync/resources.
- Note: DAGOR uses an **abstract driver interface** (`drv/3d/*.h` —
  `tuc_renderStates.h`, `tuc_renderTarget.h`, `tuc_texture.h`, etc.) so game
  code is backend-agnostic. The `tuc_` prefix is the engine's rename of the
  `d3d_` Gaijin prefix for the fork.

## 2. Design for SuperTucanoEngine — `Tucano::RenderGraph`

Keep it simpler than `daFG` (no multiplexing/VR yet) but adopt the same
declarative model: declare resources and passes, let the graph compile into a
barrier-correct execution schedule.

### Goals

- Replace the hardcoded `DrawFrame` ordering in `Renderer.cpp`.
- Enable incremental pass addition (shadows, GI, SSR, post) without touching
  core frame logic.
- Automatic image layout transitions and pipeline barriers based on declared
  usage (read/write/attachment).
- Resource aliasing/reuse to minimize VRAM (transient textures).
- Optional pass culling when outputs are unused.

### Proposed API sketch (to implement in Milestone 6)

```cpp
namespace Tucano::RG {

enum class ResourceType { Texture2D, Buffer };
enum class Stage { Vertex, Fragment, Compute, Transfer };
enum class Access { ShaderRead, ShaderWrite, ColorAttachment,
                    DepthAttachment, TransferDst, TransferSrc };

struct TextureDesc {
    uint32_t width = 0, height = 0;  // 0 = swapchain-relative
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
};

class ResourceHandle { uint32_t id; };
class PassHandle { uint32_t id; };

class RenderGraph {
public:
    // Declaration phase
    ResourceHandle createTexture(const char* name, TextureDesc desc);
    PassHandle addPass(const char* name, Stage stage,
                       std::function<void(ExecutionContext&)> exec);

    // Per-pass resource binding (inside the addPass callback)
    void read(PassHandle p, ResourceHandle r, Access a, Stage s);
    void write(PassHandle p, ResourceHandle r, Access a, Stage s);

    // External swapchain target (presented)
    ResourceHandle getSwapchainOutput();

    // Compile + execute
    bool compile();   // topo sort, barrier planning, resource lifetime
    void execute(VkCommandBuffer cmd, uint32_t frameIdx);

private:
    // ... DAG of passes, resource states, barriers
};

} // namespace Tucano::RG
```

### Planned pass order (target frame)

```
[ Depth Prepass        ]  -> depth texture (early-Z, reduces overdraw)
[ Shadow Passes (CSM)  ]  -> shadow atlas (directional, then spot/point)
[ Visibility/Cull       ]  -> indirect draw counts (optional, for Forward+)
[ Opaque Forward+       ]  -> HDR color + writes depth (clustered lights)
[ Atmosphere Sky        ]  -> sky into HDR color where depth == far
[ Volumetric Fog        ]  -> 3D froxel volume (aerial perspective merged)
[ Lighting Resolve      ]  -> applies volumetrics + GI to HDR color (if deferred split)
[ SSR                   ]  -> reflections buffer
[ SSAO/GTAO             ]  -> ambient occlusion buffer
[ Reflections Composite  ]  -> merge SSR + probes into HDR color
[ Transparent            ]  -> forward transparent over HDR color
[ Post Process           ]  -> bloom, DOF, motion blur (HDR)
[ Tonemap                ]  -> HDR -> LDR
[ Upscaler (FSR/XeSS)    ]  -> LDR upscaled (if dynamic res)
[ UI / ImGui             ]  -> LDR final
```

### Migration plan from current `DrawFrame`

1. Extract the existing render pass + framebuffer + clear into an
   `RG::Pass` "OpaqueGeometry" that writes `HDRColor` + `Depth`. Atmosphere
   computes stay outside the graph (they feed textures consumed by passes).
2. Extract `RenderSky` into a graph pass that reads `Depth` (as
   `DepthAttachment`, `LESS_OR_EQUAL`, write=FALSE) and writes `HDRColor`.
3. Extract ImGui into a final pass that writes the swapchain output.
4. Add a depth prepass as a new pass before opaque.
5. Add shadow passes (Milestone 8) as producers of a `ShadowAtlas` resource.
6. Move atmosphere compute dispatches into a "AtmospherePrecompute" compute
   pass (or keep them as graph-external setup, since they rarely change).

### What to keep from DAGOR vs. modernize

| Aspect | DAGOR approach | SuperTucano decision |
|---|---|---|
| Render Graph | `daFG` with multiplexing, slots, namespaces | Adopt core: nodes + registry + history. Skip multiplexing/slots initially. |
| Shadow quality presets | Enum + runtime switch | Adopt: `ShadowsQuality` enum driving cascade count/res. |
| Static/dynamic shadow split | `toroidalStaticShadows` + separate dynamic cascade | Modernize: toroidal streaming is AAA-large-world; start with simple CSM (4 cascades), add static caching later. |
| VSM/EVSM | `variance.h` + `MomentShadowMaps` | Defer to Milestone 8 phase 2; start with PCF then PCSS. |
| Driver abstraction | `drv/3d/*` abstract API across D3D11/12/Vulkan/Metal | Skip: SuperTucano is Vulkan-only. Keep `VulkanContext` as the single backend. |
| ECS-driven render events | `BeforeDraw`/`RenderPostFx` broadcast | Defer: only worth it once gameplay systems need hooks. Start with explicit `Renderer::Render(world, camera)` calls. |
| FSR/XeSS/DLSS | Per-upscaler classes implementing `AntiAliasing` interface | Adopt the interface pattern; implement FSR via FidelityFX SDK first (Milestone 14). |
| GI | `giObjectsES`, `DaGI`, DDGI-style | Defer to Milestone 10; reference DDGI (Zinenko 2019) for medium tier. |

### Outdated techniques to avoid (use modern equivalents)

- **Classic SSAO** → use **GTAO** (Voutilainen 2015) or **HBAO+** as the new
  baseline; SSAO is entry-level only.
- **Forward clássico** → **Clustered Forward+** (Olsson 2015) is the 2025
  baseline for many lights; defer to optional hybrid only if G-buffer bandwidth
  becomes an issue.
- **Tonemap operator Reinhard** (`color/(color+1)`) → keep filmic ACES or
  **Khronos PBR Neutral** as default; already using `1 - exp(-color*exposure)`
  which is acceptable but ACES is preferred for filmic look.
- **Screen-space reflections only** → SSR as low tier, RT reflections
  (via `VK_KHR_ray_tracing_pipeline`) as high tier.
- **Shadow PCF only** → PCF as low, **PCSS** for soft contact shadows, EVSM for
  large-area penumbra; ray-traced shadows as optional ultra.

## 3. Action items for Milestone 6 (Render Graph)

1. Create `TucanoCore/include/Tucano/Render/RenderGraph.h` with the API above.
2. Implement `Resource` (texture/buffer + state tracking) and `Pass` (exec
   callback + resource refs).
3. Implement `compile()`: topological sort, deduce barriers from
   last-write/next-read, allocate transient resources via VMA, alias lifetimes.
4. Implement `execute()`: record barriers + dispatch/draw via the exec
   callbacks into the per-frame command buffer.
5. Refactor `Renderer::DrawFrame` to: build the graph, compile (cache by
   signature hash), execute. Keep atmosphere compute as a pre-graph setup step.
6. Add `DepthPrepass` pass (Z-only draw of opaque geometry) to feed early-Z and
   SSR/SSAO depth reads.
7. Validate with RenderDoc: barriers match declared usage, no validation layer
   errors, image layouts correct at every pass boundary.