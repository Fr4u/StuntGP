#include "sgn/retail_world_polygon_producer.hpp"
#include <iostream>
#include <utility>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool good,const char* text){if(!good)throw FormatError(text);}
template<class F>void rejects(F&& operation){bool bad=false;try{operation();}catch(const FormatError&){bad=true;}require(bad,"invalid borrowed view accepted");}
template<class T>concept RefFromTemporary=requires{world_polygon_header_ref(T{});};
static_assert(!RefFromTemporary<WorldPolygonHeader> && !RefFromTemporary<VehicleStripPolygon>);
template<class T>concept RefFromConstTemporary=requires{world_polygon_header_ref(std::declval<const T>());};
template<class T>concept ExplicitRefFromConstTemporary=requires{world_polygon_header_ref<const T>(std::declval<const T>());};
static_assert(!RefFromConstTemporary<WorldPolygonHeader> && !RefFromConstTemporary<VehicleStripPolygon>);
static_assert(!ExplicitRefFromConstTemporary<WorldPolygonHeader> && !ExplicitRefFromConstTemporary<VehicleStripPolygon>);
static_assert(!std::is_constructible_v<WorldPolygonHeaderRef,const WorldPolygonHeader*>);
static_assert(!std::is_constructible_v<WorldPolygonHeaderView,std::span<const std::byte>>);
static_assert(!std::is_constructible_v<WorldPolygonAttributeView,std::span<const std::byte>>);

template<class Header>void exercise(){
    std::array<Header,2> headers{};
    auto raw=std::as_writable_bytes(std::span(headers));raw[0]=std::byte{3};raw[3]=std::byte{0x80};
    const WorldPolygonHeaderView view{std::span(headers)};
    const auto reference=view.at(0);const auto old=reference.value();
    require(reference.identity()==static_cast<const void*>(&headers[0]),"header identity changed");
    require(view.subspan(0,1).at(0)==reference && view.index_of(reference)==0,"subspan lost record identity");
    require(view.subspan(2).empty(),"valid empty tail failed");
    std::array<Header,1> unrelated{};
    require(!view.find(world_polygon_header_ref(unrelated[0])),"different header owner accepted");
    rejects([&]{(void)view.at(2);});rejects([&]{(void)view.at(std::size_t(-1));});
    rejects([&]{(void)view.subspan(3);});rejects([&]{(void)view.subspan(1,2);});
    rejects([&]{(void)view.index_of(world_polygon_header_ref(unrelated[0]));});
    rejects([&]{(void)reference.byte(8);});

    std::array<VehicleStripCorner,3> attributes{};
    std::array<VehicleVector,3> authored{};std::array<VehicleVector,1> normals{};
    std::array<std::uint16_t,3> indices{0,1,2};std::array<WorldPolygonVertex,3> vertices{};
    for(unsigned i=0;i<3;++i){attributes[i].vertex=static_cast<std::uint16_t>(i);vertices[i].words[4]=63;}
    WorldPolygonPartSource source{view.subspan(0,1),WorldPolygonAttributeView(std::span(attributes)),authored,normals,indices};
    std::array<WorldPolygonPacket,1> packets{};WorldPolygonPacketStorageLifetime opaque_lifetime(packets);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer()},translucent;
    emit_world_polygons_clipped(source,vertices,{},opaque,translucent);
    require(opaque.count()==1&&translucent.count()==0&&packets[0].header==reference,"emitter copied a temporary header");
    raw[1]=std::byte{51};attributes[0].color=0x12345678;attributes[0].u=std::bit_cast<float>(0x80000000u);
    require(old[1]==0&&packets[0].header.byte(1)==51,"packet has stale header bytes");
    require(packets[0].attributes.value(0).color==0x12345678&&packets[0].attributes.value(0).u==0x80000000u,"packet has stale or converted attributes");

    std::array<WorldPolygonHeader,2> clipped_headers{};std::array<WorldPolygonAttribute,16> clipped_attributes{};
    std::array<WorldPolygonVertex,16> clipped_vertices{};std::array<std::uint16_t,16> unused{};
    WorldPolygonClipStorage clip{clipped_attributes,clipped_headers,unused,clipped_vertices};
    initialize_world_polygon_clip_storage(clip);clip_world_polygon(packets[0],clip,1.f);
    require(clip.header_count==1&&clip.attribute_count==3&&packets[0].header==world_polygon_header_ref(clipped_headers[0]),"clip packet escaped clip owner");
    require(packets[0].header!=reference&&packets[0].header.byte(1)==51&&packets[0].attributes.value(0).color==0x12345678,"clip data lost source values");
    raw[1]=std::byte{99};clipped_headers[0][1]=77;clipped_attributes[0].color=0x76543210;
    require(packets[0].header.byte(1)==77&&packets[0].attributes.value(0).color==0x76543210,"clip packet retained source or temporary owner");
    reject_world_polygon(packets[0]);require(!packets[0].header,"rejection retained header");
    rejects([&]{(void)packets[0].header.value();});rejects([&]{(void)packets[0].header.byte(0);});

    // The nonclipped emitter must retain the same identity and fifth word.
    opaque.copy_base_to_current();packets[0].inside_mask=0xfeed1234;
    emit_world_polygons_unclipped(source,vertices,{},opaque,translucent);
    require(opaque.count()==1&&packets[0].header==reference&&packets[0].inside_mask==0xfeed1234,"unclipped emitter lost identity or retained field");
}
}
int main()try{
    exercise<WorldPolygonHeader>();exercise<VehicleStripPolygon>();
    std::array<WorldPolygonAttribute,1> sprite{};const WorldPolygonAttributeView sprite_view{std::span(sprite)};sprite[0].color=81;
    std::array<std::array<std::uint32_t,4>,1> pmd{};const WorldPolygonAttributeView pmd_view{std::span(pmd)};pmd[0][3]=91;
    require(sprite_view.value(0).color==81&&pmd_view.value(0).color==91,"legacy live attribute owner changed");
    rejects([&]{(void)sprite_view.value(1);});
    std::cout<<"passed=1 owners=PMD,sprite,strip,clip emission_routes=2 live_header_and_color=1 temporary_and_raw_byte_factories_rejected=1\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
