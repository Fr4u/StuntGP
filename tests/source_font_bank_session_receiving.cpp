// AUTHORED actual Session composition receiver. Main compiles and executes.
// Explicit bank init and PreviewHostEntry are TEST prerequisites, not full cold.
#include "source_font_session_gpu_helpers.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/race_session_menu.hpp"
#include "sgn/race_session_pause_restart.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/polygon_device_context.hpp"
#include <set>
#include <sstream>

namespace {
using Words=std::array<std::array<std::uint32_t,7>,255>;
Words snapshot(const PhysicalMaterialStorage& storage){
    Words out{};for(std::size_t row=0;row<out.size();++row)for(unsigned lane=0;lane<7;++lane)
        out[row][lane]=storage.source_word(row,lane*4);return out;
}
std::uint32_t u32(std::span<const std::uint8_t> b,std::size_t at){
    require(at<=b.size()&&b.size()-at>=4,"source font snapshot word bound");
    std::uint32_t value{};std::memcpy(&value,b.data()+at,4);return value;
}
void equal_bytes(std::span<const std::uint8_t> a,std::span<const std::uint8_t> b,const char* label){
    require(a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin()),label);
}
void write_bytes(const std::filesystem::path& path,std::span<const std::uint8_t> bytes){
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    require(bool(out),"cannot write Session font receipt");
}
template<class F>void rejects(F&& action,const char* reason){
    try{action();}catch(const FormatError& e){require(std::string_view(e.what())==reason,"unexpected Session rejection");return;}
    throw FormatError("Session precondition unexpectedly accepted");
}
void expired(const SourceAllocationBank& bank,const SourceAllocationRef& ref){
    bool refused=false;try{(void)bank.bytes(ref);}catch(const SourceAllocationError&){refused=true;}
    require(refused,"expired font allocation remains readable");
}
RaceSessionLaunch launch(HWND window){
    RaceSessionLaunch l;l.mode=SessionMode::normal_race;l.width=1279;l.height=720;
    l.master_gain=0;l.sound_percent=0;l.music_percent=0;l.window=reinterpret_cast<std::uint64_t>(window);return l;
}
WorldMaterialLoadingContext context_for(const RaceSession& session,const SourceGraphicsRoots& roots){
    WorldMaterialLoadingContext c;c.source_graphics_roots=&roots;c.scene=session.state().scene_mode;
    const auto& prefix=session.state().level.track_texture;const auto end=std::find(prefix.begin(),prefix.end(),'\0');
    require(end!=prefix.end(),"actual Session track texture prefix is unterminated");c.track_prefix={prefix.begin(),end};
    c.quality=0;c.maximum_dimension=4096;return c; //same explicit receiving domain as actual Session material tests
}

