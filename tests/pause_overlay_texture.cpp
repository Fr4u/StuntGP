#include "sgn/font_material_d3d12.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "material_texture_replacement_gpu_helpers.hpp"

namespace {
using AllMaterialWords=std::array<std::array<std::uint32_t,7>,255>;
AllMaterialWords material_words(const PhysicalMaterialStorage& owner){
    AllMaterialWords words{};
    for(std::size_t i=0;i<words.size();++i)for(unsigned lane=0;lane<7;++lane)
        words[i][lane]=owner.source_word(i,lane*4);
    return words;
}
struct PublicationProbe final:TextureReplacementDevice{
    RaceOverlayTextures* owner{};std::uint16_t width=256;unsigned uploads{};bool fail{};std::uint32_t observed{};
    std::uint16_t destination_width(std::uint32_t)const override{return width;}
    void upload_to_destination(std::uint32_t,const Image&)override{
        ++uploads;observed=owner->selected_overlay_texture();
        if(fail)throw FormatError("pause overlay injected device failure");
    }
};
}
int main(int argc,char** argv)try{
    require(argc==5,"expected adapter retail-root source-corpus fresh-output");
    const bool warp=std::string_view(argv[1])=="--warp";
    require(warp||std::string_view(argv[1])=="--hardware","pause overlay adapter");
    const auto output=std::filesystem::absolute(argv[4]);
    require(!std::filesystem::exists(output),"pause overlay output must be fresh");
    std::filesystem::create_directories(output);
    AssetStore assets(argv[2]);RetailMaterialFileSystem files(assets,argv[2]);
    unsigned guards=0,replacements=0,presents=0,font_uploads=0;std::uint64_t compared_words=0;
    auto rejected=[&](auto&& operation){bool caught=false;try{operation();}catch(const FormatError&){caught=true;}require(caught,"pause overlay expected rejection absent");++guards;};
    auto probe_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[2],0.0f);
    PublicationProbe probe;RaceOverlayTextures publication(assets,probe,probe_platform);probe.owner=&publication;
    const auto box=assets.texture("Overlays\\BoxBack");
    require(publication.source_image(26).rgba==box.rgba&&box.width==256&&box.height==256,"source26 is not actual BoxBack");
    publication.replace_source_texture(15,26);require(probe.observed==26&&probe.uploads==1,"source26 publication boundary");
    publication.replace_source_texture(15,26);require(probe.uploads==2,"same26 source call suppressed");
    probe.width=255;publication.replace_source_texture(15,16);require(probe.uploads==2&&publication.selected_overlay_texture()==26,"width rejection changed selection");++guards;
    probe.width=256;publication.replace_source_texture(14,16);require(publication.selected_overlay_texture()==26,"non15 changed selected overlay");++guards;
    rejected([&]{publication.replace_source_texture(15,30);});
    rejected([&]{publication.replace_source_texture(15,15);});
    publication.replace_source_texture(15,16);probe.fail=true;
    rejected([&]{publication.replace_source_texture(15,26);});
    require(publication.selected_overlay_texture()==26&&probe.observed==26,"failure rolled back source26 publication");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SGN-Pause-Overlay-Probe";
    require(RegisterClassW(&wc)!=0,"pause overlay window class");
    Window window{CreateWindowExW(0,wc.lpszClassName,L"StuntGP NEW pause overlay",WS_OVERLAPPEDWINDOW,0,0,384,256,nullptr,nullptr,wc.hInstance,nullptr)};
    require(window.handle!=nullptr,"pause overlay hidden window");
    Renderer renderer(window.handle,{true,warp,false,false,true});renderer.resize(384,256);
    for(bool split:{false,true}){
        const auto folder=output/(split?"split":"alias");std::filesystem::create_directories(folder);
        MaterialTransparencyStorage transparency;MaterialD3D12 gpu(renderer,transparency,split,split?1000:1);gpu.reset_transparency_430690();
        const auto roots=source_graphics_roots_from_pe();
        auto world=std::make_unique<WorldMaterialLoadingState>();WorldMaterialLoadingContext context;
        context.source_graphics_roots=&roots;context.maximum_dimension=256;
        load_world_materials({},*world,context,files,gpu);
        require(gpu.resource_entries()==16&&world->names.materials.size()==16,"actual normal prelude required");
        world->published_material_count=16; //Explicit caller publication, not a loader side effect.
        auto& owner=world->names.materials;MaterialTextureReplacement early(gpu,owner);auto overlay_platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},argv[2],0.0f);RaceOverlayTextures overlays(assets,early,overlay_platform);
        const auto initial=gpu.bindings_snapshot();const auto primary=gpu.resource_primary_token(15),alternate=owner[15].alternate_texture;
        const auto initial_pixels=read_words(texture(*initial,primary));const auto untouched=read_words(texture(*initial,gpu.resource_primary_token(0)));
        auto previous=initial;auto previous_pixels=initial_pixels;unsigned sequence=0;
        auto replace=[&](RaceOverlayTextures& bank,unsigned id,const char* phase){
            const auto before=material_words(owner);const auto next=owner.next_index();const auto epoch=gpu.resource_epoch();
            const auto count=gpu.resource_entries();const auto saved_transparency=transparency;
            bank.replace_source_texture(15,id);++replacements;
            const auto current=gpu.bindings_snapshot();const auto actual=read_words(texture(*current,primary));
            const auto expected=read_file(std::filesystem::path(argv[3])/("overlay-"+std::to_string(id)+".u16"));
            require(expected.size()==actual.size()*2&&std::memcmp(expected.data(),actual.data(),expected.size())==0,"pause overlay GPU differs from original source pixels");
            compared_words+=actual.size();
            require(bank.selected_overlay_texture()==id&&gpu.resource_epoch()==epoch+1&&gpu.resource_entries()==count,"source26 publication/count/epoch");
            for(std::size_t row=0;row<before.size();++row)for(unsigned lane=0;lane<7;++lane)
                require(owner.source_word(row,lane*4)==(row==next&&lane<2?primary:before[row][lane]),"replacement wrote outside next-free primary pair");
            require(world->published_material_count==16&&world->names.physical_upload_count==16&&transparency.cursor==saved_transparency.cursor&&transparency.bytes==saved_transparency.bytes,"replacement changed ordinary count/transparency");
            require(read_words(texture(*previous,primary))==previous_pixels,"source26 replaced retained image");compared_words+=previous_pixels.size();
            require(texture(*current,primary).resource.Get()!=texture(*previous,primary).resource.Get(),"source26 reused old physical storage");
            require(read_words(texture(*current,alternate))==(split?initial_pixels:actual),"source26 alternate pixels changed");
            require(texture(*current,alternate).resource.Get()==(split?texture(*initial,alternate).resource.Get():texture(*current,primary).resource.Get()),"source26 alternate identity changed");
            const auto prefix=std::string(phase)+"-"+std::to_string(sequence++)+"-"+std::to_string(id);
            write_words(folder/(prefix+".u16"),actual);
            draw_material(renderer,*current,primary,folder/(prefix+"-current.bmp"));++presents;
            draw_material(renderer,*previous,primary,folder/(prefix+"-retained.bmp"));++presents;
            previous=current;previous_pixels=actual;
        };
        for(unsigned id:{16u,26u,26u,25u,26u})replace(overlays,id,"ordinary");
        require(world->format==1,"font device flag must come from actual loader");
        RetailFontResources fonts;
        {FontMaterialD3D12Target target(gpu,owner);
         initialize_retail_font_resources(fonts,files,{world->width,world->height,world->tga_width,world->tga_height},
            font_reload_views(world->reload,world->names.physical_upload_count),roots.normal,1280,world->format,target);}
        require(fonts.count==2&&gpu.resource_entries()==20&&owner.size()==20,"actual font append required");font_uploads+=4;
        const auto with_fonts=gpu.bindings_snapshot();
        std::array<std::vector<std::uint16_t>,4> font_pixels;
        for(unsigned i=0;i<4;++i)font_pixels[i]=read_words(texture(*with_fonts,gpu.resource_primary_token(16+i)));
        replace(overlays,26,"early-binding-after-fonts");
        MaterialTextureReplacement late(gpu,owner);RaceOverlayTextures late_overlays(assets,late,overlay_platform);
        replace(late_overlays,26,"late-binding-after-fonts");
        const auto before_reject=gpu.resource_epoch();const auto saved=material_words(owner);
        owner.set_dimension(15,255);late_overlays.replace_source_texture(15,16);owner.set_dimension(15,256);
        require(gpu.resource_epoch()==before_reject&&material_words(owner)==saved&&late_overlays.selected_overlay_texture()==26,"actual width rejection changed owner");++guards;
        rejected([&]{late_overlays.replace_source_texture(15,30);});
        const auto final=gpu.bindings_snapshot();
        for(unsigned i=0;i<4;++i){require(read_words(texture(*final,gpu.resource_primary_token(16+i)))==font_pixels[i],"source26 changed font resource");compared_words+=font_pixels[i].size();}
        require(read_words(texture(*final,gpu.resource_primary_token(0)))==untouched,"source26 changed unrelated material");
        require(read_words(texture(*initial,primary))==initial_pixels,"source26 destroyed original snapshot");
        healthy(renderer);
    }
    require(replacements==14&&presents==28&&font_uploads==8&&guards==9,"pause overlay executed matrix differs");
    healthy(renderer);
    std::cout<<"passed=1 adapter="<<(warp?"WARP":"hardware")<<" replacements="<<replacements<<" presentations="<<presents
        <<" font_uploads="<<font_uploads<<" exact_u16_words="<<compared_words<<" guards="<<guards<<" debug_warnings_errors=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
