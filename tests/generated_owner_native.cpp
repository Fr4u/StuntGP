#include "generated_owner_cases.hpp"
#include "sgn/retail_world_sprite_model.hpp"
#include "sgn/retail_world_sprite_producer.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include <iostream>
#include <memory>

using namespace sgn;using namespace sgn::retail;namespace gc=generated_owner_cases;
struct Binding:VehicleStripBinding{VehicleStripGeometry* value{};void bind(VehicleStripGeometry& g)override{value=&g;}};
struct State {
    WorldModelDrawState draws;SourceSpritePacketStorage sprites;VehicleStripGeometry strip;Binding binding;
    State(){initialize_world_sprite_model(draws,sprites);expand_strip_geometry(strip,binding);}
    void input(const gc::Input& values){
        auto& source=draws.sprite_source;source.normals[0]=std::bit_cast<VehicleVector>(values.normals);source.bound_vertices[0]=std::bit_cast<VehicleVector>(values.bound);
        source.bound_indices[0]=values.sprite_binding?std::optional<unsigned>{0}:std::nullopt;source.texture_namespace_word=values.texture_word;
        strip.retained_bound_tail=values.strip_tail;strip.bound_index=values.strip_binding?std::optional<unsigned>{0}:std::nullopt;strip.texture_names=values.strip_names;
    }
    gc::Bytes capture()const{
        gc::Bytes out;const auto& source=draws.sprite_source;
        gc::require((!source.bound_indices[0]||*source.bound_indices[0]==0)&&(!strip.bound_index||*strip.bound_index==0),"native bound binding outside represented domain");
        gc::append(out,source.normals);gc::append(out,source.bound_vertices);
        gc::append(out,unsigned(source.bound_indices[0].has_value()));gc::append(out,source.texture_namespace_word);gc::append(out,strip.retained_bound_tail);gc::append(out,unsigned(strip.bound_index.has_value()));gc::append(out,strip.texture_names);
        gc::append(out,sprites.headers);gc::append(out,sprites.indices);gc::append(out,sprites.positions);gc::append(out,sprites.attributes);
        gc::append(out,strip.polygons);gc::append(out,strip.corner_indices);gc::append(out,strip.vertices);gc::append(out,strip.corners);gc::append(out,strip.normals);
        gc::append(out,draws.sprite_part_transform);gc::append(out,strip.part_transform);gc::require(out.size()==94428,"native initialization observation extent");return out;
    }
};
void contracts(){
    auto state=std::make_unique<State>();auto& source=state->draws.sprite_source;
    gc::require(&source.bound_vertex(0)==&source.bound_vertices[0],"sprite sole bound owner");
    gc::require(reinterpret_cast<const void*>(&source.bound_vertex(0))!=reinterpret_cast<const void*>(&source.normals[0]),"normal/bound merged");
    auto copy=source;gc::require(&copy.bound_vertex(0)==&copy.bound_vertices[0],"copied index retained old owner");
    source.bound_indices[0].reset();unsigned rejected{};try{(void)source.bound_vertex(0);}catch(const std::exception&){++rejected;}
    source.bound_indices[0]=1;try{(void)source.bound_vertex(0);}catch(const std::exception&){++rejected;}
    gc::require(rejected==2,"invalid bound binding accepted");
    state->strip.bound_index.reset();state->strip.retained_bound_tail=0xabcddcba;initialize_strip_geometry(state->strip,state->binding);
    gc::require(state->strip.bound_index==0u&&state->strip.retained_bound_tail==0xabcddcba&&state->binding.value==&state->strip,"strip source rebinding/tail");
    std::cout<<"owner_contracts=6 passed=1\n";
}
int main(int argc,char** argv)try{
    if(argc==2&&std::string_view(argv[1])=="--contracts"){contracts();return 0;}
    gc::require(argc==2,"native owners expects output file");gc::Writer out(argv[1]);auto state=std::make_unique<State>();unsigned initializers{},facing{};
    for(unsigned kind=0;kind<3;++kind)for(unsigned group=0;group<gc::groups;++group){
        state->input(gc::fixture(group));out.record(kind,group,0,state->capture());
        for(unsigned phase=1;phase<=2;++phase){
            if(kind==0)initialize_world_sprite_model(state->draws,state->sprites);
            else if(kind==1)initialize_strip_geometry(state->strip,state->binding);
            else expand_strip_geometry(state->strip,state->binding);
            ++initializers;out.record(kind,group,phase,state->capture());
        }
    }
    for(unsigned clipped=0;clipped<2;++clipped)for(unsigned group=0;group<gc::groups;++group){
        initialize_world_sprite_model(state->draws,state->sprites);auto& s=state->sprites;auto& draws=state->draws;
        draws.sprite_source.normals[0]=std::bit_cast<VehicleVector>(gc::fixture(group).normals);
        s.polygons=1;s.vertices=s.corners=4;s.headers[0]={4,1,255,0,0,0,0,0};
        for(unsigned i=0;i<4;++i){s.positions[i]=std::bit_cast<CameraVector>(gc::positions[i]);s.attributes[i]={0,0,static_cast<std::uint16_t>(i),0x2317,0x00ffffff};}
        ModelTransform outer{{0,0,0},0,{1,1,1},0,{0,0,1},0,{0,1,0},0,{0,1,1,0}};draws.sprite_outer_transform=&outer;
        CameraState camera;camera.position={0,20,0,0};camera.world_to_camera_reference=1;camera.world_to_camera_rotation=gc::identity;
        ViewMatrixArena arena;WorldModelViewScratch scratch{};const ProjectionParameters projection{gc::projection[0],gc::projection[1],gc::projection[2],gc::projection[3],gc::projection[4]};
        std::array<WorldPolygonVertex,8> vertices;for(auto& v:vertices)v.words.fill(gc::vertex_fill);
        std::array<std::array<WorldPolygonPacket,4>,2> pools;
        for(unsigned p=0;p<2;++p)for(unsigned i=0;i<4;++i)pools[p][i]={world_polygon_header_ref(s.headers[0]),s.indices,WorldPolygonAttributeView(std::span(s.attributes)),vertices,gc::packet_fill+p*16+i};
        WorldTransformedVertexCursor cursor{vertices,0};WorldPolygonPacketStorageLifetime opaque_lifetime(pools[0]),alpha_lifetime(pools[1]);WorldPolygonProducerList opaque{opaque_lifetime.pointer(),opaque_lifetime.pointer(0)},translucent{alpha_lifetime.pointer(),alpha_lifetime.pointer(0)};
        produce_world_sprite_unlit(draws,s,camera,arena,gc::identity,projection,clipped!=0,scratch,cursor,opaque,translucent);++facing;
        gc::Bytes data;gc::append(data,draws.sprite_source.normals);gc::append(data,std::array<unsigned,3>{cursor.count,opaque.count(),translucent.count()});gc::append(data,vertices);gc::append(data,scratch);
        for(const auto& pool:pools)for(const auto& p:pool){gc::require(p.attributes.bytes().data()==std::as_bytes(std::span(s.attributes)).data(),"native attribute alias");
            const auto header=static_cast<unsigned>(WorldPolygonHeaderView(std::span(s.headers)).index_of(p.header));
            gc::append(data,std::array<unsigned,5>{header+1,static_cast<unsigned>(p.indices.data()-s.indices.data())+1,1,static_cast<unsigned>(p.vertices.data()-vertices.data())+1,p.inside_mask});}
        gc::require(data.size()==540,"native facing record size");out.record(3+clipped,group,1,data);
    }
    const auto records=out.finish();std::cout<<"{\"passed\":true,\"initializer_calls\":"<<initializers<<",\"facing_calls\":"<<facing<<",\"records\":"<<records<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