struct Observer final:FontBankObserver {
    RaceSession& session;SessionWorldMaterials& materials;MaterialD3D12& backend;
    SourceAllocationBank& bank;RetailFontResources& fonts;const WorldMaterialLoadingState& loaded;
    std::filesystem::path source,output;std::ofstream log;
    unsigned events{},allocations{},reads{},decoded{},packed{},uploads{},released{};
    std::uint32_t ordinary{},base{},initial_name_cursor{};std::uint64_t epoch{};Words before{};
    std::array<SourceAllocationRef,2> outer{},nested{},atlas{},scratch{};
    std::array<std::vector<std::uint8_t>,2> original_files;
    std::array<std::uint32_t,2> cursor_end{};
    Observer(RaceSession& s,SessionWorldMaterials& m,MaterialD3D12& b,std::filesystem::path src,std::filesystem::path out):
        session(s),materials(m),backend(b),bank(s.shared_platform()->source_allocations()),fonts(s.state().ui.fonts),loaded(m.state()),
        source(std::move(src)),output(std::move(out)),log(output/"bank-events.csv",std::ios::binary){
        require(bool(log),"cannot create font bank observation log");
        log<<"sequence,font,kind,source_site,requested_bytes,live_allocations,ordinary_bytes,next_ordinal,aligned_address\n";
        // Independent expected bytes from the SAME actual AssetStore; this is observation input, not a second font owner.
        RetailMaterialFileSystem actual(s.assets(),s.shared_platform()->retail_root());
        for(unsigned f=0;f<2;++f){
            const MaterialResourcePath path{MaterialResourceRoot::graphics24,f==0?"\\Fonts\\I32.TGA":"\\Fonts\\I32_b025.TGA"};
            const auto size=actual.source_file_size(path);require(size>=18&&size<=16*1024*1024,"actual font file extent");
            original_files[f].resize(size);const auto read=actual.read_source_file(path,original_files[f]);
            require(read.copied_bytes==size&&read.source_size==size,"actual font expected file read");
        }
    }
    void observe(const FontBankEvent& e,const SourceAllocationBank& observed) override{
        require(&observed==&bank&&&bank==&session.shared_platform()->source_allocations(),"Session substituted allocation bank");
        require(&fonts==&session.state().ui.fonts&&&loaded==&materials.state(),"Session substituted font/material owner");
        require(fonts.current>=1&&fonts.current<=2,"font producer selection outside Session owner");
        const auto f=fonts.current-1;require(fonts.count==f,"source font count published before completion");
        constexpr std::array<std::uint32_t,12> sites{0x428e75,0x4623f9,0x462408,0x46241a,0x462425,0x428e91,
            0x428c58,0x4327e0,0x43280d,0x432832,0x428eb7,0x428ebe};
        require(events<24&&f==events/12&&e.source_site==sites[events%12],"Session font source event order");
        require(e.category==0,"font category differs from source ordinary allocation");
        if(events==0){
            ordinary=loaded.names.physical_upload_count;base=loaded.names.materials.size();epoch=backend.resource_epoch();
            require(ordinary>16&&base==ordinary&&backend.resource_entries()==base,"actual Session world material loading missing");
            require(loaded.format==1&&loaded.names.logical_names.size()>16,"real Session material format/registry missing");
            require(loaded.reload.font_cursor==0,"Session font reload precondition");
            initial_name_cursor=loaded.reload.name_cursor;before=snapshot(loaded.names.materials);
        }
        require(loaded.names.physical_upload_count==ordinary&&loaded.published_material_count==0,
            "font advanced ordinary material count or prematurely published42E772");
        require(backend.resource_epoch()==epoch,"font append replaced ordinary resource bank");
        std::uintptr_t address=0;
        if(e.kind!=FontBankEventKind::release){const auto payload=bank.bytes(e.allocation);address=reinterpret_cast<std::uintptr_t>(payload.data());}
        log<<++events<<','<<f<<','<<static_cast<unsigned>(e.kind)<<','<<e.source_site<<','<<e.requested_bytes<<','
           <<bank.live_count()<<','<<bank.source_words().ordinary_bytes<<','<<bank.source_words().next_ordinal<<','<<address<<'\n';log.flush();
        require(bool(log),"cannot flush font bank observation");
        if(e.kind==FontBankEventKind::allocation){
            const auto bytes=bank.bytes(e.allocation);require(bytes.size()==e.requested_bytes&&address%32==0,"actual bank size/alignment");
            require(std::all_of(bytes.begin(),bytes.end(),[](auto byte){return byte==0;}),"new font bank payload not zero-filled");
            const auto rows=bank.source_records();const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.aligned==address;});
            require(row!=rows.end()&&row->raw!=0&&row->category==0&&row->requested_bytes==bytes.size()&&
                row->reserved_bytes==bytes.size()+0x24,"font payload has no actual bank row");
            if(e.source_site==0x428e75)outer[f]=e.allocation;
            else if(e.source_site==0x4623f9)nested[f]=e.allocation;
            else if(e.source_site==0x428e91)atlas[f]=e.allocation;
            else if(e.source_site==0x428c58){scratch[f]=e.allocation;require(bytes.size()==1024,"font scratch bytes");}
            else throw FormatError("unknown font allocation site");++allocations;
        }else if(e.kind==FontBankEventKind::read){
            require(e.allocation==nested[f]&&loaded.source_tga_decode.file==nested[f],"actual read/decode file owner differs");
            const auto raw=bank.bytes(nested[f]);equal_bytes(raw,original_files[f],"Session font read differs from actual AssetStore");
            const auto width=std::uint32_t(raw[12])|(std::uint32_t(raw[13])<<8),height=std::uint32_t(raw[14])|(std::uint32_t(raw[15])<<8);
            const auto end=18u+raw[0]+std::uint64_t(width)*height*3;require(end<=raw.size(),"source decoder input end outside payload");
            cursor_end[f]=static_cast<std::uint32_t>(end);++reads;
        }else if(e.kind==FontBankEventKind::decoded){
            require(e.allocation==outer[f]&&loaded.source_tga_decode.pixel_cursor_allocation==nested[f],"decoded output has another owner");
            const auto raw=bank.bytes(nested[f]),rgb=bank.bytes(outer[f]);
            require(loaded.source_tga_decode.pixel_depth==24&&loaded.source_tga_decode.pixel_cursor_offset==cursor_end[f],"actual decoder globals/end cursor");
            require(loaded.width==loaded.tga_width&&loaded.height==loaded.tga_height&&rgb.size()==raw.size(),"actual source image globals/outer extent");
            const auto extent=std::size_t(loaded.width)*loaded.height*3;require(extent<=rgb.size(),"decoded RGB extent");
            require(std::all_of(rgb.begin()+static_cast<std::ptrdiff_t>(extent),rgb.end(),[](auto b){return b==0;}),"decoded RGB changed zero file tail");
            ++decoded;
        }else if(e.kind==FontBankEventKind::packed){
            require(e.allocation==atlas[f]&&fonts.source_scratch_allocation(f,bank)==scratch[f],"canonical atlas/scratch owner differs");
            const auto expected=read_file(source/("group3-step0-font"+std::to_string(f)+"-rgb.bin"));
            equal_bytes(bank.bytes(atlas[f]),expected,"actual Session packed atlas differs from source capture");++packed;
        }else if(e.kind==FontBankEventKind::upload){
            require(e.allocation==atlas[f]&&backend.resource_entries()==base+uploads&&loaded.names.materials.size()==base+uploads,
                "Session font upload source allocation/resource/physical cursor");
            const auto payload=bank.bytes(atlas[f]);const auto expected=read_file(source/("group3-step0-atlas"+std::to_string(uploads)+".u16"));
            equal_bytes(payload.first(256*256*2),expected,"Session canonical font upload words differ from source capture");
            write_bytes(output/("source-atlas"+std::to_string(uploads)+".u16"),payload.first(256*256*2));
            if(uploads%2==1)require(fonts.source_word(f,0x20)==base+uploads-1,"primary font index was not published before alternate");
            ++uploads;
        }else if(e.kind==FontBankEventKind::release){
            expired(bank,e.allocation);++released;
        }else throw FormatError("unhandled font bank event");
    }
    void completed(){
        require(events==24&&allocations==8&&reads==2&&decoded==2&&packed==2&&uploads==4&&released==6,"actual Session font event coverage");
        require(fonts.count==2&&fonts.current==1&&fonts.scratch_allocations()==2,"font Session completion owners");
        require(bank.live_count()==2&&bank.source_words().ordinary_bytes==2120&&bank.source_words().persistent_bytes==0&&
            bank.source_words().next_ordinal==8,"Session canonical bank final accounting");
        require(loaded.names.materials.size()==base+4&&backend.resource_entries()==base+4&&
            loaded.names.physical_upload_count==ordinary,"Session font final physical/ordinary counts");
        const auto expected=read_file(source/"group3-step0-after.bin");require(expected.size()==30247,"source font snapshot schema");
        std::set<const std::uint32_t*> owners;
        for(unsigned f=0;f<2;++f){
            require(fonts.source_word(f,0x20)==base+f*2&&fonts.source_word(f,0x24)==base+f*2+1,"actual font material indices");
            require(fonts.source_scratch_allocation(f,bank)==scratch[f],"Session scratch ref differs");
            const auto bytes=bank.bytes(scratch[f]);require(bytes.size()==1024&&std::all_of(bytes.begin(),bytes.end(),[](auto v){return v==0;}),"retained scratch actual bytes");
            expired(bank,outer[f]);expired(bank,nested[f]);expired(bank,atlas[f]);
            for(unsigned at=0;at<retail_font_record_bytes;at+=4){
                require(owners.insert(&fonts.source_word(f,at)).second,"font word ownership aliases incorrectly");
                if(at!=0x20&&at!=0x24)require(fonts.source_word(f,at)==u32(expected,f*retail_font_record_bytes+at),"Session font metrics/record differs from source capture");
            }
            const auto metrics=fonts.metrics();require(&metrics.records[f*161]==&fonts.source_word(f,8),"Session metric width duplicated");
            for(unsigned glyph=0;glyph<160;++glyph)require(&metrics.records[f*161+glyph+1]==&fonts.source_word(f,0x28+44*glyph),"Session metric advance duplicated");
        }
        equal_bytes(fonts.index,std::span(expected).subspan(2*retail_font_record_bytes,160),"Session source font index differs");
        equal_bytes(fonts.glyph_map,std::span(expected).subspan(2*retail_font_record_bytes+160,256),"Session glyph map differs");
        for(std::size_t row=0;row<before.size();++row)for(unsigned lane=0;lane<7;++lane){
            const auto wanted=row>=base&&row<base+4&&lane==0?backend.resource_primary_token(static_cast<std::uint32_t>(row)):before[row][lane];
            require(loaded.names.materials.source_word(row,lane*4)==wanted,"font altered nonprimary or retained material word");
        }
        require(loaded.reload.font_cursor==2&&loaded.reload.font_records[4]==0,"Session reload count/terminal");
        auto cursor=initial_name_cursor;
        for(unsigned f=0;f<2;++f){
            const auto path=source_graphics_roots_from_pe().normal+(f==0?"\\Fonts\\I32.TGA":"\\Fonts\\I32_b025.TGA");
            require(loaded.reload.font_records[f*2]==cursor+1&&loaded.reload.font_records[f*2+1]==ordinary,"Session shared reload offset/index");
            require(cursor+path.size()<loaded.reload.name_arena.size(),"Session reload name extent");
            require(std::equal(path.begin(),path.end(),loaded.reload.name_arena.begin()+cursor)&&loaded.reload.name_arena[cursor+path.size()]==0,"Session reload name owner");
            cursor+=static_cast<std::uint32_t>(path.size()+1);
        }
        require(loaded.reload.name_cursor==cursor,"Session font shared reload cursor");
        expired(bank,loaded.source_tga_decode.file);expired(bank,loaded.source_tga_decode.pixel_cursor_allocation);
    }
};

