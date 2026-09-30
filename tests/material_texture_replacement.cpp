#include "sgn/material_texture_replacement.hpp"
#include "material_texture_replacement_gpu_helpers.hpp"
#include <thread>

int main(int argc,char** argv){try{
    require(argc==5,"expected adapter retail-root source-corpus fresh-output");const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","invalid replacement test adapter");
    const auto output=std::filesystem::absolute(argv[4]);require(!std::filesystem::exists(output),"replacement output must be fresh");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Replacement-GPU-Probe";
    require(RegisterClassW(&wc)!=0,"replacement window class");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW texture replacement",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"replacement hidden window");Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(384,256);
    AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);unsigned guards=0,frames=0,uploads=0;std::uint64_t words_compared=0;
    auto rejected=[&](auto&& operation){bool caught=false;try{operation();}catch(const std::exception&){caught=true;}require(caught,"expected replacement guard absent");++guards;};
    for(const bool split:{false,true}){
        MaterialTransparencyStorage transparency;MaterialD3D12 gpu(renderer,transparency,split,split?1000:1);gpu.reset_transparency_430690();
        const auto source_roots=source_graphics_roots_from_pe();
        auto materials=std::make_unique<WorldMaterialLoadingState>();WorldMaterialLoadingContext context;context.source_graphics_roots=&source_roots;context.scene=0;context.maximum_dimension=256;
        load_world_materials({},*materials,context,files,gpu);require(gpu.resource_entries()==16&&materials->names.materials.size()==16,"actual normal prelude required");
        materials->published_material_count=16;auto& cursor=materials->names.materials.record(materials->names.materials.next_index());cursor.primary_texture=0x11223344;cursor.alternate_texture=0x55667788;
        MaterialTextureReplacement device(gpu,materials->names.materials);auto overlay_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[2],0.0f);RaceOverlayTextures overlays(assets,device,overlay_platform);
        const auto material=materials->names.materials[15];const auto primary=material.primary_texture,alternate=material.alternate_texture;
        const auto initial=gpu.bindings_snapshot();const auto initial_pixels=read_words(texture(*initial,primary));
        const auto untouched=read_words(texture(*initial,materials->names.materials[0].primary_texture));
        const auto saved_transparency=transparency;const auto initial_epoch=gpu.resource_epoch();
        auto previous=initial;auto previous_words=initial_pixels;
        for(std::uint32_t id=16;id<26;++id){
            overlays.replace_source_texture(15,id);++uploads;const auto bank=gpu.bindings_snapshot();
            const auto original=read_file(std::filesystem::path(argv[3])/("overlay-"+std::to_string(id)+".u16"));
            const auto actual=read_words(texture(*bank,primary));
            write_words(output/(std::to_string(split)+"-"+std::to_string(id)+".u16"),actual);
            require(original.size()==actual.size()*2&&std::memcmp(original.data(),actual.data(),original.size())==0,"replacement GPU words differ from original430D40");words_compared+=actual.size();
            require(read_words(texture(*previous,primary))==previous_words,"replacement changed pixels in retained old snapshot");words_compared+=previous_words.size();
            require(texture(*bank,primary).resource.Get()!=texture(*previous,primary).resource.Get(),"replacement reused old physical GPU storage");
            require(read_words(texture(*bank,alternate))==(split?initial_pixels:actual),"replacement wrongly changed alternate surface");
            require(texture(*bank,alternate).resource.Get()==(split?texture(*initial,alternate).resource.Get():texture(*bank,primary).resource.Get()),"alternate alias identity changed");
            require(bank->epoch==previous->epoch+1&&bank->entries.size()==initial->entries.size(),"replacement epoch/count changed incorrectly");
            require(cursor.primary_texture==primary&&cursor.alternate_texture==primary,"next-free cursor did not receive two primary words");
            require(materials->names.materials[15].primary_texture==primary&&materials->names.materials[15].alternate_texture==alternate&&materials->published_material_count==16&&materials->names.physical_upload_count==16,"replacement rewrote published material/count");
            const auto& creation=texture(*bank,primary).source_creation;const auto& old_creation=texture(*initial,primary).source_creation;
            require(creation&&old_creation&&creation->descriptor_flags==old_creation->descriptor_flags&&creation->transparency_byte==old_creation->transparency_byte&&creation->texture_stage==0,"replacement recreated source descriptor metadata");
            draw_material(renderer,*bank,primary,output/(std::to_string(split)+"-"+std::to_string(id)+"-current.bmp"));++frames;
            draw_material(renderer,*previous,primary,output/(std::to_string(split)+"-"+std::to_string(id)+"-retained.bmp"));++frames;
            if(split){draw_material(renderer,*bank,alternate,output/("alternate-"+std::to_string(id)+".bmp"));++frames;}
            previous=bank;previous_words=actual;
        }
        require(overlays.selected_overlay_texture()==25&&transparency.cursor==saved_transparency.cursor&&transparency.bytes==saved_transparency.bytes,"replacement changed selection/transparency incorrectly");
        require(read_words(texture(*gpu.bindings_snapshot(),materials->names.materials[0].primary_texture))==untouched,"replacement touched another slot");
        //An unchanged sourceID must still perform the source Load transition.
        overlays.replace_source_texture(15,25);++uploads;require(gpu.resource_epoch()==initial_epoch+11,"sameID replacement was skipped");
        const auto before_reject=gpu.bindings_snapshot();const auto epoch=before_reject->epoch;const auto old_cursor=cursor;
        materials->names.materials.set_dimension(15,255);overlays.replace_source_texture(15,16);materials->names.materials.set_dimension(15,256);
        require(gpu.resource_epoch()==epoch&&overlays.selected_overlay_texture()==25&&cursor.primary_texture==old_cursor.primary_texture&&cursor.alternate_texture==old_cursor.alternate_texture,"width mismatch changed state");++guards;
        rejected([&]{overlays.replace_source_texture(15,100);});rejected([&]{overlays.replace_source_texture(200,16);});
        auto malformed=overlays.source_image(16);malformed.rgba.pop_back();rejected([&]{device.upload_to_destination(15,malformed);});
        auto wrong_height=overlays.source_image(16);wrong_height.height=128;wrong_height.rgba.resize(256*128*4);rejected([&]{device.upload_to_destination(15,wrong_height);});
        bool other_thread_rejected=false;std::thread worker([&]{try{(void)device.destination_width(15);}catch(const std::exception&){other_thread_rejected=true;}});worker.join();require(other_thread_rejected,"cross-thread replacement accepted");++guards;
        //Material aliases must never choose the COM destination. A live alias
        //change is legal here: resource entry+4 remains the same primary.
        materials->names.materials[15].primary_texture=materials->names.materials[0].primary_texture;
        overlays.replace_source_texture(15,16);++uploads;
        require(cursor.primary_texture==primary&&read_words(texture(*gpu.bindings_snapshot(),primary))!=previous_words,"replacement followed material alias instead of resource entry");
        materials->names.materials[15]=material;
        //GPU wait failure occurs after submission. The saved bank stays valid,
        //but the failed producer cannot publish/retry before explicit release.
        const auto good=gpu.bindings_snapshot();const auto good_pixels=read_words(texture(*good,primary));const auto good_epoch=gpu.resource_epoch();cursor={0xabcdef01,0x10203040};
        renderer.inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);rejected([&]{overlays.replace_source_texture(15,24);});
        require(overlays.selected_overlay_texture()==24&&cursor.primary_texture==0xabcdef01&&cursor.alternate_texture==0x10203040&&gpu.resource_epoch()==good_epoch&&renderer.stats().pending_polygon_uploads==1,"failed replacement publication/order/lifetime differs");
        rejected([&]{(void)gpu.bindings_snapshot();});healthy(renderer);
        require(read_words(texture(*good,primary))==good_pixels,"GPU failure destroyed retained bank");
        gpu.release_all();rejected([&]{(void)device.destination_width(15);});
        gpu.reset_transparency_430690();load_world_materials({},*materials,context,files,gpu);rejected([&]{(void)device.destination_width(15);});
        require(read_words(texture(*initial,primary))==initial_pixels,"release/reload destroyed initial retained bank");healthy(renderer);
    }
    require(guards==20&&uploads==24&&frames==50,"replacement coverage counts differ");healthy(renderer);
    std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" replacements="<<uploads<<" rendered_frames="<<frames<<" source_and_retained_words="<<words_compared<<" guards="<<guards<<" debug_warnings_errors=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
