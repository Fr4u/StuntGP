#include "sgn/retail_world_polygon_producer.hpp"
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr std::size_t clip_offset=0x60000/sizeof(WorldPolygonVertex);
std::size_t current_case{},compared{},undefined_bytes{};bool audited=true;
void require(bool okay,const std::string& why){if(!okay)throw FormatError("pipeline case "+std::to_string(current_case)+": "+why);}
class Reader {
 std::ifstream in_;
public:
 explicit Reader(const char* name):in_(name,std::ios::binary){require(bool(in_),"missing corpus");}
 void bytes(void* target,std::size_t count){require(count<=8*1024*1024,"input extent");if(count)require(bool(in_.read(static_cast<char*>(target),static_cast<std::streamsize>(count))),"truncated corpus");}
 template<class T>T value(){T result{};bytes(&result,sizeof(result));return result;}
 std::uint32_t word(){return value<std::uint32_t>();}
 std::string name(){const auto count=word();require(count>0&&count<64,"model name extent");std::string result(count,'\0');bytes(result.data(),count);return result;}
 void exact(std::span<const std::byte> actual){std::vector<std::byte> expected(actual.size());bytes(expected.data(),expected.size());
  for(std::size_t i=0;i<actual.size();++i){require(actual[i]==expected[i],"exact byte "+std::to_string(i));++compared;}}
 template<class T>void exact(const T& actual){exact(std::as_bytes(std::span(&actual,1)));}
 void masked(std::span<const std::byte> actual,std::span<const std::byte> provenance){
  require(actual.size()==provenance.size(),"provenance extent");std::vector<std::byte> expected(actual.size()),mask(actual.size());bytes(expected.data(),expected.size());bytes(mask.data(),mask.size());
  for(std::size_t i=0;i<actual.size();++i){if(audited)require(provenance[i]==mask[i],"provenance byte "+std::to_string(i));
   if(mask[i]!=std::byte{}){++undefined_bytes;continue;}require(actual[i]==expected[i],"defined byte "+std::to_string(i));++compared;}
 }
 void end(){require(in_.peek()==std::char_traits<char>::eof(),"trailing corpus");}
};
std::uint32_t next(std::uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
void poison(std::span<std::byte> output,std::uint32_t& state){for(auto& value:output)value=static_cast<std::byte>(next(state)&255u);}
template<class T,std::size_t N>void poison(std::span<T,N> output,std::uint32_t& state){poison(std::span<std::byte>(std::as_writable_bytes(output)),state);}
std::pair<bool,std::uint32_t> offset(const void* pointer,const void* base,std::size_t extent,std::size_t stride){
 const auto p=reinterpret_cast<std::uintptr_t>(pointer),b=reinterpret_cast<std::uintptr_t>(base);
 if(!pointer||p<b||p-b>=extent||(p-b)%stride)return {false,0};return {true,static_cast<std::uint32_t>((p-b)/stride)};
}
std::vector<std::byte> source_snapshot(ModelResource& model,unsigned lod){
 std::vector<std::byte> bytes;
 const auto append=[&](std::span<const std::byte> values){bytes.insert(bytes.end(),values.begin(),values.end());};
 append(std::as_bytes(model.source_attribute_table()));append(std::as_bytes(model.source_normals()));append(std::as_bytes(model.source_corner_indices()));
 for(unsigned part=0;part<model.part_count(lod);++part){append(std::as_bytes(model.source_coordinates(lod,part)));append(std::as_bytes(model.source_vertices(lod,part)));}
 return bytes;
}
std::array<std::uint32_t,9> canonical(const WorldPolygonPacket& packet,ModelResource& model,
 const void* coordinate_anchor,std::span<WorldPolygonHeader> output_headers,std::span<WorldPolygonAttribute> output_attributes,
 const std::array<std::uint16_t,16>& sequential,std::span<WorldPolygonVertex> allocation){
 std::array<std::uint32_t,9> result{};
 const auto headers=model.source_attribute_table();const auto indices=model.source_corner_indices();
 if(packet.header){
  const auto source=WorldPolygonHeaderView(headers).find(packet.header),clipped=WorldPolygonHeaderView(output_headers).find(packet.header);
  require(source||clipped,"header lost owner");result[0]=source?1u:2u;result[1]=static_cast<std::uint32_t>(source?*source:*clipped);
 }
 if(packet.indices.data()){
  const auto source=offset(packet.indices.data(),indices.data(),indices.size_bytes(),2),clipped=offset(packet.indices.data(),sequential.data(),sizeof(sequential),2);
  require(source.first||clipped.first,"indices lost owner");result[2]=source.first?1u:2u;result[3]=source.first?source.second:clipped.second;
 }
 if(packet.attributes.bytes().data()){
  const auto clipped=offset(packet.attributes.bytes().data(),output_attributes.data(),output_attributes.size_bytes(),16);
  if(clipped.first){result[4]=2;result[5]=clipped.second;}
  else {
   // Every PMD part borrows one actual coordinate allocation. Offsets from
   // part0 preserve authored aliases without exposing a new mutable owner.
   const auto p=reinterpret_cast<std::uintptr_t>(packet.attributes.bytes().data()),b=reinterpret_cast<std::uintptr_t>(coordinate_anchor);
   const auto distance=p>=b?p-b:b-p;require(distance<0x1000000&&distance%16==0,"coordinate offset outside source owner domain");
   result[4]=1;result[5]=p>=b?static_cast<std::uint32_t>(distance/16):0u-static_cast<std::uint32_t>(distance/16);
  }
 }
 if(packet.vertices.data()){
  const auto source=offset(packet.vertices.data(),allocation.data(),allocation.size_bytes(),32);require(source.first,"transformed vertices lost shared allocation");
  result[6]=1;result[7]=source.second;
 }
 result[8]=packet.inside_mask;return result;
}
}