unsigned consumer(RaceSession& session,SessionWorldMaterials& owner,MaterialD3D12& backend){
    auto& s=session.state();auto& physical=owner.replacement_storage(session);
    MaterialTextureReplacement replacement(backend,physical);RaceOverlayTextures overlays(session.assets(),replacement,session.shared_platform());
    SessionPauseRestartSource restart(session);PolygonSubmissionState scratch;
    PolygonDeviceContext device{device_seed(0)};
    SessionMenuInvocation menu(session,owner,scratch,overlays,restart,device);
    unsigned draws=0;
    for(unsigned font=0;font<2;++font)for(unsigned alternate=0;alternate<2;++alternate){
        select_retail_font_428f70(s.ui.fonts,font);
        auto view=menu.view().drawing.borrow_text();
        require(&view.fonts==&s.ui.fonts&&&view.selected_material==&s.ui.selected_material&&
            view.materials.data()==physical.active_materials().data()&&&view.viewport==&s.viewport,
            "actual Session menu consumer borrowed a duplicate owner");
        const auto material=s.ui.fonts.source_word(font,alternate?0x24:0x20);
        const auto token=physical[material].primary_texture;
        // Explicit TEST draw input. Actual source begin/glyph/end use actual Session metrics and bank.
        device.begin_capture(123+draws,backend.resource_epoch(),{{256,65536},512});require(device.begin_scene()==0,"font consumer BeginScene");
        const std::uint32_t flags=alternate?0x10u:0u;
        begin_text_432300(view,font,flags,device);
        const auto glyph=s.ui.fonts.glyph_map[static_cast<unsigned char>('A')];
        const auto advance=submit_text_glyph_4323f0(view,font,'A',{100,100,0.5f,0x00ffffff,1.0f,flags},device);
        require(advance==double(s.ui.fonts.source_word(font,0x28+44*glyph))+1.0,"consumer did not use actual glyph width");
        end_text_432380(view,device);require(device.end_scene()==0,"font consumer EndScene");
        const auto frame=device.seal_capture();require(frame&&frame->final.polygon.texture[0]==token&&s.ui.selected_material==material,"consumer atlas index/token binding differs");
        unsigned captured=0;
        for(const auto& command:frame->polygon_commands->commands)if(const auto* draw=std::get_if<CapturedPolygonDraw>(&command)){
            require(draw->vertices.size()==4&&draw->type==6,"source glyph did not produce actual quad");
            const auto at=0x20+44*glyph;
            require(draw->vertices[0].words[6]==s.ui.fonts.source_word(font,at+28)&&
                draw->vertices[0].words[7]==s.ui.fonts.source_word(font,at+32)&&
                draw->vertices[2].words[6]==s.ui.fonts.source_word(font,at+36)&&
                draw->vertices[2].words[7]==s.ui.fonts.source_word(font,at+40),"consumer UVs differ from canonical font record");++captured;
        }
        require(captured==1,"font consumer expected one actual glyph command");++draws;
    }
    return draws; //CPU receiving, not a rendered full menu image.
}
}

