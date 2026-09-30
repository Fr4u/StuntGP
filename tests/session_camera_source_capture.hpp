#pragma once
#include "sgn/race_session.hpp"
#include "retail_camera_lifecycle_state.hpp"
#include <algorithm>
#include <limits>
#include <string>

// Output-only qualification adapter. It does not initialize or import any
// Session field. Addresses below belong to the pinned D3D image, not the host.
namespace sgn::retail::session_camera_test {
using Bytes=std::vector<std::uint8_t>;
inline constexpr unsigned global_base=0x470000,global_size=0x1c8000;
inline constexpr unsigned controller_base=0x498248,controller_stride=0x74c;
inline constexpr unsigned camera_bank_base=0x4722d8,light_vertices_base=0x72000000;
struct SourceBlock {std::string name;unsigned address{};Bytes bytes;};
struct SourceRange {unsigned address{},length{};};
struct CameraSourceCapture {
    std::vector<SourceBlock> extra_blocks;
    std::vector<SourceRange> written_ranges;
};
namespace detail {
inline void require(bool ok,const char* message){if(!ok)throw FormatError(message);}
inline unsigned word(std::span<const std::uint8_t> bytes,std::size_t at){
    require(at<=bytes.size()&&bytes.size()-at>=4,"camera capture PMD word bounds");
    unsigned value;std::memcpy(&value,bytes.data()+at,4);return value;
}
inline bool same_descriptor(const SplineDescriptor& a,const SplineDescriptor& b){
    return a.id==b.id&&a.point_index==b.point_index&&a.point_count==b.point_count&&a.opaque==b.opaque&&
        std::bit_cast<unsigned>(a.length)==std::bit_cast<unsigned>(b.length)&&a.point_offset==b.point_offset;
}
inline unsigned record_address(std::optional<unsigned> index){
    require(!index||*index<32,"camera capture authored record index");
    return index?camera_bank_base+*index*64:0u;
}
inline unsigned arena_address(unsigned token){
    if(token==0)return 0;
    if(token==1)return 0x61c468u;
    require(token>=2&&(token-2)%64==0&&(token-2)/64<384,"camera capture invalid arena reference");
    return 0x61c7ccu+token-2;
}
inline unsigned spline_address(const std::optional<SplineView>& view,const SplineSet& splines,
                               std::span<const std::uint8_t> track,unsigned track_base){
    if(!view)return 0;
    const auto canonical=splines.find(view->descriptor.id);
    require(canonical.has_value(),"camera capture spline not registered");
    require(view->storage.data()==canonical->storage.data()&&view->storage.size()==canonical->storage.size()&&
        view->point_storage_end==canonical->point_storage_end&&same_descriptor(view->descriptor,canonical->descriptor),
        "camera capture spline is not the Session borrowed view");
    require(canonical->storage.size()==track.size()&&std::equal(track.begin(),track.end(),canonical->storage.begin()),
        "camera capture spline PMD identity differs");
    const auto bytes=word(track,0x10c);const auto at=std::uint64_t{0x148}+word(track,0x78);
    require(bytes%28==0&&at<=track.size()&&bytes<=track.size()-at,"camera capture spline table bounds");
    for(unsigned i=0;i<bytes/28;++i)if(word(track,static_cast<std::size_t>(at+i*28))==view->descriptor.id)
        return track_base+static_cast<unsigned>(at)+i*28;
    throw FormatError("camera capture authored spline descriptor absent");
}
}
inline CameraSourceCapture capture_session_camera_source(const RaceSessionState& s,const SplineSet& splines,
    std::span<const std::uint8_t> track,Bytes& globalcarrier,unsigned track_base=0x11400000){
    using detail::require;
    require(globalcarrier.size()==global_size,"camera capture requires exact470000..638000 carrier");
    require(track_base>=global_base+global_size&&std::uint64_t(track_base)+track.size()<=0x100000000ull,
        "camera capture source track address range");
    require(track.size()>=0x148&&std::string_view(reinterpret_cast<const char*>(track.data()),10)==std::string_view("PMD V1.83\0",10),
        "camera capture requires actual PMD V1.83");
    require(s.controls.current<4&&s.camera_bank.count<=32,"camera capture selected controller or bank count");
    require(s.light_vertices.size()==0x4800&&s.light_vertex_offset<=s.light_vertices.size(),"camera capture light allocation/cursor");
    require(!(std::uint64_t(track_base)<std::uint64_t(light_vertices_base)+s.light_vertices.size()&&
              light_vertices_base<std::uint64_t(track_base)+track.size()),"camera capture overlapping source allocations");
    std::array<unsigned,4> routes{},current_records{},pending_records{};
    for(unsigned i=0;i<4;++i){
        require(s.controls.controllers[i].vehicle<6,"camera capture vehicle slot");
        routes[i]=detail::spline_address(s.camera_presentation[i].spline,splines,track,track_base);
        current_records[i]=detail::record_address(s.camera_lifecycle[i].current_record);
        pending_records[i]=detail::record_address(s.camera_lifecycle[i].pending_record);
    }
    // Stage all output so malformed capture input cannot partially alter the
    // caller's carrier. Session remains const even on successful calls.
    Bytes output=globalcarrier;CameraSourceCapture result;
    const auto blob=[&](unsigned address,std::span<const std::uint8_t> bytes){
        require(address>=global_base&&address-global_base<=output.size()&&bytes.size()<=output.size()-(address-global_base),
            "camera capture global write bounds");
        require(bytes.size()<=std::numeric_limits<unsigned>::max(),"camera capture range size");
        std::memcpy(output.data()+address-global_base,bytes.data(),bytes.size());
        result.written_ranges.push_back({address,static_cast<unsigned>(bytes.size())});
    };
    const auto field=[&](unsigned address,const auto& value){
        static_assert(std::is_trivially_copyable_v<std::remove_cvref_t<decltype(value)>>);
        blob(address,{reinterpret_cast<const std::uint8_t*>(&value),sizeof(value)});
    };
    for(unsigned i=0;i<4;++i){
        const auto at=controller_base+i*controller_stride;const auto& controller=s.controls.controllers[i];
        camera_test::Bytes c(camera_test::camera_size),dummy_vehicle(camera_test::vehicle_size);
        std::copy_n(globalcarrier.begin()+at-global_base,c.size(),c.begin());
        auto camera=controller.camera;auto life=s.camera_lifecycle[i];CameraVehicle dummy;
        camera_test::transfer(c,dummy_vehicle,camera,dummy,false);camera_test::lifecycle(c,life,false);
        camera_test::put(c,0x1cc,current_records[i]);camera_test::put(c,0x1d8,pending_records[i]);
        // Actual controller offsets only. +48 is the opaque lifecycle owner;
        // +4C/+50 encode source matrix identities, not the derived arrays.
        // +200..2CF in the old serializer are diagnostics, not camera storage.
        constexpr std::array<SourceRange,6> camera_ranges{{{4,0x48},{0x54,4},{0x68,0x24},
            {0xb0,0x64},{0x198,0x10},{0x1ac,0x3c}}};
        for(const auto range:camera_ranges)blob(at+range.address,{c.data()+range.address,range.length});
        field(at+0x4c,detail::arena_address(controller.camera.world_to_camera_reference));
        field(at+0x50,detail::arena_address(controller.camera.camera_to_world_reference));
        field(at,controller.active);field(at+0xac,0x49a1f8u+controller.vehicle*0x7724u);
        const auto& presentation=s.camera_presentation[i];
        field(at+0x8c,presentation.up_axis);field(at+0x9c,presentation.forward_axis);
        field(at+0x118,presentation.planes);field(at+0x1a8,routes[i]);
    }
    field(0x4722d0,controller_base+s.controls.current*controller_stride);
    field(camera_bank_base,s.camera_bank.records);field(0x499f90,s.camera_bank.count);
    field(0x499f94,s.camera_session.authored_cycle_index);field(0x499f88,s.camera_session.path_speed);
    field(0x499f8c,s.camera_session.path_parameter);field(0x60f020,s.selected_camera_preset);
    field(0x61c3e8,s.frame_projection);field(0x61c468,s.shared_model_identity);
    field(0x61c534,s.inverse_depth_scale);field(0x61c538,s.depth_offset);field(0x61c594,s.frame_world_view);
    require(!s.view_matrices.cursor||(*s.view_matrices.cursor%64==0&&*s.view_matrices.cursor<=384*64),"camera capture invalid arena cursor");
    field(0x61c7cc,s.view_matrices.matrices);
    field(0x6227cc,s.view_matrices.cursor?0x61c7ccu+*s.view_matrices.cursor:0u);
    field(0x6227d0,s.view_matrices.count);
    const auto& v=s.viewport;
    field(0x576b80,v.width);field(0x576b84,v.height);field(0x576b8c,v.center_pixel_x);field(0x576b90,v.center_pixel_y);
    field(0x576b94,v.center_x);field(0x576b98,v.center_y);field(0x576b9c,v.scale_x);field(0x576ba0,v.scale_y);
    field(0x576ba4,v.right_edge);field(0x576ba8,v.bottom_edge);field(0x576bac,v.min_x);field(0x576bb0,v.min_y);
    field(0x576bb4,v.max_x);field(0x576bb8,v.max_y);field(0x576bbc,v.depth_bits);field(0x576bc0,v.bytes_per_pixel);
    field(0x576bc4,v.full_width);field(0x576bc8,v.full_height);field(0x576bd8,v.cropped);field(0x576bdc,v.selector);
    field(0x576be0,s.split_setting);field(0x576be4,v.origin_x);field(0x576be8,v.origin_y);
    field(0x576c04,v.clip_virtual);field(0x576c14,v.clip_screen);field(0x576c24,v.clip_expanded);
    field(0x624988,v.applied_selector);field(0x62498c,v.requested_selector);field(0x624990,v.ui_scale_x);field(0x624994,v.ui_scale_y);
    field(0x589df8,s.light_animation.phase_fast);field(0x589dfc,s.light_animation.phase_slow);field(0x477b88,s.glare.elements);
    field(0x624530,light_vertices_base);field(0x624574,light_vertices_base+s.light_vertex_offset);
    result.extra_blocks.push_back({"light_vertices",light_vertices_base,s.light_vertices});
    globalcarrier.swap(output);return result;
}
}
