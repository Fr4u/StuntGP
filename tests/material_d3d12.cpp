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
struct Observer final:MaterialTextureBackend {
    MaterialD3D12& gpu;bool split;std::filesystem::path output;
    unsigned uploads{},generated{},pairs{},releases{};std::uint64_t compared_bytes{};
    Observer(MaterialD3D12& backend,bool separate,std::filesystem::path path):gpu(backend),split(separate),output(std::move(path)){std::filesystem::create_directories(output);}
    void release_all() override{gpu.release_all();++releases;}
    std::uint8_t& current_transparency() override{return gpu.current_transparency();}
    void begin_pending_record(std::uint32_t motion) override{gpu.begin_pending_record(motion);}
    void inspect(CompletedMaterialUpload result,unsigned width,unsigned height,std::span<const std::uint16_t> expected,bool is_generated,std::uint8_t transparency){
        const auto bank=gpu.bindings_snapshot();const auto& first=texture(*bank,result.primary);const auto& second=texture(*bank,result.alternate);
        require(result.dimension==static_cast<std::uint16_t>(width),"material dimension is not source low width word");
        require((result.primary!=result.alternate)==split,"material handle identity does not follow capability bit");
        require((first.resource.Get()!=second.resource.Get())==split,"material surfaces do not follow handle identity");
        require(first.source_creation&&second.source_creation,"material creation metadata discarded");
        const auto flags=0x101007u|(transparency?0x10000u:0u);
        require(first.source_creation->descriptor_flags==flags&&second.source_creation->descriptor_flags==flags&&
            first.source_creation->texture_stage==0&&second.source_creation->texture_stage==(split?1u:0u)&&
            first.source_creation->transparency_byte==transparency&&second.source_creation->transparency_byte==transparency,"material retained source descriptor state differs");
        const auto description=first.resource->GetDesc();require(description.Width==width&&description.Height==height,"material allocation dimensions changed");
        const auto actual=read_words(first);const auto alternate=read_words(second);require(actual==alternate,"alternate material was not copied from the same initial content");
        const auto name=std::to_string(pairs++)+(is_generated?"-generated":"-file");write_words(output/(name+"-primary.u16"),actual);write_words(output/(name+"-alternate.u16"),alternate);
        if(!is_generated){require(std::equal(actual.begin(),actual.end(),expected.begin(),expected.end()),"actual GPU material words differ from prepared source pixels");write_words(output/(name+"-expected.u16"),expected);compared_bytes+=actual.size()*2;}
    }
    CompletedMaterialUpload upload(PreparedMaterialPixels pixels) override{
        const auto transparency=gpu.current_transparency();const auto result=gpu.upload(pixels);inspect(result,pixels.width,pixels.height,pixels.words,false,transparency);++uploads;return result;
    }
    CompletedMaterialUpload generate(std::uint32_t dimension) override{
        const auto transparency=gpu.current_transparency();const auto result=gpu.generate(dimension);inspect(result,dimension,dimension,{},true,transparency);++generated;return result;
    }
};
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