int main(int argc,char** argv)try{
    require(argc==6,"expected --hardware|--warp --alias|--split retail-root font-source fresh-output");
    const bool warp=std::string_view(argv[1])=="--warp",split=std::string_view(argv[2])=="--split";
    require(warp||std::string_view(argv[1])=="--hardware","adapter selection");
    require(split||std::string_view(argv[2])=="--alias","ordinary texture policy selection");
    const auto output=std::filesystem::absolute(argv[5]);require(!std::filesystem::exists(output),"Session font receipt must be fresh");
    std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Source-Font-Session";
    require(RegisterClassW(&wc)!=0,"font Session window class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW Session font receiving",WS_OVERLAPPEDWINDOW,0,0,1279,720,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr&&!IsWindowVisible(window.handle),"real hidden Session HWND");
    Renderer renderer(window.handle,{true,warp,false,false,false,1});renderer.resize(1279,720);
    MaterialTransparencyStorage transparency;MaterialD3D12 backend(renderer,transparency,split,1);backend.reset_transparency_430690();
    std::shared_ptr<RaceSessionPlatform> retained_platform;std::weak_ptr<RaceSessionPlatform> weak_platform;
    std::array<SourceAllocationRef,2> retained_scratch;std::shared_ptr<const PolygonGpuTextures> retained_gpu;
    std::array<std::uint32_t,4> font_tokens{};std::array<std::vector<std::uint16_t>,4> words;
    unsigned draws=0;std::uint32_t ordinary=0;std::uint64_t compared_words=0;
    {
        RaceSession session(PreviewHostEntry{},argv[3],launch(window.handle));
        require(session.readiness()==RaceSessionReadiness::preview_ready,"actual preview prerequisites unavailable");
        retained_platform=session.shared_platform();weak_platform=retained_platform;
        require(&session.state()==&retained_platform->state()&&!session.shared_state().owner_before(retained_platform)&&
            !retained_platform.owner_before(session.shared_state()),"Session/world/platform control block differs");
        auto& bank=retained_platform->source_allocations();auto& fonts=session.state().ui.fonts;
        require(!bank.initialized()&&bank.live_count()==0&&fonts.scratch_allocations()==0,"preview unexpectedly initialized source bank or font scratch");
        SessionWorldMaterials owner(session);const auto roots=source_graphics_roots_from_pe();auto loading=context_for(session,roots);
        Observer observer(session,owner,backend,argv[4],output);
        auto no_roots=loading;no_roots.source_graphics_roots=nullptr;
        rejects([&]{owner.load_with_source_fonts(no_roots,backend,nullptr,&observer);},"Session source fonts require explicit source graphics roots");
        rejects([&]{owner.load_with_source_fonts(loading,backend,nullptr,&observer);},"font bank requires explicit prior source42EAB0");
        require(observer.events==0&&bank.live_count()==0&&backend.resource_entries()==0&&fonts.scratch_allocations()==0,"font preflight mutated production owners");
        bank.initialize_42eab0(); // TEST precondition only. Earlier material allocation history remains unmigrated.
        owner.load_with_source_fonts(loading,backend,nullptr,&observer); // Actual requested Session entry.
        observer.completed();ordinary=observer.ordinary;retained_scratch=observer.scratch;
        rejects([&]{(void)owner.validate_fonts_for(session);},"Session materials require completed loading and source42E772 publication");
        owner.publish_count_42e772();require(&owner.validate_fonts_for(session)==&fonts,"published Session font owner differs");
        owner.require_backend_for(session,backend);
        require(owner.state().published_material_count==ordinary+4&&transparency.cursor==ordinary,"physical publication or ordinary transparency cursor differs");
        retained_gpu=backend.bindings_snapshot();
        for(unsigned i=0;i<4;++i){
            font_tokens[i]=owner.state().names.materials[ordinary+i].primary_texture;
            const auto& actual=texture(*retained_gpu,font_tokens[i]);require(!actual.source_creation,"font invented ordinary source creation metadata");
            words[i]=read_words(actual);const auto expected=read_file(std::filesystem::path(argv[4])/("group3-step0-atlas"+std::to_string(i)+".u16"));
            equal_bytes({reinterpret_cast<const std::uint8_t*>(words[i].data()),words[i].size()*2},expected,"actual Session GPU font payload differs from source capture");
            write_words(output/("gpu-atlas"+std::to_string(i)+".u16"),words[i]);compared_words+=words[i].size();
        }
        draws=consumer(session,owner,backend);
        const auto rows_before=snapshot(owner.state().names.materials);const auto event_count=observer.events;
        rejects([&]{owner.load_with_source_fonts(loading,backend,nullptr,&observer);},"Session materials require fresh model headers; repeated cold loading is unsupported");
        require(observer.events==event_count&&snapshot(owner.state().names.materials)==rows_before&&bank.live_count()==2,"repeated Session cold load changed font/material owners");
    } // real material wrapper and Session destroyed; platform alias deliberately retains the sole bank/world.
    require(!weak_platform.expired()&&retained_platform->source_allocations().live_count()==2,"Session destructor freed platform font scratch");
    auto& bank=retained_platform->source_allocations();auto& fonts=retained_platform->state().ui.fonts;
    for(unsigned f=0;f<2;++f)require(fonts.source_scratch_allocation(f,bank)==retained_scratch[f]&&bank.bytes(retained_scratch[f]).size()==1024,"platform alias lost actual font allocation");
    bank.begin_42ea80(); // TEST-only allocator lifetime check after consumers retired; not production warm activation.
    require(bank.live_count()==0&&bank.source_words().ordinary_bytes==0,"actual source scope release did not retire scratch");
    for(const auto& ref:retained_scratch)expired(bank,ref);
    backend.release_all();
    for(unsigned i=0;i<4;++i){require(read_words(texture(*retained_gpu,font_tokens[i]))==words[i],"retained Session GPU font changed after CPU scope/backend release");compared_words+=words[i].size();}
    retained_platform.reset();require(weak_platform.expired(),"font weak refs unexpectedly retained platform owner");
    healthy(renderer);require(renderer.stats().samples==1,"receiving sample count");
    std::ofstream result(output/"result.json",std::ios::binary);
    result<<"{\"status\":\"PASS\",\"actual_session_entry\":true,\"preview_prerequisite\":true,\"explicit_bank_init_test_only\":true,"
        <<"\"earlier_material_allocations_migrated\":false,\"full_cold\":false,\"production_warm\":false,\"bank_owners\":1,"
        <<"\"bank_events\":24,\"allocations\":8,\"releases\":6,\"font_uploads\":4,\"consumer_glyph_commands\":"<<draws
        <<",\"ordinary_materials\":"<<ordinary<<",\"compared_gpu_words\":"<<compared_words
        <<",\"debug_warnings_errors\":0,\"full_menu_image_qualified\":false}\n";
    result.flush();require(bool(result),"font Session result write");
    std::cout<<"passed=1 actual_session_entry=1 single_bank=1 font_uploads=4 consumer_commands="<<draws
             <<" ordinary_materials="<<ordinary<<" gpu_words="<<compared_words<<" full_cold=0 production_warm=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"Session font receiving failed: "<<error.what()<<'\n';return 1;}
