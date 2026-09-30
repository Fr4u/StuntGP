#pragma once
#include "sgn/sdr_gamma.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace sgn
{
// GPU cache belongs to one Renderer/device, not to the source gamma owner.
// Each frame owns its input copy/descriptors/CB through that frame's fence.
struct SdrGammaFrame
{
    Microsoft::WRL::ComPtr<ID3D12Resource> input, constants;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptors;
    // NEW HOST (R27): per-channel ramp full scale used by the dithered pass
    // (written by prepare, read by record).
    float full_scale[3]{65535.0f, 65535.0f, 65535.0f};
};
// NEW HOST (R28b): one FXAA and/or smooth-gradient pass, built without touching
// an SdrGammaD3D12 (so it can be built off the render thread) and adopted later.
struct SdrGammaVariant
{
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
};
class SdrGammaD3D12 final
{
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    // NEW HOST (R23): combined FXAA + ramp pass, created by the first prepare()
    // that asks for it. The plain pipeline above is unchanged.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> fxaa_root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> fxaa_pipeline_;
    // NEW HOST (R27): smooth-gradient (dithered) variants of both passes, one
    // shared root with 8 root constants. Created by the first prepare(dither).
    Microsoft::WRL::ComPtr<ID3D12RootSignature> dither_root_, fxaa_dither_root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dither_pipeline_, fxaa_dither_pipeline_;

  public:
    explicit SdrGammaD3D12(ID3D12Device &);
    // NEW HOST (R28b): thread-safe builder (device + D3DCompile only) for fxaa
    // and/or dither; has_variant/adopt_variant run on the render thread.
    static SdrGammaVariant build_variant(ID3D12Device &, bool fxaa, bool dither);
    bool has_variant(bool fxaa, bool dither) const noexcept;
    void adopt_variant(bool fxaa, bool dither, SdrGammaVariant);
    // Caller has waited this frame's fence. All allocations/Map happen BEFORE
    // command-list reset; failed prepare never leaves a half-recorded list.
    // NEW HOST (R23): fxaa=true also builds the FXAA pipeline (may throw here,
    // never in record).
    // NEW HOST (R27): dither=true builds the smooth-gradient variant of the
    // requested pass and derives the ramp's full scale (see the .cpp).
    void prepare(SdrGammaFrame &, const D3D12_RESOURCE_DESC &, const SdrGammaRamp &, bool fxaa = false,
                 bool dither = false);
    // No throwing host operations/allocations. Input enters/leaves COPY_DEST;
    // back enters before, leaves RENDER_TARGET, ready for capture/Present.
    // NEW HOST (R23): fxaa=true (after a prepare with fxaa) runs FXAA 3.11
    // (quality algorithm) on the resolved image and applies the SAME exact ramp
    // lookup to its result in the same full-screen pass; fxaa=false is the
    // unchanged pass. NEW HOST (R27): dither=true (after a prepare with dither)
    // stores the ramp output with a static +-0.5 LSB dither instead of plain
    // rounding; fxaa=false/dither=false is the unchanged pre-R23 pass.
    void record(ID3D12GraphicsCommandList &, SdrGammaFrame &, ID3D12Resource &back, D3D12_RESOURCE_STATES before,
                D3D12_CPU_DESCRIPTOR_HANDLE output, unsigned width, unsigned height, bool fxaa = false,
                bool dither = false) const noexcept;
};
} // namespace sgn