int main(int argc,char** argv){try{
    require(argc==4,"expected --warp|--hardware retail-root fresh-output");const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","invalid material test adapter");const auto output=std::filesystem::absolute(argv[3]);
    require(!std::filesystem::exists(output),"material evidence directory already exists");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Material-GPU-Probe";
    require(RegisterClassW(&wc)!=0,"material window class");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW material GPU qualification",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"material hidden window");Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(384,256);
    AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);unsigned guards=0,uploads=0,generated=0,pairs=0,frames=0;std::uint64_t compared_bytes=0;
    for(bool split:{false,true}){
        MaterialTransparencyStorage storage;MaterialD3D12 gpu(renderer,storage,split,split?1000:1);
        Observer observer(gpu,split,output/(split?"split":"alias"));
        try{(void)gpu.current_transparency();}catch(const FormatError&){++guards;}
        gpu.reset_transparency_430690();require(storage.cursor==0u&&std::all_of(storage.bytes.begin(),storage.bytes.end(),[](auto b){return b==0;}),"explicit transparency reset failed");
        gpu.current_transparency()=1;observer.begin_pending_record(17);require(gpu.current_transparency()==1,"begin cleared retained transparency");
        const std::array<std::uint16_t,6> direct{0xfc00,0x81e0,0x8007,0xffff,0x7fff,0x8000};observer.upload({2,3,direct});require(storage.cursor==1u,"upload did not advance independent byte cursor");
        auto retained=gpu.bindings_snapshot();const auto old_epoch=gpu.resource_epoch();gpu.current_transparency()=0x44;observer.release_all();
        require(gpu.resource_entries()==0&&gpu.resource_epoch()==old_epoch+1&&storage.cursor==1u&&gpu.current_transparency()==0x44,"resource release reset another source cursor");
        require(read_words(retained->entries.front())==std::vector<std::uint16_t>(direct.begin(),direct.end()),"resource snapshot lost texture after release");retained.reset();
        observer.begin_pending_record(0);observer.generate(16);require(storage.cursor==2u&&storage.bytes[1]==0x44,"generated upload cleared retained transparency or advanced wrong cursor");
        const auto source_roots=source_graphics_roots_from_pe();
        auto materials=std::make_unique<WorldMaterialLoadingState>();WorldMaterialLoadingContext context;context.source_graphics_roots=&source_roots;context.maximum_dimension=0x3ffff8;context.quality=0;
        for(unsigned scene:{0u,2u}){
            observer.release_all();gpu.reset_transparency_430690();context.scene=scene;
            load_world_materials({},*materials,context,files,observer);
            require(materials->names.physical_upload_count==gpu.resource_entries()&&storage.cursor==gpu.resource_entries(),"initial material/table/byte counts disagree");
            const auto before=observer.uploads;
            const std::array<std::string,3> requests{{"Misc\\LIGHT1","FrontEnd\\Pads\\none","Misc\\LIGHT1"}};
            load_world_material_requests(requests,*materials,context,files,observer);
            require(observer.uploads==before+(scene==0?1u:2u),"raw-name duplicate triggered wrong upload count");
            const auto snapshot=gpu.bindings_snapshot();
            for(const auto& material:materials->names.materials){
                draw_material(renderer,*snapshot,material.primary_texture,observer.output/("draw-"+std::to_string(frames++)+".bmp"));
                if(split)draw_material(renderer,*snapshot,material.alternate_texture,observer.output/("draw-"+std::to_string(frames++)+".bmp"));
            }
            const auto byte_cursor=storage.cursor;reset_world_material_loading(*materials,observer);
            require(materials->names.physical_upload_count==0&&gpu.resource_entries()==0&&storage.cursor==byte_cursor,"42C480 reset transparency with physical table");
            healthy(renderer);
        }
        const auto unchanged=storage.cursor;observer.begin_pending_record(0);
        try{(void)observer.upload({3,3,direct});}catch(const FormatError&){++guards;}
        require(storage.cursor==unchanged&&gpu.resource_entries()==0,"failed upload published source success");
        try{(void)gpu.bindings_snapshot();}catch(const FormatError&){++guards;}
        observer.release_all();gpu.reset_transparency_430690();storage.cursor=255;
        try{(void)gpu.current_transparency();}catch(const FormatError&){++guards;}
        gpu.reset_transparency_430690();observer.begin_pending_record(0);
        renderer.inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);
        try{(void)observer.generate(16);}catch(const std::runtime_error&){++guards;}
        require(renderer.stats().pending_polygon_uploads==(split?2u:1u)&&storage.cursor==0u&&gpu.resource_entries()==0,"generated wait failure dropped owners or committed material");
        healthy(renderer);observer.release_all();observer.begin_pending_record(0);observer.generate(16);healthy(renderer);
        uploads+=observer.uploads;generated+=observer.generated;pairs+=observer.pairs;compared_bytes+=observer.compared_bytes;
    }
    require(guards==10,"material expected guards were not all exercised");healthy(renderer);
    std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" uploads="<<uploads<<" generated="<<generated<<" pairs="<<pairs
        <<" compared_pixel_bytes="<<compared_bytes<<" rendered_material_tokens="<<frames<<" guards="<<guards<<" debug_warnings_errors=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
