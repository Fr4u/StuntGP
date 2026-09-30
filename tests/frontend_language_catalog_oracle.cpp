#include "sgn/retail_frontend_language.hpp"
#include "sgn/assets.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned state_size=0x900c;
using State=std::array<std::uint32_t,state_size/4>;
template<class T>void read(std::ifstream& f,T& v){if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("short language catalog corpus");}
Bytes bytes(std::ifstream& f,unsigned size){if(size>4*1024*1024)throw std::runtime_error("catalog payload bound");Bytes b(size);if(!f.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("short catalog payload");return b;}
constexpr std::array<std::pair<unsigned,unsigned>,8> blocks{{{0x5e37b8,0x128},{0x5ff8c4,0x404},{0x5ffcc8,0x6000},
 {0x605cc8,0xb70},{0x606838,0x24},{0x606860,0x1f40},{0x6087a0,8},{0x47b1a0,4}}};
unsigned offset(unsigned address){unsigned result=0;for(auto [a,n]:blocks){if(address>=a&&address<a+n)return (result+address-a)/4;result+=n;}throw std::runtime_error("unknown catalog owner");}
FrontendLanguageCatalogViews view(State& s){return {
 std::span<std::uint32_t,frontend_language_catalog_words>(s.data()+offset(0x5e37b8),frontend_language_catalog_words),
 std::span<std::uint32_t,frontend_text_count>(s.data()+offset(0x605cc8),frontend_text_count),
 std::span<std::uint32_t,9>(s.data()+offset(0x606838),9),s[offset(0x6087a0)]};}
void compare(const State& actual,const State& expected,unsigned group,unsigned step){
 for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i]){
  std::cerr<<"group "<<group<<" step "<<step<<" word "<<i<<" actual "<<std::hex<<actual[i]<<" expected "<<expected[i]<<std::dec<<'\n';
  throw std::runtime_error("catalog retained bank mismatch");}}
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("expected language catalog corpus");std::ifstream file(argv[1],std::ios::binary);
 std::array<char,8> magic{};read(file,magic);if(std::string_view(magic.data(),8)!="SGNFCAT1")throw std::runtime_error("catalog corpus magic");
 unsigned groups{};read(file,groups);if(groups==0||groups>4096)throw std::runtime_error("catalog group bound");
 std::uint64_t calls=0,continuations=0,compared_bytes=0,resource_bytes=0;
 for(unsigned group=0;group<groups;++group){
  unsigned wad_size{};read(file,wad_size);Archive archive(bytes(file,wad_size));
  const auto found=std::find_if(archive.entries().begin(),archive.entries().end(),[](const auto& e){return canonical(e.name)=="setup\\languagepc.csv";});
  if(found==archive.entries().end())throw std::runtime_error("catalog source archive entry missing");const auto input=archive.data(*found);
  unsigned input_size{};read(file,input_size);const auto expected_input=bytes(file,input_size);
  if(!std::ranges::equal(input,expected_input))throw std::runtime_error("actual native archive resource differs from original loaded blob");resource_bytes+=input.size();
  State actual{};read(file,actual);unsigned steps{};read(file,steps);if(steps==0||steps>32)throw std::runtime_error("catalog sequence bound");
  for(unsigned step=0;step<steps;++step){
   State before{},after{};unsigned identity{},expected_cursor{};read(file,before);read(file,after);read(file,identity);read(file,expected_cursor);
   compare(actual,before,group,step);const auto cursor=initialize_frontend_language_catalog(view(actual),{input,identity});compare(actual,after,group,step);
   if(cursor!=expected_cursor)throw std::runtime_error("catalog source cursor mismatch");
   ++calls;continuations+=step!=0;compared_bytes+=2*state_size;
  }
 }
 if(file.peek()!=std::ifstream::traits_type::eof())throw std::runtime_error("trailing language catalog corpus");
 std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"continuations\":"<<continuations
 <<",\"compared_state_bytes\":"<<compared_bytes<<",\"resource_bytes\":"<<resource_bytes<<",\"mismatches\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
