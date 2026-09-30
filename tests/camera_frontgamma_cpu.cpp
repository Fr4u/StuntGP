// Authored CPU contracts, not executed in this packet. No GPU/native game run.
// Analytic data checks selection, wire bytes and field coverage, not retail parity.
#include "sgn/receiving_camera_witness.hpp"
#include <iostream>
#include <iterator>
#include <memory>

using namespace sgn;
using namespace sgn::diagnostics;
namespace {
void require(bool condition,const char* what){if(!condition)throw std::runtime_error(what);}
template<class F> void rejects(F&& call,const char* what){bool failed=false;try{call();}catch(const std::exception&){failed=true;}require(failed,what);}
ImmutablePolygonSubmission submission(const PolygonDeviceIdentityRef& device,std::uint64_t tick){
    auto commands=std::make_shared<CapturedPolygonFrame>();commands->source_tick=tick;commands->resource_epoch=7;
    auto frame=std::make_shared<CapturedPolygonDeviceFrame>();frame->device=device;frame->polygon_commands=commands;
    auto textures=std::make_shared<PolygonGpuTextures>();textures->epoch=7;
    auto value=std::make_shared<CapturedPolygonSubmission>();value->device=device;value->source_tick=tick;
    value->segments.push_back({frame,textures});return value;
}
SdrGammaRamp ramp(unsigned seed,unsigned step){
    SdrGammaRamp value{};for(std::size_t i=0;i<value.size();++i)value[i]=static_cast<std::uint16_t>((seed+i*step)&65535u);return value;
}
std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);require(bool(file),"read test output");
    const std::string text{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    std::vector<std::uint8_t> result;result.reserve(text.size());for(const auto c:text)result.push_back(static_cast<std::uint8_t>(c));return result;
}
// A legal live byte array in exclusively owned VirtualAlloc pages. The span
// is constructed while accessible, then cached OUTSIDE those pages. Protecting
// storage does not fabricate an invalid pointer range or touch heap pages.
class InaccessibleBytePage final {
    struct Storage {std::array<std::uint8_t,4096> bytes{};};
    static_assert(std::is_trivially_destructible_v<Storage>);
    void* allocation_{};
    std::span<const std::uint8_t> view_;
    DWORD prior_protection_{PAGE_READWRITE};
    bool protected_{};
    void release_noexcept() noexcept{
        if(!allocation_)return;
        if(protected_){DWORD ignored{};(void)VirtualProtect(allocation_,sizeof(Storage),prior_protection_,&ignored);}
        // Storage is trivially destructible. Cleanup never accesses its bytes,
        // including if restoring access failed; VirtualFree releases OWN pages.
        (void)VirtualFree(allocation_,0,MEM_RELEASE);
        allocation_=nullptr;view_={};protected_=false;
    }
public:
    InaccessibleBytePage(){
        allocation_=VirtualAlloc(nullptr,sizeof(Storage),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(!allocation_)throw std::runtime_error("test VirtualAlloc failed");
        try{
            auto* storage=std::construct_at(static_cast<Storage*>(allocation_));
            view_=std::span<const std::uint8_t>{storage->bytes};
            if(!VirtualProtect(allocation_,sizeof(Storage),PAGE_NOACCESS,&prior_protection_))
                throw std::runtime_error("test PAGE_NOACCESS failed");
            protected_=true;
        }catch(...){release_noexcept();throw;}
    }
    ~InaccessibleBytePage() noexcept{release_noexcept();}
    InaccessibleBytePage(const InaccessibleBytePage&)=delete;
    InaccessibleBytePage& operator=(const InaccessibleBytePage&)=delete;
    std::span<const std::uint8_t> view() const noexcept{return view_;}
    void close(){
        if(!allocation_)return;
        if(protected_){
            DWORD ignored{};
            if(!VirtualProtect(allocation_,sizeof(Storage),prior_protection_,&ignored))
                throw std::runtime_error("test restoring page access failed");
            protected_=false;
        }
        if(!VirtualFree(allocation_,0,MEM_RELEASE))throw std::runtime_error("test VirtualFree failed");
        allocation_=nullptr;view_={};
    }
};
bool has_difference(const CameraRecoverySnapshot& a,const CameraRecoverySnapshot& b,std::string_view name){
    const auto get=[&](const auto& s)->const CameraRecoveryField*{for(const auto& f:s.fields)if(f.name==name)return &f;return nullptr;};
    const auto left=get(a),right=get(b);return left&&right&&left->bytes!=right->bytes;
}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"expected existing output parent");
        const auto output=std::filesystem::path(argv[1])/("camera-front-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        front_bundle_detail::fresh_directory(output);
        const PolygonDeviceIdentityRef device=std::make_shared<PolygonDeviceIdentity>();
        const auto a=submission(device,31),b=submission(device,32);
        const auto front_ramp=ramp(0x1234,73),back_ramp=ramp(0x9876,131),current_ramp=ramp(0x5317,17);
        require(front_ramp!=back_ramp&&front_ramp!=current_ramp&&back_ramp!=current_ramp,"three discriminating ramps");
        PolygonSurfaceRecipes recipes;recipes.submissions={a,b};recipes.ramps={front_ramp,back_ramp};
        PolygonSurfaceRoles images{true,11,22,1280,720};
        auto front=select_polygon_front_snapshot(recipes,images,images,device);
        require(front&&front->submission==a&&front->gamma==front_ramp&&front->gamma!=back_ramp&&front->gamma!=current_ramp,"must select front recipe AND front gamma");
        const auto retained=*front;
        const auto encoded=encode_front_gamma(front->gamma);
        // Independently chosen sentinels catch endian/channel/stride errors.
        require(encoded[0]==0x34&&encoded[1]==0x12,"red first word little-endian");
        require(encoded[512]==0x34&&encoded[513]==0x5b,"green first word little-endian");
        require(encoded[1024]==0x34&&encoded[1025]==0xa4,"blue first word little-endian");
        export_front_recipe_bundle(output/"selected-front",*front);
        const auto saved=read_bytes(output/"selected-front"/"gamma.u16le");
        require(saved==std::vector<std::uint8_t>(encoded.begin(),encoded.end()),"persisted exact front gamma");
        rejects([&]{export_front_recipe_bundle(output/"selected-front",*front);},"exclusive directory reuse rejected");
        const WorldJoinSidecarView missing_join;
        rejects([&]{export_front_recipe_bundle(output/"incomplete",*front,&missing_join);},"incomplete join prevents completion record");
        require(std::filesystem::exists(output/"incomplete"/"recipe.bin")&&std::filesystem::exists(output/"incomplete"/"gamma.u16le")&&
            std::filesystem::exists(output/"incomplete"/"join.json")&&!std::filesystem::exists(output/"incomplete"/"front.json"),"partial evidence retained without completion");
        recipes.ramps[1]=current_ramp; // accepted back changed; displayed front did not.
        front=select_polygon_front_snapshot(recipes,images,images,device);
        require(front->submission==a&&front->gamma==front_ramp,"pending accepted back must not supply displayed gamma");
        recipes.ramps[1]=back_ramp;recipes.exchange();std::swap(images.front_image,images.back_image);
        front=select_polygon_front_snapshot(recipes,images,images,device);
        require(front->submission==b&&front->gamma==back_ramp,"exchange keeps recipe/ramp together");
        require(retained.submission==a&&retained.gamma==front_ramp,"owned snapshot survives later exchange unchanged");
        images.front_image=111;images.back_image=222; // analytic successful recreation tags
        front=select_polygon_front_snapshot(recipes,images,images,device);
        require(front->submission==b&&front->gamma==back_ramp,"new GPU tags retain CPU front pair");
        const auto expected=images;
        auto stale=images;stale.width=1920;
        rejects([&]{(void)select_polygon_front_snapshot(recipes,stale,expected,device);},"stale extent rejected without restore");
        stale=images;stale.front_image=333;
        rejects([&]{(void)select_polygon_front_snapshot(recipes,stale,expected,device);},"foreign front role rejected");
        const PolygonDeviceIdentityRef foreign=std::make_shared<PolygonDeviceIdentity>();
        rejects([&]{(void)select_polygon_front_snapshot(recipes,images,images,foreign);},"foreign context rejected");
        PolygonSurfaceRecipes empty;PolygonSurfaceRoles none{};
        require(!select_polygon_front_snapshot(empty,none,none,device),"both absent means no snapshot");
        rejects([&]{(void)select_polygon_front_snapshot(empty,images,images,device);},"one-sided presence rejected");

        // A real native state type, populated with analytic sentinel fields.
        // Its allocation identity stays the same across every comparison.
        auto state=std::make_unique<retail::RaceSessionState>();
        const CameraRecoveryStamp stamp{33,33,33,33,33};
        const auto capture=[&]{return capture_camera_recovery_state(*state,retail::RaceSessionReadiness::source_attachment_only,stamp,front);};
        auto base=capture();require(base==capture(),"stable same-process field snapshot");
        auto& camera=state->controls.controllers[0].camera;
        camera.position.w=std::bit_cast<float>(0x80000000u);
        require(has_difference(base,capture(),"camera/controller/0/position"),"negative zero W lane detected");camera.position.w=0;
        camera.projection[7]=std::bit_cast<float>(0x7fc01234u);base=capture();
        camera.projection[7]=std::bit_cast<float>(0x7fc01235u);
        require(has_difference(base,capture(),"camera/controller/0/native-only/projection"),"NaN payload bit detected");camera.projection[7]=0;
        base=capture();state->shared_model_identity[15]=std::bit_cast<float>(1u);
        require(has_difference(base,capture(),"camera/shared-model-identity"),"sole61C468 bit detected");state->shared_model_identity[15]=0;
        base=capture();state->view_matrices.matrices[383][15]=std::bit_cast<float>(1u);
        require(has_difference(base,capture(),"camera/matrix-arena"),"final matrix arena word detected");state->view_matrices.matrices[383][15]=0;
        base=capture();state->camera_lifecycle[3].current_record=0;
        require(base!=capture(),"null versus record zero detected");state->camera_lifecycle[3].current_record.reset();
        base=capture();state->viewport.full_width=1920;
        require(has_difference(base,capture(),"viewport/full-width"),"resize remains a mismatch");state->viewport.full_width=0;
        base=capture();front->gamma[256]^=1;
        require(has_difference(base,capture(),"front/gamma-u16le"),"same camera and counts cannot hide one gamma bit");front->gamma[256]^=1;
        {
            InaccessibleBytePage owned_page;
            state->camera_presentation[0].spline=retail::SplineView{{},owned_page.view(),4096};
            (void)capture(); // A mistaken dereference faults on this legal live range.
            state->camera_presentation[0].spline.reset(); // End the borrow before release.
            owned_page.close(); // Check successful restoration/release; RAII handles exceptions.
        }
        std::array<std::uint8_t,4> backing{1,2,3,4};
        state->camera_presentation[0].spline=retail::SplineView{{},backing,4};
        const auto loaded=[&]{return capture_camera_recovery_state(*state,retail::RaceSessionReadiness::preview_ready,stamp,front);};
        base=loaded();backing[3]^=1;
        require(has_difference(base,loaded(),"camera/controller/0/spline/backing"),"available borrowed backing byte detected");
        state->camera_presentation[0].spline.reset();

        // Save a complete analytic interval for independent Python readback.
        CameraRecoveryInterval interval(output/"interval",1);
        export_front_recipe_bundle(interval.directory()/"front-before",*front);
        interval.begin(capture());
        export_front_recipe_bundle(interval.directory()/"front-after",*front);
        interval.finish(capture());
        rejects([&]{interval.finish(capture());},"second completion rejected");
        CameraRecoveryInterval mismatch(output/"mismatch-interval",2);
        export_front_recipe_bundle(mismatch.directory()/"front-before",*front);mismatch.begin(capture());
        state->shared_model_identity[0]=std::bit_cast<float>(1u);
        export_front_recipe_bundle(mismatch.directory()/"front-after",*front);
        rejects([&]{mismatch.finish(capture());},"same gamma cannot hide changed camera identity matrix");
        require(std::filesystem::exists(mismatch.directory()/"recovery.json"),"mismatch evidence record preserved");
        (void)front_bundle_detail::write_text(output/"cases.json","{\"schema\":\"sgn.camera-front-cpu-cases.v1\",\"analytic_only\":true,\"retail_or_gpu_qualification\":false}\n");
        std::cout<<output.string()<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
