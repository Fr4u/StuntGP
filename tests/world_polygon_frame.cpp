#include "sgn/retail_world_polygon_frame.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool okay,const char* message){if(!okay)throw FormatError(message);}
using Wire=std::array<std::uint32_t,75>;
class Reader {
    std::ifstream file_;
public:
    std::uint64_t compared{};
    explicit Reader(const char* path):file_(path,std::ios::binary){require(bool(file_),"missing world frame corpus");}
    void bytes(void* data,std::size_t size){file_.read(static_cast<char*>(data),static_cast<std::streamsize>(size));require(bool(file_),"truncated frame corpus");}
    template<class T>T value(){T v;bytes(&v,sizeof(v));return v;}
    std::uint32_t word(){return value<std::uint32_t>();}
    void equal(const void* data,std::size_t size,const char* message){std::vector<std::byte> expected(size);bytes(expected.data(),size);require(std::memcmp(data,expected.data(),size)==0,message);compared+=size;}
    void end(){require(file_.peek()==std::char_traits<char>::eof(),"trailing frame corpus");}
};
Wire state(const WorldPolygonFrame& f){
    Wire w{};std::size_t i=0;w[i++]=f.frame_stamp;w[i++]=f.remaining_first;w[i++]=f.spatial_batch_count;
    static_assert(sizeof(f.model_view)==24*4);std::memcpy(w.data()+i,&f.model_view,sizeof(f.model_view));i+=24;
    w[i++]=f.transformed.count;w[i++]=f.indexed.count();for(const auto& p:f.packets)w[i++]=p.count();for(const auto n:f.totals)w[i++]=n;
    for(const auto& group:f.groups)for(const auto& r:group){w[i++]=r.first.is_null()?0u:static_cast<std::uint32_t>(*r.first.offset()+1);w[i++]=r.count;}
    for(const auto& p:f.spatial)w[i++]=p.count;
    w[i++]=f.clip.attribute_count;w[i++]=f.clip.header_count;w[i++]=f.clip.unused_index_count;w[i++]=f.clip.vertex_count;
    require(i==w.size(),"frame wire size");return w;
}
void load(WorldPolygonFrame& f,const Wire& w){
    std::size_t i=0;f.frame_stamp=w[i++];f.remaining_first=w[i++];f.spatial_batch_count=w[i++];
    std::memcpy(&f.model_view,w.data()+i,sizeof(f.model_view));i+=24;
    f.transformed.count=w[i++];f.indexed.publish_current(f.allocations->indexed_lifetime.pointer(w[i++]));for(unsigned p=0;p<4;++p)f.packets[p].publish_current(f.allocations->packet_lifetimes[p].pointer(w[i++]));for(auto& n:f.totals)n=w[i++];
    for(auto& group:f.groups)for(unsigned p=0;p<4;++p){auto& r=group[p];const auto token=w[i++];r.first=token?f.allocations->packet_lifetimes[p].pointer(token-1):WorldPolygonPacketPointer{};r.count=w[i++];}
    for(auto& p:f.spatial)p.count=w[i++];
    f.clip.attribute_count=w[i++];f.clip.header_count=w[i++];f.clip.unused_index_count=w[i++];f.clip.vertex_count=w[i++];
}
void allocation_contract(){
    WorldPolygonFrame f;f.frame_stamp=65535;f.spatial_batch_count=173;f.model_view.local_camera={1,2,3,4};
    allocate_world_polygon_frame(f);require(f.frame_stamp==65535&&f.spatial_batch_count==173&&f.model_view.local_camera.w==4,"resource binding reset persistent globals");
    auto& a=*f.allocations;require(f.transformed.allocation.size()==15360&&f.indexed.base().storage().size()==12288,"source allocation counts");
    require(f.clip.vertices.data()==a.vertices.data()+12288&&f.clip.vertices.size()==3072,"clipping vertices are not the actual source alias");
    f.clip.vertices[3].words[2]=0x12345678;require(a.vertices[12291].words[2]==0x12345678,"clip vertex alias detached");
    for(unsigned i=0;i<16;++i)require(f.clip.sequential_indices[i]==i,"427270 sequential index binding");
    for(unsigned p=0;p<4;++p)require(f.packets[p].base().storage().data()==a.packets[p].data()&&f.packets[p].base().storage().size()==3072,"source packet owner detached");
    for(unsigned p=0;p<2;++p)require(f.spatial[p].records.data()==a.spatial[p].data()&&f.spatial[p].records.size()==512,"source spatial owner detached");
    const auto old=a.vertices.data();allocate_world_polygon_frame(f);
    require(f.transformed.allocation.data()!=old&&f.clip.vertices.data()==f.allocations->vertices.data()+12288,"resource replacement retained old aliases");
    require(f.frame_stamp==65535&&f.spatial_batch_count==173&&f.model_view.local_camera.w==4,"replacement reset persistent globals");
    for(const auto& v:f.allocations->vertices)for(const auto value:v.words)require(value==0,"new42E8D0 allocation not zero");
    WorldPolygonFrame absent;bool rejected=false;try{reset_world_polygon_vertex_cursors(absent);}catch(const FormatError&){rejected=true;}require(rejected,"absent allocation accepted");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"expected original frame corpus");Reader in(argv[1]);const std::array<char,8> magic{'S','G','N','W','F','0','0','1'};in.equal(magic.data(),8,"wrong frame corpus");const auto suites=in.word();require(suites==512,"expected512 frame suites");
    WorldPolygonFrame frame;allocate_world_polygon_frame(frame);auto& allocation=*frame.allocations;
    for(unsigned i=0;i<allocation.vertices.size();++i)allocation.vertices[i].words.fill(0x81230000+i);
    allocation.indexed[7]=&allocation.vertices[103];allocation.clip_headers[6].fill(0xc3);allocation.clip_unused_indices[2]=0x173;
    const auto payload_size=static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(&allocation.packet_lifetimes)-reinterpret_cast<std::uintptr_t>(&allocation));
    const auto allocation_bytes=std::as_bytes(std::span(&allocation,1)).first(payload_size);const std::vector<std::byte> retained(allocation_bytes.begin(),allocation_bytes.end());std::uint64_t calls=0;
    for(unsigned s=0;s<suites;++s){load(frame,in.value<Wire>());const auto steps=in.word();require(steps==15,"expected continued frame operations");
        for(unsigned step=0;step<steps;++step){++calls;const auto operation=in.word(),group=in.word();const auto increments=in.value<std::array<std::uint32_t,4>>();
            for(unsigned p=0;p<4;++p){require(increments[p]<=128&&frame.packets[p].count()+increments[p]<=3072,"frame input append outside allocation");frame.packets[p].publish_current(allocation.packet_lifetimes[p].pointer(*frame.packets[p].current().offset()+increments[p]));}
            switch(operation){case 0:reset_world_polygon_vertex_cursors(frame);break;case 1:reset_world_polygon_groups(frame);break;
                case 2:begin_world_polygon_group(frame,group);break;case 3:finish_world_polygon_group(frame,group);break;
                case 4:publish_world_polygon_totals(frame);break;default:throw FormatError("unknown frame operation");}
            const auto actual=state(frame);const auto label="frame suite "+std::to_string(s)+" step "+std::to_string(step);in.equal(actual.data(),sizeof(actual),label.c_str());
        }
    }
    require(std::equal(retained.begin(),retained.end(),allocation_bytes.begin()),"source reset/group helper cleared retained allocation bytes");
    in.end();allocation_contract();std::cout<<"{\"passed\":true,\"suites\":"<<suites<<",\"full_original_calls\":"<<calls<<",\"compared_bytes\":"<<in.compared<<",\"retained_allocation_bytes\":"<<retained.size()<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
