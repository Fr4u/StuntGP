#include "sgn/retail_vehicle_strip.hpp"
#include <bit>
#include <iostream>
using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Binding final:VehicleStripBinding {
 VehicleStripGeometry* live{};const VehicleStripHeaders* headers{};unsigned calls{};
 void bind(VehicleStripGeometry& geometry) override{
  require(headers&&headers->items[0].field_00==97,"header reset preceded resource publication");
  live=&geometry;++calls;
 }
};
}
int main(){try{
 VehicleStripGeometry g;VehicleStripHeaders h;Binding binding;binding.headers=&h;
 for(unsigned pass=0;pass<3;++pass){
  const auto w=0x7fc0abcd+pass;g.normals[0].w=std::bit_cast<float>(w);g.texture_id=0xfedcba90+pass;
  g.corner_indices[0]=99;g.polygons[127].flags=0xff;
  for(auto& header:h.items)header={97,98,99};
  initialize_vehicle_strip(g,h,binding);
  require(binding.live==&g&&binding.calls==pass+1,"resource publication not live");
  require(binding.live->corners[511].vertex==511&&binding.live->polygons[127].normal==127,"published resource is a stale initial snapshot");
  require(g.texture_id==0xfedcba90+pass,"shared texture identity erased");
  for(auto& normal:g.normals)require(std::bit_cast<std::uint32_t>(normal.w)==w,"opaque normal W lost across reuse");
  for(auto& header:h.items)require(header.field_00==0&&header.field_04==0&&header.field_08==0,"header not cleared");
  require(g.corner_indices[0]==0&&g.polygon_count==0&&g.corner_count==0&&g.vertex_count==0,"source inactive draw state changed");
 }
 const auto& a=vehicle_strip_transform();const auto& b=vehicle_strip_transform();
 require(&a==&b&&a.forward.z==1&&a.scale.x==1,"source transform identity changed");
 std::cout<<"{\"passed\":true,\"reuse_sequences\":3,\"live_publication\":true,\"opaque_W_preserved\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
