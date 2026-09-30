// Private future receiving target. No Session/application execution claim.
#include "sgn/polygon_submission_capture.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "material_texture_replacement_gpu_helpers.hpp"
#include <unordered_map>

namespace {
using PhysicalWords=std::array<std::array<std::uint32_t,7>,255>;
PhysicalWords physical(const PhysicalMaterialStorage& owner){
    PhysicalWords result{};for(std::size_t i=0;i<result.size();++i)for(unsigned j=0;j<7;++j)result[i][j]=owner.source_word(i,j*4);return result;
}
void panel(PolygonDeviceContext& device,bool right){
    std::array<PolygonSubmissionVertex,4> vertices{};unsigned i=0;const float left=right?208.0f:16.0f;
    for(const auto [x,y]:std::array<std::array<float,2>,4>{{{left,16},{left+160,16},{left+160,240},{left,240}}})
        vertices[i++].words={std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,0xffffffff,0,0x3f000000,0x3f000000,0,0};
    ViewportState viewport;viewport.full_width=384;viewport.full_height=256;
    require(device.draw(6,0x2c4,vertices,0x1c,viewport)==0,"rebind panel capture");
}
}
int main(int argc,char** argv)try{
    require(argc==6,"expected adapter retail-root original27-corpus fresh-output original16.u16");const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","rebind adapter");
    const auto output=std::filesystem::absolute(argv[4]);require(!std::filesystem::exists(output),"rebind output must be fresh");
    std::filesystem::create_directories(output);AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Material-Rebind";
    require(RegisterClassW(&wc)!=0,"rebind class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW material rebind receiving",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"rebind window");RenderOptions options{true,warp,false,false,true};options.requested_samples=1;
    const auto original16=read_file(argv[5]);require(original16.size()==131072,"original16 corpus extent");
    auto overlay_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[2],0.0f);
    unsigned passed=0;
    for(bool split:{false,true}){
        const auto folder=output/(split?"split":"alias");std::filesystem::create_directories(folder);
        auto renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);
        MaterialTransparencyStorage transparency;MaterialD3D12 backend(*renderer,transparency,split,1);backend.reset_transparency_430690();
        auto world=std::make_unique<WorldMaterialLoadingState>();const auto roots=source_graphics_roots_from_pe();WorldMaterialLoadingContext loading;
        loading.source_graphics_roots=&roots;loading.maximum_dimension=256;load_world_materials({},*world,loading,files,backend);world->published_material_count=16;
        require(world->names.materials.size()==16&&backend.resource_entries()==16,"rebind actual normal prelude");
        PolygonDeviceContext device(device_seed(backend.resource_primary_token(15)));
        PolygonSubmissionCapture capture(device,backend,world->names.materials);RaceOverlayTextures overlays(assets,capture,overlay_platform);
        // Accepted16..26 and new27 all target15, with one persistent registry.
        for(std::uint32_t source=16;source<=26;++source){overlays.replace_source_texture(15,source);
            require(overlays.selected_overlay_texture()==source&&overlays.completed_source(15)==source,"accepted15 mapping lost");}
        const auto last26=backend.resource_epoch();world->names.materials.set_dimension(15,255);overlays.replace_source_texture(15,27);
        require(backend.resource_epoch()==last26&&overlays.completed_source(15)==26u&&overlays.selected_overlay_texture()==26,"width rejection changed completed source");
        world->names.materials.set_dimension(15,256);
        constexpr PolygonSubmissionCaptureLimits limits{{{16,4096},32},4};
        capture.begin_source_frame(71,limits);ImmutablePolygonSubmission display;
        try{
            require(device.begin_scene()==0,"rebind scene begin");panel(device,false);overlays.replace_source_texture(15,27);panel(device,true);
            overlays.replace_source_texture(11,28);overlays.replace_source_texture(14,29);
            require(device.end_scene()==0,"rebind scene end");display=capture.seal_source_frame();
        }catch(...){capture.discard_failed_source_frame();throw;}
        require(display->segments.size()==4&&display->segments[2].device_frame->events.empty(),"rebind empty resource segment");
        auto original=display;auto live=backend.bindings_snapshot();const auto displayed_epoch=live->epoch;
        for(const auto [slot,source]:std::array<std::array<std::uint32_t,2>,3>{{{15,27},{11,28},{14,29}}}){
            const auto words=read_words(texture(*live,backend.resource_primary_token(slot)));
            const auto corpus=read_file(std::filesystem::path(argv[3])/("overlay-"+std::to_string(source)+".u16"));
            require(corpus.size()==256*256*2&&corpus.size()==words.size()*2&&std::memcmp(corpus.data(),words.data(),corpus.size())==0,
                "rebind source mapping differs from original430D40 corpus");
        }
        // P2 receiving amendment: the current bank deliberately advances AFTER
        // sealing the displayed frame. Recovery must preserve BOTH histories.
        const auto upload_before=renderer->stats().uploaded_texture_bytes;
        overlays.replace_source_texture(15,16); //Actual idle source replacement, same persistent registry.
        live=backend.bindings_snapshot();const auto epoch=backend.resource_epoch();
        require(epoch==displayed_epoch+1&&original->segments.back().textures->epoch==displayed_epoch&&
            renderer->stats().uploaded_texture_bytes==upload_before+131072,"late current bank did not advance independently");
        const auto primary15=backend.resource_primary_token(15);
        const auto late15=read_words(texture(*live,primary15));
        const auto retained15=read_words(texture(*original->segments.back().textures,primary15));
        require(late15!=retained15&&late15.size()*2==original16.size()&&
            std::memcmp(late15.data(),original16.data(),original16.size())==0,"late current15 is not distinct actual source16");
        require(overlays.selected_overlay_texture()==16&&overlays.completed_source(15)==16u&&
            overlays.completed_source(11)==28u&&overlays.completed_source(14)==29u,"late current source identities");
        const auto source_words=physical(world->names.materials);const auto cpu=device.snapshot();
        const auto generation=world->names.materials.generation();const auto cursor=world->names.materials.source_cursor();
        const auto transparency_before=transparency;
        renderer->draw_polygon_submission(*display);renderer->capture(folder/"before.bmp");healthy(*renderer);
        struct DistinctResource {std::uint32_t token;std::vector<std::uint16_t> gpu_words;};
        struct SavedSegment {
            ImmutablePolygonDeviceFrame frame;std::uint64_t epoch;
            std::vector<std::shared_ptr<const PolygonTexturePixels>> pixels;
            std::vector<std::uint32_t> tokens;
            std::vector<std::size_t> ordinals;
        };
        std::vector<DistinctResource> distinct;
        std::vector<std::size_t> current_ordinals;
        std::vector<SavedSegment> saved;
        std::ofstream identities(folder/"resource-identities.csv");require(bool(identities),"identity evidence output");
        identities<<"phase,bank,epoch,entry,token,resource_ordinal\n";
        {
            // This map dies BEFORE RemoveDevice. Only integer ordinals, actual
            // GPU readback WORDS and existing CPU command/pixel owners survive.
            std::unordered_map<ID3D12Resource*,std::size_t> old_identities;
            const auto observe=[&](const PolygonGpuTextures& bank,const std::string& label){
                std::vector<std::size_t> ordinals;ordinals.reserve(bank.entries.size());
                for(std::size_t i=0;i<bank.entries.size();++i){const auto& entry=bank.entries[i];
                    require(entry.resource&&entry.recovery,"missing actual resource or CPU retention");
                    const auto [found,inserted]=old_identities.emplace(entry.resource.Get(),old_identities.size());
                    const auto ordinal=found->second;
                    if(inserted){auto words=read_words(entry);
                        require(words==entry.recovery->words,"retention differs from actual distinct old GPU version");
                        write_words(folder/("before-version-"+std::to_string(ordinal)+".u16"),words);
                        distinct.push_back({entry.token,std::move(words)});
                    }else require(distinct.at(ordinal).token==entry.token,"old shared resource token identity differs");
                    ordinals.push_back(ordinal);
                    identities<<"before,"<<label<<','<<bank.epoch<<','<<i<<','<<entry.token<<','<<ordinal<<'\n';
                }
                // These physical slots must retain the configured alternate
                // identity, including after primary replacement versions.
                for(const auto slot:{11u,14u,15u}){
                    const auto primary=backend.resource_primary_token(slot),alternate=world->names.materials[slot].alternate_texture;
                    const auto p=old_identities.at(texture(bank,primary).resource.Get());
                    const auto q=old_identities.at(texture(bank,alternate).resource.Get());
                    require((p==q)==!split,"old primary/alternate resource alias policy differs");
                }
                return ordinals;
            };
            current_ordinals=observe(*live,"current");
            for(std::size_t i=0;i<original->segments.size();++i){const auto& segment=original->segments[i];
                SavedSegment item{segment.device_frame,segment.textures->epoch,{},{},observe(*segment.textures,"segment-"+std::to_string(i))};
                for(const auto& entry:segment.textures->entries){item.pixels.push_back(entry.recovery);item.tokens.push_back(entry.token);}
                saved.push_back(std::move(item));
            }
            require(distinct.size()==live->entries.size()+4,"distinct prior/current resource version count differs");
        }
        identities.flush();require(bool(identities),"old identity evidence flush");
        // Retain ONLY CPU evidence across real removal. External old GPU banks
        // would keep the removed D3D12 singleton device alive and block creation.
        original.reset();live.reset();renderer->remove_device_for_test(true);
        capture.suspend_renderer_for_recreation(display);
        require(!display&&!backend.bound_to_renderer(*renderer),"suspend retained GPU display binding");
        renderer.reset();
        renderer=std::make_unique<Renderer>(window.handle,options);renderer->resize(384,256);
        // Inject a real queued upload failure during recovery. The source
        // frame stays healthy; the CPU handoff survives and no GPU bank is published.
        renderer->inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);
        bool failed=false;try{capture.rebind_renderer(*renderer,display);}catch(const std::exception&){failed=true;}
        require(failed&&!display&&backend.resource_epoch()==epoch&&!backend.bound_to_renderer(*renderer)&&
            !capture.poisoned()&&!capture.capturing()&&renderer->stats().pending_polygon_uploads==1,"rebind published partial candidate");
        healthy(*renderer);capture.rebind_renderer(*renderer,display);
        require(backend.bound_to_renderer(*renderer)&&backend.resource_epoch()==epoch&&display,"rebind completion/epoch");
        require(physical(world->names.materials)==source_words&&world->names.materials.generation()==generation&&
            world->names.materials.source_cursor()==cursor&&same_polygon_snapshot(cpu,device.snapshot()),"rebind changed source material/device owner");
        require(transparency.bytes==transparency_before.bytes&&transparency.cursor==transparency_before.cursor&&
            overlays.selected_overlay_texture()==16&&overlays.completed_source(15)==16u&&overlays.completed_source(11)==28u&&overlays.completed_source(14)==29u,
            "rebind changed source selection/per-destination state");
        require(display->segments.size()==saved.size(),"rebind lost retained resource segments");
        for(std::size_t i=0;i<display->segments.size();++i){const auto& prior=saved[i];const auto& now=*display->segments[i].textures;
            require(display->segments[i].device_frame==prior.frame&&prior.epoch==now.epoch&&now.entries.size()==prior.pixels.size(),"rebind replaced source command frame");
            for(std::size_t j=0;j<now.entries.size();++j){const auto& next=now.entries[j];
                require(next.token==prior.tokens[j]&&next.recovery==prior.pixels[j],"rebind resource version identity");
            }
        }
        const auto rebuilt=backend.bindings_snapshot();
        require(rebuilt->epoch==epoch&&display->segments.back().textures->epoch==displayed_epoch&&epoch>displayed_epoch,
            "rebind substituted displayed final bank for newer current bank");
        {
            // NEW pointers only. The same traversal assigns observed ordinals;
            // exact ordinal equality proves the full shared/distinct relation.
            std::unordered_map<ID3D12Resource*,std::size_t> new_identities;
            const auto verify=[&](const PolygonGpuTextures& bank,const std::vector<std::size_t>& expected,const std::string& label){
                require(bank.entries.size()==expected.size(),"recreated identity graph extent");
                for(std::size_t i=0;i<bank.entries.size();++i){const auto& entry=bank.entries[i];
                    const auto [found,inserted]=new_identities.emplace(entry.resource.Get(),new_identities.size());
                    const auto observed=found->second;
                    require(entry.resource&&observed==expected[i]&&entry.token==distinct.at(expected[i]).token,
                        "rebind split a shared version or merged distinct versions/alternates");
                    if(inserted){const auto words=read_words(entry);
                        require(words==distinct.at(observed).gpu_words,"recreated distinct version differs from prior actual GPU readback");
                        write_words(folder/("after-version-"+std::to_string(observed)+".u16"),words);
                    }
                    identities<<"after,"<<label<<','<<bank.epoch<<','<<i<<','<<entry.token<<','<<observed<<'\n';
                }
            };
            verify(*rebuilt,current_ordinals,"current");
            for(std::size_t i=0;i<display->segments.size();++i)verify(*display->segments[i].textures,saved[i].ordinals,"segment-"+std::to_string(i));
            require(new_identities.size()==distinct.size(),"rebind distinct resource graph count differs");
        }
        identities.flush();require(bool(identities),"recreated identity evidence flush");
        require(read_words(texture(*rebuilt,primary15))==late15&&
            read_words(texture(*display->segments.back().textures,primary15))==retained15,
            "newer source16 and retained source27 images were conflated");
        renderer->draw_polygon_submission(*display);renderer->capture(folder/"after.bmp");healthy(*renderer);
        require(read_file(folder/"before.bmp")==read_file(folder/"after.bmp"),"rebind changed retained multi-epoch frame pixels");
        const auto token=backend.resource_primary_token(11);write_words(folder/"source28.u16",read_words(texture(*rebuilt,token)));
        draw_material(*renderer,*rebuilt,token,folder/"source28.bmp");healthy(*renderer);
        // Same coordinator and physical generation can begin the next source
        // frame; its initial bank is the recreated current backend bank.
        capture.begin_source_frame(72,limits);require(device.begin_scene()==0&&device.end_scene()==0,"post-rebind source Begin/End");
        const auto next=capture.seal_source_frame();require(same_polygon_bank(*next->segments[0].textures,*rebuilt),"post-rebind coordinator borrowed stale bank");
        require(next->segments[0].textures->epoch==epoch&&read_words(texture(*next->segments[0].textures,primary15))==late15,
            "next source frame did not start from newer current source16");
        ++passed;
    }
    require(passed==2,"rebind alias/split matrix");std::cout<<"passed=1 material_rebind_cases=2 actual_application_execution=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