int main(int argc,char**argv)try{
 require(argc==3||(argc==4&&std::string(argv[3])=="--without-audit"),"expected retail root/corpus and optional --without-audit");audited=argc==3;
 Reader input(argv[2]);const auto magic=input.value<std::array<char,8>>();require(std::string(magic.data(),magic.size())=="SGNPL601","corpus version");
 const auto model_count=input.word();require(model_count==20,"requires all twenty car models");AssetStore assets(argv[1]);ModelResources resources(assets,argv[1]);
 std::uint64_t published{},rejected{},source_vertices{},clipped_vertices{};
 for(unsigned m=0;m<model_count;++m){
  const auto name=input.name();require(name=="car"+std::to_string(m+1),"car inventory order");
  const auto identity=resources.load(name);auto& model=resources.at(*resources.resolve(identity));
  const auto main_count=input.word(),header_capacity=input.word(),vertex_capacity=input.word(),packet_capacity=input.word();auto seed=input.word();
  require(main_count<clip_offset&&header_capacity>0&&header_capacity<65536&&vertex_capacity==header_capacity*16&&packet_capacity==header_capacity+2,"allocation bounds");
  std::vector<WorldPolygonVertex> allocation(clip_offset+vertex_capacity);
  std::vector<WorldPolygonHeader> headers(header_capacity);std::vector<WorldPolygonAttribute> attributes(vertex_capacity);std::array<std::uint16_t,16> unused;
  poison(std::span(allocation),seed);poison(std::span(headers),seed);poison(std::span(attributes),seed);poison(std::span(unused),seed);
  std::array<std::vector<WorldPolygonPacket>,4> packets;
  for(auto& list:packets){list.resize(packet_capacity);for(auto& packet:list)packet.inside_mask=next(seed);}
  const auto initial_gap=std::vector<WorldPolygonVertex>(allocation.begin()+main_count,allocation.begin()+clip_offset);
  auto clipped=std::span(allocation).subspan(clip_offset);WorldPolygonClipStorage storage{attributes,headers,unused,clipped};initialize_world_polygon_clip_storage(storage);
  std::vector<std::array<std::uint8_t,8>> header_tags(header_capacity);std::vector<std::array<std::uint8_t,16>> attribute_tags(vertex_capacity);
  std::vector<std::array<std::uint8_t,32>> vertex_tags(vertex_capacity);WorldPolygonClipAudit audit{header_tags,attribute_tags,vertex_tags};
  const auto* coordinate_anchor=model.source_coordinates(model.source_lod_index(0),0).data();
  const auto cases=input.word();require(cases==16,"requires four view fixtures for each source LOD slot");
  for(unsigned c=0;c<cases;++c,++current_case){
   const auto source_lod=input.word(),clipping=input.word();require(source_lod==c/4&&clipping==static_cast<unsigned>(c%4!=3),"source LOD/view selectors");
   const auto lod=model.source_lod_index(source_lod);const auto matrix=input.value<std::array<float,16>>();const auto projection=input.value<ProjectionParameters>();
   const auto local=input.value<VehicleVector>();const auto viewport_words=input.value<std::array<float,4>>();
   const auto readonly_before=source_snapshot(model,lod);
   ViewportState viewport;viewport.center_x=viewport_words[0];viewport.center_y=viewport_words[1];viewport.scale_x=viewport_words[2];viewport.scale_y=viewport_words[3];
   std::array<WorldPolygonProducerList,4> lists;std::array<std::unique_ptr<WorldPolygonPacketStorageLifetime>,4> packet_lifetimes;
   for(unsigned i=0;i<4;++i){packet_lifetimes[i]=std::make_unique<WorldPolygonPacketStorageLifetime>(packets[i]);lists[i].publish_base(packet_lifetimes[i]->pointer());lists[i].copy_base_to_current();}std::size_t cursor=0;
   for(unsigned part=0;part<model.part_count(lod);++part){
    if(model.part(lod,part).flags!=1)continue;
    auto source=world_polygon_part_source(model,lod,part);require(source.vertices.size()<=main_count-cursor,"main transformed allocation");
    auto destination=std::span(allocation).subspan(cursor,source.vertices.size());
    if(clipping){transform_world_vertices_clipped(source.vertices,destination,matrix,projection);emit_world_polygons_clipped(source,destination,local,lists[0],lists[1]);}
    else{transform_world_vertices_unclipped(source.vertices,destination,matrix,projection);emit_world_polygons_unclipped(source,destination,local,lists[2],lists[3]);}
    cursor+=source.vertices.size();
   }
   std::array<std::span<WorldPolygonPacket>,7> clip_lists{};clip_lists[5]=lists[0].prefix();clip_lists[6]=lists[1].prefix();
   float far=500000.0f;clip_world_polygon_lists(clip_lists,storage,projection.clip_near,far,audited?&audit:nullptr);
   project_world_polygon_vertices({},clipped.first(storage.vertex_count),std::span(allocation).first(cursor),viewport);
   require(source_snapshot(model,lod)==readonly_before,"producer/clip/project mutated PMD geometry, material or UV owner");
   require(std::memcmp(initial_gap.data(),allocation.data()+main_count,initial_gap.size()*sizeof(WorldPolygonVertex))==0,"unused main/clipped allocation gap overwritten");
   const std::array<std::uint32_t,9> counts{lists[0].count(),lists[1].count(),lists[2].count(),lists[3].count(),static_cast<std::uint32_t>(cursor),storage.header_count,storage.attribute_count,storage.vertex_count,std::bit_cast<std::uint32_t>(far)};
   input.exact(counts);source_vertices+=cursor;clipped_vertices+=storage.vertex_count;
   for(unsigned i=0;i<4;++i){published+=lists[i].count();for(unsigned j=0;j<packets[i].size();++j){
    if(j<lists[i].count()&&!packets[i][j].header)++rejected;
    input.exact(canonical(packets[i][j],model,coordinate_anchor,headers,attributes,storage.sequential_indices,allocation));
   }}
   input.exact(std::as_bytes(std::span(allocation).first(main_count)));
   input.masked(std::as_bytes(std::span(headers)),std::as_bytes(std::span(header_tags)));
   input.masked(std::as_bytes(std::span(attributes)),std::as_bytes(std::span(attribute_tags)));
   input.masked(std::as_bytes(clipped),std::as_bytes(std::span(vertex_tags)));input.exact(unused);input.exact(storage.sequential_indices);
  }
 }
 input.end();require(current_case==320&&published>0&&clipped_vertices>0,"empty composed geometry coverage");
 std::cout<<"{\"passed\":true,\"models\":"<<model_count<<",\"cases\":"<<current_case<<",\"published_packets\":"<<published
 <<",\"rejected_packets\":"<<rejected<<",\"source_vertices\":"<<source_vertices<<",\"clipped_vertices\":"<<clipped_vertices
 <<",\"compared_defined_bytes\":"<<compared<<",\"source_undefined_bytes\":"<<undefined_bytes<<",\"audit_enabled\":"<<(audited?"true":"false")<<"}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
