#pragma once
#include <d3dcompiler.h>

// NEW HOST (R35) shader bytecode cache (not recovered behaviour). Every HLSL compile of the host renderer
// (renderer, polygon backend, gamma, post process, gpu_world) goes through compile(), which has exactly
// the contract of D3DCompile without an include handler: the same HRESULT, the bytecode blob on success,
// the error blob on failure. A successful result is kept in memory for the process and, once the settings
// folder is known (host_texture_disk_cache::configure, beside it: <settings>/shader-cache), on disk, so a
// later run (of any build whose shader text is unchanged) skips D3DCompile at startup. A cached result is
// used only when EVERYTHING that decides the bytecode is byte-identical: the source text, the source name,
// the macros, the entry point, the target, both flag words, and the identity of the loaded compiler DLL
// (d3dcompiler_47: PE link timestamp, image size, checksum). The stored key material is compared in full
// and the bytecode carries a 64-bit checksum, so a torn or foreign file is ignored and recompiled.
// D3DCompile is deterministic for identical inputs, so the bytecode, the pipelines and every pixel are
// the same with and without the cache. SGN_SHADER_CACHE=0 disables the disk part. Any thread.
namespace sgn::host_shader_cache
{
HRESULT compile(LPCVOID source, SIZE_T size, LPCSTR name, const D3D_SHADER_MACRO *defines, LPCSTR entry, LPCSTR target,
                UINT flags1, UINT flags2, ID3DBlob **code, ID3DBlob **errors) noexcept;
struct Statistics
{
    unsigned long long compiled{}, memory_hits{}, disk_hits{}, disk_rejected{}, written{};
    double compile_ms{};
};
Statistics statistics() noexcept;
} // namespace sgn::host_shader_cache
