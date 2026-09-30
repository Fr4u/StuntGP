#include "sgn/font_material_d3d12.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "material_texture_replacement_gpu_helpers.hpp"
#include <thread>
#include <utility>

namespace {
void draw_font_pixel(Renderer& renderer,const PolygonGpuTextures& bank,std::uint32_t token,
    std::span<const std::uint16_t> words,const std::filesystem::path& path){
    const auto found=std::find_if(words.begin(),words.end(),[](auto word){return (word&0x8000)!=0&&(word&0x7fff)>0x421;});
    require(found!=words.end(),"font atlas has no visible glyph pixel");const auto index=static_cast<std::size_t>(found-words.begin());
    const float u=(static_cast<float>(index%256)+0.5f)/256,v=(static_cast<float>(index/256)+0.5f)/256;
    CapturedPolygonDraw draw;draw.fvf=0x2c4;draw.flags=0x1c;draw.type=4;draw.viewport.full_width=384;draw.viewport.full_height=256;
    for(const auto [x,y]:std::array<std::array<float,2>,6>{{{16,16},{368,16},{368,240},{16,16},{368,240},{16,240}}})
        draw.vertices.push_back({{{std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),0x3f000000,0x3f800000,0xffffffff,0,std::bit_cast<std::uint32_t>(u),std::bit_cast<std::uint32_t>(v),0x7fc00001,0xdeadbeef}}});
    CapturedPolygonFrame frame;frame.source_tick=90;frame.resource_epoch=bank.epoch;frame.commands.push_back(std::move(draw));
    renderer.draw_polygons(frame,device_seed(token),std::make_shared<const PolygonGpuTextures>(bank));renderer.capture(path);const auto bytes=read_file(path);
    constexpr std::size_t pixel=54+(128*384+192)*4;
    for(unsigned channel=0;channel<3;++channel){const auto value=((*found)>>(10-5*channel))&31;
        const auto expected=static_cast<int>(std::lround(double(value)*255/31));require(std::abs(int(bytes[pixel+2-channel])-expected)<=1,"font draw sampled wrong atlas/glyph pixel");}
}
using MaterialWords=std::array<std::array<std::uint32_t,7>,255>;
MaterialWords snapshot_materials(const PhysicalMaterialStorage& storage){
    MaterialWords out{};for(std::size_t row=0;row<out.size();++row)for(unsigned lane=0;lane<7;++lane)out[row][lane]=storage.source_word(row,lane*4);return out;
}
struct Observer final:FontAtlasUploadTarget,FontAtlasObserver {
    FontMaterialD3D12Target& target;MaterialD3D12& gpu;Renderer& renderer;PhysicalMaterialStorage& materials;
    RetailFontResources& fonts;std::filesystem::path corpus,output;unsigned step{},uploads{},packed{},frames{};
    std::uint64_t compared_words{};std::shared_ptr<const PolygonGpuTextures> retained;std::uint32_t retained_token{};std::vector<std::uint16_t> retained_words;
    Observer(FontMaterialD3D12Target& t,MaterialD3D12& g,Renderer& r,PhysicalMaterialStorage& m,RetailFontResources& f,
        std::filesystem::path c,std::filesystem::path o):target(t),gpu(g),renderer(r),materials(m),fonts(f),corpus(std::move(c)),output(std::move(o)){}
    std::string prefix() const{return "group3-step"+std::to_string(step);}
    void packed_atlas(std::size_t font,std::span<const std::uint8_t> rgb) override{
        const auto expected=read_file(corpus/(prefix()+"-font"+std::to_string(font)+"-rgb.bin"));
        require(std::equal(rgb.begin(),rgb.end(),expected.begin(),expected.end()),"GPU font packed atlas differs from original428C00");++packed;
    }
    std::uint8_t& current_transparency() override{return target.current_transparency();}
    std::uint32_t upload_font_atlas(PreparedMaterialPixels pixels) override{
        const auto within=uploads%4;const auto source=read_file(corpus/(prefix()+"-atlas"+std::to_string(within)+".u16"));
        require(source.size()==pixels.words.size_bytes()&&std::memcmp(source.data(),pixels.words.data(),source.size())==0,"GPU font upload differs from original source pixels");
        require(fonts.current==within/2+1&&fonts.count==within/2,"GPU font publication order");
        const auto before=snapshot_materials(materials);const auto count=gpu.resource_entries();const auto bank_before=gpu.bindings_snapshot();
        const auto index=target.upload_font_atlas(pixels);const auto bank=gpu.bindings_snapshot();const auto token=gpu.resource_primary_token(count);
        require(index==16+uploads&&gpu.resource_entries()==count+1&&bank->entries.size()==bank_before->entries.size()+1,"font primary physical/resource count differs");
        require(bank->epoch==bank_before->epoch,"font append changed same-bank epoch");
        for(std::size_t row=0;row<PhysicalMaterialStorage::capacity;++row)for(unsigned lane=0;lane<7;++lane)
            require(materials.source_word(row,lane*4)==(row==index&&lane==0?token:before[row][lane]),"font append changed retained nonprimary material word");
        const auto& actual_texture=texture(*bank,token);require(!actual_texture.source_creation,"font invented ordinary transparency creation metadata");
        const auto actual=read_words(actual_texture);
        require(actual.size()*2==source.size()&&std::memcmp(actual.data(),source.data(),source.size())==0,"actual GPU font words differ from original4325E0");
        write_words(output/("atlas-"+std::to_string(uploads)+".u16"),actual);compared_words+=actual.size();
        if(retained){require(read_words(texture(*retained,retained_token))==retained_words,"font append changed old atlas pixels");compared_words+=retained_words.size();
            draw_font_pixel(renderer,*retained,retained_token,retained_words,output/("retained-"+std::to_string(uploads)+".bmp"));++frames;}
        draw_font_pixel(renderer,*bank,token,actual,output/("current-"+std::to_string(uploads)+".bmp"));++frames;
        retained=bank;retained_token=token;retained_words=actual;++uploads;return index;
    }
};
}
int main(int argc,char** argv){try{
    require(argc==5,"expected adapter retail-root font-source fresh-output");const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","font adapter argument");const auto output=std::filesystem::absolute(argv[4]);
    require(!std::filesystem::exists(output),"font output must be fresh");std::filesystem::create_directories(output);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Font-GPU-Probe";
    require(RegisterClassW(&wc)!=0,"font window class");Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW font qualification",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"font hidden window");Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(384,256);
    AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);unsigned guards=0,uploads=0,frames=0,post_font_replacements=0;std::uint64_t words=0;
    auto rejected=[&](auto&& op){bool caught=false;try{op();}catch(const std::exception&){caught=true;}require(caught,"font expected guard missing");++guards;};
    for(bool split:{false,true}){
        auto overlay_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[2],0.0f);
        const auto folder=output/(split?"split":"alias");std::filesystem::create_directories(folder);
        MaterialTransparencyStorage transparency;MaterialD3D12 gpu(renderer,transparency,split,split?1000:1);gpu.reset_transparency_430690();
        const auto source_roots=source_graphics_roots_from_pe();
        auto world=std::make_unique<WorldMaterialLoadingState>();WorldMaterialLoadingContext context;context.source_graphics_roots=&source_roots;context.maximum_dimension=256;
        load_world_materials({},*world,context,files,gpu);world->published_material_count=16;require(gpu.resource_entries()==16,"font actual normal prelude count");
        auto& material_words=world->names.materials;
        // Seed only unused retained slots in this SAME physical owner.
        for(std::size_t row=16;row<PhysicalMaterialStorage::capacity;++row)for(unsigned lane=0;lane<7;++lane)
            material_words.source_word(row,lane*4)=0x17171717u+static_cast<std::uint32_t>((row*7+lane)*13);
        MaterialTextureReplacement replace(gpu,material_words);RaceOverlayTextures overlays(assets,replace,overlay_platform);overlays.replace_source_texture(15,16);
        const auto& next=material_words.record(16);
        require(next.primary_texture!=0&&next.primary_texture==next.alternate_texture,"font requires actual overlay next-free alias");
        const auto old_span=material_words.active_materials();const auto old=gpu.bindings_snapshot();const auto old_token=material_words[15].primary_texture;const auto old_words=read_words(texture(*old,old_token));
        const auto retained_alternate=material_words.record(16).alternate_texture;std::vector<FontMaterialPrimarySlot> slots;slots.reserve(255);
        for(std::size_t row=0;row<PhysicalMaterialStorage::capacity;++row)slots.push_back({material_words.record(row).primary_texture});
        auto& cursor=material_words.active_cursor();FontMaterialD3D12Target target(gpu,material_words);RetailFontResources fonts;
        const auto source_before=read_file(std::filesystem::path(argv[3])/"group3-step0-before.bin");
        std::array<std::uint8_t,8160> names{};std::array<std::uint32_t,16> reload{};
        constexpr std::size_t names_offset=2*0x1ba0+160+256+17*4+255*28+255;
        require(source_before.size()==30247,"font source snapshot size");std::copy_n(source_before.begin()+names_offset,8160,names.begin());
        std::uint32_t name_cursor=529,reload_cursor=0;
        Observer observer(target,gpu,renderer,material_words,fonts,argv[3],folder);
        for(unsigned step=0;step<2;++step){observer.step=step;
            initialize_retail_font_resources(fonts,files,{world->width,world->height,world->tga_width,world->tga_height},
                {names,name_cursor,reload,reload_cursor,world->names.physical_upload_count},context.source_graphics_roots->normal,static_cast<std::int32_t>(1279+step),1,observer,&observer);
            const auto expected=read_file(std::filesystem::path(argv[3])/(observer.prefix()+"-after.bin"));
            for(unsigned font=0;font<2;++font)for(unsigned i=0;i<retail_font_record_words;++i){std::uint32_t value{};std::memcpy(&value,expected.data()+font*0x1ba0+i*4,4);require(fonts.source_word(font,i*4)==value,"GPU font source record differs");}
            require(std::equal(names.begin(),names.end(),expected.begin()+names_offset),"GPU font shared reload names differ");
            require(std::memcmp(reload.data(),expected.data()+names_offset+8160,sizeof(reload))==0,"GPU font reload records differ");
            require(cursor==20+step*4&&gpu.resource_entries()==cursor&&transparency.cursor==16u&&transparency.bytes[16]==1,"GPU font source cursor/transparency behavior differs");
            require(world->names.physical_upload_count==16&&world->published_material_count==16&&material_words.record(16).alternate_texture==retained_alternate,"font changed ordinary material count/publication/retained alternate");
        }
        require(observer.uploads==8&&observer.packed==4,"font full initializer GPU coverage");
        require(read_words(texture(*old,old_token))==old_words,"font changed retained prelude texture");words+=old_words.size();
        draw_material(renderer,*old,old_token,folder/"retained-world.bmp");++frames;
        require(old_span.size()==16&&material_words.active_materials().size()==24&&old_span.data()==material_words.active_materials().data(),"font span extent/address contract");
        world->published_material_count=material_words.size(); // Explicit42E772 publication AFTER fonts.
        // The ordinary count remains16 while the actual physical/resource extent is24.
        // Reuse the pre-font adapter, then construct a new one after fonts.
        for(unsigned receiving=0;receiving<2;++receiving){
            const auto retained=snapshot_materials(material_words);const auto epoch=gpu.resource_epoch();const auto at=cursor;
            if(receiving==0)overlays.replace_source_texture(15,16);
            else{MaterialTextureReplacement late(gpu,material_words);RaceOverlayTextures late_overlay(assets,late,overlay_platform);late_overlay.replace_source_texture(15,16);}
            require(gpu.resource_epoch()==epoch+1&&cursor==at&&world->names.physical_upload_count==16&&world->published_material_count==24,"post-font replacement count/publication changed");
            for(std::size_t row=0;row<PhysicalMaterialStorage::capacity;++row)for(unsigned lane=0;lane<7;++lane)
                require(material_words.source_word(row,lane*4)==(row==at&&lane<2?old_token:retained[row][lane]),"post-font replacement did not use current next-free slot");
            require(read_words(texture(*gpu.bindings_snapshot(),old_token))==old_words,"post-font replacement pixels differ from original overlay16");
            ++post_font_replacements;words+=old_words.size();
        }
        rejected([&]{(void)target.current_transparency();}); // Replacement expires the old font GPU binding.
        FontMaterialD3D12Target guard_target(gpu,material_words);
        const auto before=snapshot_materials(material_words);const auto before_cursor=cursor;const auto current_count=gpu.resource_entries();
        const std::vector<std::uint16_t> valid(256*256,0x8421);const PreparedMaterialPixels pixels{256,256,valid};
        rejected([&]{(void)guard_target.upload_font_atlas({256,255,valid});});
        bool thread_reject=false;std::thread worker([&]{try{(void)guard_target.current_transparency();}catch(const std::exception&){thread_reject=true;}});worker.join();require(thread_reject,"font cross-thread access accepted");++guards;
        rejected([&]{std::uint32_t at=0;std::array<FontMaterialPrimarySlot,2> duplicate{{{material_words.record(0).primary_texture},{material_words.record(0).primary_texture}}};FontMaterialD3D12Target bad(gpu,duplicate,at);});
        rejected([&]{std::uint32_t at=0;std::array<FontMaterialPrimarySlot,1> alias{{{at}}};FontMaterialD3D12Target bad(gpu,alias,at);});
        ++cursor;rejected([&]{(void)guard_target.current_transparency();});--cursor;
        require(snapshot_materials(material_words)==before&&cursor==before_cursor&&gpu.resource_entries()==current_count,"font guards changed source state");
        renderer.inject_failure_for_test(RenderFailurePoint::polygon_upload_wait);rejected([&]{(void)guard_target.upload_font_atlas(pixels);});
        require(snapshot_materials(material_words)==before&&cursor==before_cursor&&gpu.resource_entries()==current_count,"failed font upload published physical state");
        rejected([&]{(void)guard_target.current_transparency();});rejected([&]{(void)gpu.bindings_snapshot();});healthy(renderer);
        require(read_words(texture(*observer.retained,observer.retained_token))==observer.retained_words,"failed font upload invalidated old atlas");words+=observer.retained_words.size();
        gpu.release_all();rejected([&]{(void)guard_target.current_transparency();});
        require(read_words(texture(*observer.retained,observer.retained_token))==observer.retained_words,"font release invalidated retained GPU resource");words+=observer.retained_words.size();
        draw_font_pixel(renderer,*observer.retained,observer.retained_token,observer.retained_words,folder/"retained-after-release.bmp");++frames;
        uploads+=observer.uploads;frames+=observer.frames;words+=observer.compared_words;healthy(renderer);
    }
    {MaterialTransparencyStorage bytes;MaterialD3D12 idle(renderer,bytes,false,9000);idle.reset_transparency_430690();
     PhysicalMaterialStorage empty;empty.reset_loading_42c480();FontMaterialD3D12Target before_scene(idle,empty);
     const auto epoch=idle.resource_epoch();empty.reset_scene_42e510();rejected([&]{(void)before_scene.current_transparency();});
     require(idle.resource_epoch()==epoch&&idle.resource_entries()==0&&!empty.active(),"inactive reset ABA changed backend");
     empty.reset_loading_42c480();FontMaterialD3D12Target before_loading(idle,empty);empty.reset_loading_42c480();
     rejected([&]{(void)before_loading.current_transparency();});
     require(idle.resource_epoch()==epoch&&empty.active()&&empty.size()==0,"active reset ABA changed backend/cursor");}
    healthy(renderer);std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" font_uploads="<<uploads<<" rendered_frames="<<frames<<" source_and_retained_words="<<words<<" post_font_replacements="<<post_font_replacements<<" guards="<<guards<<" debug_warnings_errors=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
