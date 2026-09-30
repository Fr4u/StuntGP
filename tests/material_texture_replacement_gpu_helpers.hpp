#pragma once
#include "sgn/material_d3d12.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace sgn;using namespace sgn::retail;using Microsoft::WRL::ComPtr;
namespace {
void require(bool condition,const char* message){if(!condition)throw FormatError(message);}
void check(HRESULT value){require(SUCCEEDED(value),"material GPU readback HRESULT");}
struct Window {HWND handle{};~Window(){if(handle)DestroyWindow(handle);}};
struct Event {HANDLE handle{};~Event(){if(handle)CloseHandle(handle);}};
void write_words(const std::filesystem::path& path,std::span<const std::uint16_t> words){
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(words.data()),static_cast<std::streamsize>(words.size_bytes()));require(bool(out),"cannot write material pixel evidence");
}
const PolygonGpuTexture& texture(const PolygonGpuTextures& bank,std::uint32_t token){
    const auto found=std::find_if(bank.entries.begin(),bank.entries.end(),[&](const auto& item){return item.token==token;});
    require(found!=bank.entries.end(),"completed material token absent from actual GPU bank");return *found;
}
std::vector<std::uint16_t> read_words(const PolygonGpuTexture& texture){
    require(texture.format==DXGI_FORMAT_B5G5R5A1_UNORM&&texture.resource!=nullptr,"actual material format/resource mismatch");
    ComPtr<ID3D12Device> device;check(texture.resource->GetDevice(IID_PPV_ARGS(&device)));
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    const auto description=texture.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};UINT64 bytes{};
    device->GetCopyableFootprints(&description,0,1,0,&layout,nullptr,nullptr,&bytes);
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
    buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;heap.CreationNodeMask=heap.VisibleNodeMask=1;
    ComPtr<ID3D12Resource> readback;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={texture.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=texture.resource.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.pResource=readback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=layout;list->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);check(list->Close());
    ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    check(queue->Signal(fence.Get(),1));Event event{CreateEventW(nullptr,FALSE,FALSE,nullptr)};require(event.handle!=nullptr,"material fence event");
    check(fence->SetEventOnCompletion(1,event.handle));require(WaitForSingleObject(event.handle,15000)==WAIT_OBJECT_0,"material readback timeout");
    check(device->GetDeviceRemovedReason());void* mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};check(readback->Map(0,&range,&mapped));
    const auto width=static_cast<std::size_t>(description.Width);std::vector<std::uint16_t> result(width*description.Height);
    for(unsigned y=0;y<description.Height;++y)std::memcpy(result.data()+y*width,static_cast<const std::byte*>(mapped)+layout.Offset+std::size_t(y)*layout.Footprint.RowPitch,width*2);
    D3D12_RANGE empty{};readback->Unmap(0,&empty);return result;
}
PolygonDeviceState device_seed(std::uint32_t token){
    PolygonDeviceState s;for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,1},{7,1},{8,3},{9,2},{14,1},{15,1},{19,2},{20,1},{22,1},{23,7},{24,1},{25,7},{27,0},{28,0},{29,0}})s.render[i]=v;
    for(unsigned stage=0;stage<2;++stage)for(auto [i,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{1,stage?1u:4u},{2,2},{3,0},{4,4},{5,2},{6,0},{11,stage},{13,3},{14,3},{16,1},{17,1},{18,0}})s.stage[stage][i]=v;
    s.render[41]=0;s.texture[0]=token;s.texture[1]=0;return s; //Explicit modern test state.
}
void draw_material(Renderer& renderer,const PolygonGpuTextures& bank,std::uint32_t token,const std::filesystem::path& path){
    const auto& resource=texture(bank,token);const auto words=read_words(resource);const auto description=resource.resource->GetDesc();
    const auto expected=words[(description.Height/2)*static_cast<std::size_t>(description.Width)+static_cast<std::size_t>(description.Width)/2];
    CapturedPolygonDraw draw;draw.fvf=0x2c4;draw.flags=0x1c;draw.type=4;draw.viewport.full_width=384;draw.viewport.full_height=256;
    for(const auto [x,y]:std::array<std::array<float,2>,6>{{{16,16},{368,16},{368,240},{16,16},{368,240},{16,240}}})
        draw.vertices.push_back({{{std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,0xffffffff,0,0x3f000000,0x3f000000,0x7fc00001,0xdeadbeef}}});
    CapturedPolygonFrame frame;frame.source_tick=90;frame.resource_epoch=bank.epoch;frame.commands.push_back(std::move(draw));
    renderer.draw_polygons(frame,device_seed(token),std::make_shared<const PolygonGpuTextures>(bank));renderer.capture(path);const auto bytes=read_file(path);
    constexpr std::size_t pixel=54+(128*384+192)*4;
    for(unsigned channel=0;channel<3;++channel){const unsigned value=(expected&0x8000)?((expected>>(10-5*channel))&31):0;
        const auto wanted=static_cast<int>(std::lround(double(value)*255/31));require(std::abs(int(bytes[pixel+2-channel])-wanted)<=1,"material token sampled the wrong GPU texture");}
}
void healthy(Renderer& renderer){renderer.finish();renderer.collect_diagnostics();renderer.check_health();
    for(const auto& diagnostic:renderer.diagnostics())std::cerr<<diagnostic.message<<'\n';
    require(renderer.stats().debug_active&&renderer.stats().debug_errors==0,"material GPU debug warning/error");}
}
