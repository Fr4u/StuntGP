#include "sgn/retail_font_resources.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

namespace {
using namespace sgn;
using namespace sgn::retail;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F&& action){
    bool caught=false;try{action();}catch(const std::exception&){caught=true;}
    check(caught,"expected explicit source/host stop");
}
bool live(const SourceAllocationBank& bank,const SourceAllocationRef& ref){
    try{(void)bank.bytes(ref);return true;}catch(const SourceAllocationError&){return false;}
}
struct Audit final:FontBankObserver {
    const SourceAllocationBank& expected;
    std::vector<FontBankEvent> events;
    SourceAllocationRef outer,input,atlas,scratch;
    unsigned abort_site{};
    explicit Audit(const SourceAllocationBank& bank):expected(bank){}
    void observe(const FontBankEvent& e,const SourceAllocationBank& bank) override{
        check(&bank==&expected&&e.category==0,"font observer got another bank/category");
        events.push_back(e);
        if(e.kind==FontBankEventKind::allocation){
            if(e.allocation){
                const auto bytes=bank.bytes(e.allocation);
                check(bytes.size()==e.requested_bytes,"actual allocation size");
                check(std::all_of(bytes.begin(),bytes.end(),[](auto x){return x==0;}),"actual allocation zero fill");
            }
            if(e.source_site==0x428e75)outer=e.allocation;
            else if(e.source_site==0x4623f9)input=e.allocation;
            else if(e.source_site==0x428e91)atlas=e.allocation;
            else if(e.source_site==0x428c58)scratch=e.allocation;
            else check(false,"unexpected bank allocation source site");
        }
        if(abort_site==e.source_site)throw std::runtime_error("explicit read-only observer abort");
    }
};
struct Files final:MaterialFileSource {
    RetailMaterialFileSystem actual;
    const SourceAllocationBank& bank;
    unsigned sizes{},reads{};
    bool corrupt{},short_read{};
    Files(const AssetStore& assets,const std::filesystem::path& root,const SourceAllocationBank& b):actual(assets,root),bank(b){}
    std::uint32_t source_file_size(const MaterialResourcePath& path) override{++sizes;return actual.source_file_size(path);}
    MaterialFileRead read_source_file(const MaterialResourcePath& path,std::span<std::uint8_t> destination) override{
        ++reads;const bool tga=path.relative_name.ends_with(".TGA");
        if(tga){
            const auto address=reinterpret_cast<std::uintptr_t>(destination.data());const auto records=bank.source_records();
            check(std::any_of(records.begin(),records.end(),[&](const auto& r){return r.raw!=0&&r.aligned==address&&r.requested_bytes==destination.size();}),
                "real filesystem destination is not the bank input");
        }
        const auto result=actual.read_source_file(path,tga&&short_read?destination.first(destination.size()-1):destination);
        if(tga&&corrupt)destination[16]=8; //explicit fault injection AFTER actual file read, never edits retail files
        return result;
    }
};
struct Target final:FontAtlasUploadTarget {
    SourceAllocationBank& bank;Audit& audit;
    std::uint8_t transparency{};
    unsigned reads{},uploads{},fail_upload{};
    std::uint64_t consumed{};
    Target(SourceAllocationBank& b,Audit& a):bank(b),audit(a){}
    std::uint8_t& current_transparency() override{++reads;return transparency;}
    std::uint32_t upload_font_atlas(PreparedMaterialPixels pixels) override{
        const auto bytes=bank.bytes(audit.atlas);
        check(pixels.width==256&&pixels.height==256&&pixels.words.size()==65536,
            "actual font upload extent");
        check(reinterpret_cast<const std::uint8_t*>(pixels.words.data())==bytes.data(),"upload got a shadow payload");
        for(const auto word:pixels.words)consumed+=word; //actual consumer, not a token-only stub
        ++uploads;
        if(uploads==fail_upload)throw std::runtime_error("explicit upload completion failure");
        return 6u+uploads;
    }
};
struct Fixture {
    AssetStore assets;
    SourceAllocationBank bank;
    Files files;
    std::unique_ptr<RetailFontResources> fonts=std::make_unique<RetailFontResources>();
    WorldMaterialLoadingState world;
    Audit audit;
    Target target;
    explicit Fixture(const std::filesystem::path& root,bool initialize=true)
        :assets(root),files(assets,root,bank),audit(bank),target(bank,audit){
        if(initialize)bank.initialize_42eab0(); //fixture event, never implicit in font production
        // Independent caller prerequisite:42C510 calls42BDD0 before fonts.
        // Also required in the negative bank case; reset once, never in one/all.
        reset_material_reload_42bdd0(world.reload);
        fonts->current=1;
    }
    SourceFontContext context(){return {bank,world.source_tga_decode,&audit};}
    FontImageGlobals image(){return {world.width,world.height,world.tga_width,world.tga_height};}
    FontReloadViews reload(){return font_reload_views(world.reload,world.names.physical_upload_count);}
    void one(std::uint32_t enabled=1,std::string_view name="I32_b05"){
        load_source_font_resource_428e40(*fonts,0,context(),files,image(),reload(),"Graphics24",name,800,enabled,target);
    }
    void all(){initialize_retail_font_resources(*fonts,context(),files,image(),reload(),"Graphics24",800,1,target);}
};
void execute(std::string_view name,const std::filesystem::path& root){
    if(name=="uninitialized"){
        Fixture f(root,false);
        check(f.world.reload.initialized&&!f.bank.initialized(),"negative case must omit only the bank initializer");
        (void)f.reload(); //prove argument preparation succeeds before entering the producer
        bool bank_guard=false;
        try{f.all();}catch(const FormatError& error){
            check(std::string_view(error.what())=="font bank requires explicit prior source42EAB0",
                "uninitialized test stopped at a different prerequisite");bank_guard=true;
        }
        check(bank_guard,"font initializer did not reject the uninitialized source bank");
        check(f.files.sizes==0&&f.files.reads==0&&f.bank.live_count()==0&&f.fonts->count==0,"uninitialized bank had producer effects");
        check(f.audit.events.empty()&&f.target.uploads==0&&f.target.reads==0&&!f.bank.initialized(),"negative bank case reached allocation/upload or initialized the bank");
    }else if(name=="missing_file"){
        Fixture f(root);rejects([&]{f.one(1,"__missing_source_font_fixture__");});
        check(f.files.sizes==2&&f.files.reads==0&&f.bank.live_count()==1&&f.bank.bytes(f.audit.outer).empty(),
            "missing file must retain the actual zero-sized outer allocation");
        check(f.bank.source_words().ordinary_bytes==36&&f.audit.events.size()==1,"missing file invented nested input/free");
    }else if(name=="invalid_format"||name=="short_read"){
        Fixture f(root);f.files.corrupt=name=="invalid_format";f.files.short_read=name=="short_read";
        rejects([&]{f.one();});
        check(f.files.sizes==2&&f.files.reads==1&&f.bank.live_count()==2,"file failure must retain outer and nested allocations");
        check(live(f.bank,f.audit.input)&&live(f.bank,f.audit.outer)&&!f.audit.atlas,"file failure manufactured atlas/free");
        check(f.world.source_tga_decode.file==f.audit.input&&f.world.source_tga_decode.pixel_depth==0,
            "failed file/format unexpectedly published decoder header globals");
    }else if(name=="first_upload"||name=="second_upload"){
        Fixture f(root);f.target.fail_upload=name=="first_upload"?1u:2u;
        f.fonts->source_word(0,0x20)=0x1234;f.fonts->source_word(0,0x24)=0x5678;
        rejects([&]{f.one();});
        check(f.bank.live_count()==3&&!live(f.bank,f.audit.input)&&live(f.bank,f.audit.outer)&&live(f.bank,f.audit.atlas)&&live(f.bank,f.audit.scratch),
            "upload stop freed reached font allocations");
        check(f.target.uploads==f.target.fail_upload&&f.target.consumed!=0&&f.fonts->count==0&&f.world.reload.font_cursor==0,
            "upload stop published font/reload completion");
        check(f.fonts->source_word(0,0x20)==(f.target.fail_upload==1?0x1234u:7u)&&f.fonts->source_word(0,0x24)==0x5678,
            "source first/second upload publication order");
    }else if(name=="observer_abort"){
        Fixture f(root);f.audit.abort_site=0x4327e0;rejects([&]{f.one();});
        check(f.bank.live_count()==3&&f.target.uploads==0&&f.fonts->count==0,"diagnostic stop erased actual source allocations");
    }else if(name=="disabled"){
        Fixture f(root);f.one(2);
        check(f.target.uploads==0&&f.target.reads==0&&f.fonts->count==1&&f.bank.live_count()==1,
            "disabled upload must still complete file/pack/scratch and source frees");
        check(f.bank.source_words().next_ordinal==4&&!live(f.bank,f.audit.input)&&!live(f.bank,f.audit.atlas)&&!live(f.bank,f.audit.outer),
            "disabled branch source allocation history");
    }else if(name=="scope_and_owner"){
        Fixture f(root);f.all();const auto first=f.fonts->source_scratch_allocation(0,f.bank);
        check(first==f.audit.scratch&&live(f.bank,first),"font scratch did not publish actual bank identity");
        f.bank.begin_42ea80(); //core scope event only, NOT actual production warm transition
        check(!live(f.bank,first),"actual font scratch survived a matching source scope sweep");
        f.all();const auto second=f.fonts->source_scratch_allocation(0,f.bank);
        check(!(first==second)&&f.fonts->scratch_allocations()==2&&f.bank.live_count()==1,"repeated source font generation/history");
        f.fonts.reset();check(live(f.bank,second),"font destructor illegally freed source-owned scratch");
    }else if(name=="high_ordinal"){
        Fixture f(root);
        //Synthetic core precondition, not a claimed game producer history.
        for(unsigned i=0;i<384;++i){const auto ref=f.bank.allocate_42e8d0(1,0);check(bool(ref),"prefix allocation");(void)f.bank.release_42e980(ref);}
        f.one();const auto scratch=f.audit.scratch;check(f.bank.source_words().next_ordinal==388,"actual font did not advance source ordinals");
        f.bank.begin_42ea80();check(live(f.bank,scratch)&&f.bank.live_count()==1,"source high-ordinal font row was bulk-freed");
    }else if(name=="scratch_null"){
        Fixture f(root);
        for(unsigned i=0;i<382;++i)check(bool(f.bank.allocate_42e8d0(1,0)),"synthetic capacity prefix");
        f.one();
        check(!f.audit.scratch&&f.fonts->source_word(0,4)==0&&f.fonts->scratch_allocations()==0,
            "source unused scratch null must be published without fabricated ownership");
        check(f.fonts->count==1&&f.target.uploads==2&&f.bank.live_count()==382,"null scratch incorrectly stopped or leaked font temporaries");
    }else if(name=="nested_null"){
        Fixture f(root);
        for(unsigned i=0;i<383;++i)check(bool(f.bank.allocate_42e8d0(1,0)),"synthetic capacity prefix");
        rejects([&]{f.one();});check(f.bank.live_count()==384&&live(f.bank,f.audit.outer)&&!f.world.source_tga_decode.file,
            "nested null must retain prior actual outer row and publish null input ref");
        check(f.files.sizes==2&&f.files.reads==0&&f.target.uploads==0,"nested null entered unsafe payload read");
    }else if(name=="mode_and_bank"){
        Fixture f(root);f.all();const auto before=f.fonts->source_word(0,0x20);const auto count=f.fonts->count;
        SourceAllocationBank other;other.initialize_42eab0();rejects([&]{f.fonts->bind_source_bank(other);});
        rejects([&]{initialize_retail_font_resources(*f.fonts,f.files,f.image(),f.reload(),"Graphics24",800,1,f.target);});
        check(f.fonts->source_word(0,0x20)==before&&f.fonts->count==count&&other.live_count()==0,"mode/bank rejection mutated actual fonts");
        RetailFontResources preview;preview.allocate_scratch(0);rejects([&]{preview.bind_source_bank(f.bank);});
        rejects([&]{(void)other.bytes(f.audit.scratch);});
    }else throw std::runtime_error("unknown source-font receiving case");
}
}
int main(int argc,char** argv){try{
    check(argc==3,"expected case name and actual retail root");execute(argv[1],argv[2]);
    std::cout<<"{\"passed\":true,\"case\":\""<<argv[1]<<"\",\"GPU\":false,\"actual_warm_transition\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
