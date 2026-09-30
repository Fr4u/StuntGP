#include "sgn/retail_frontend_language_selection.hpp"
#include "sgn/assets.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned state_size=0x900c,font_size=1716;
using State=std::array<std::uint32_t,(state_size+font_size)/4>;
template<class T>void read(std::ifstream& f,T& value){if(!f.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("short frontend language corpus");}
Bytes bytes(std::ifstream& f,unsigned size){if(size>8*1024*1024)throw std::runtime_error("frontend resource bound");Bytes b(size);if(!f.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("short frontend resource");return b;}
constexpr std::array<std::pair<unsigned,unsigned>,8> blocks{{{0x5e37b8,0x128},{0x5ff8c4,0x404},{0x5ffcc8,0x6000},
 {0x605cc8,0xb70},{0x606838,0x24},{0x606860,0x1f40},{0x6087a0,8},{0x47b1a0,4}}};
unsigned offset(unsigned address){unsigned result=0;for(auto [a,n]:blocks){if(address>=a&&address<a+n)return (result+address-a)/4;result+=n;}throw std::runtime_error("unknown language owner");}
FrontendLanguageCatalogViews catalog(State& s){return {
 std::span<std::uint32_t,frontend_language_catalog_words>(s.data()+offset(0x5e37b8),frontend_language_catalog_words),
 std::span<std::uint32_t,frontend_text_count>(s.data()+offset(0x605cc8),frontend_text_count),
 std::span<std::uint32_t,9>(s.data()+offset(0x606838),9),s[offset(0x6087a0)]};}
FrontendLanguageSelectionViews selection(State& s){return {catalog(s),
 std::span<std::uint8_t,0x404>(reinterpret_cast<std::uint8_t*>(s.data()+offset(0x5ff8c4)),0x404),
 std::span<std::uint8_t,0x6000>(reinterpret_cast<std::uint8_t*>(s.data()+offset(0x5ffcc8)),0x6000),
 std::span<std::uint32_t,2000>(s.data()+offset(0x606860),2000),s[offset(0x47b1a0)]};}
FrontendFontMetricsViews fonts(State& s){auto* p=s.data()+state_size/4;return {
 std::span<std::uint32_t,322>(p,322),std::span<std::uint8_t,160>(reinterpret_cast<std::uint8_t*>(p+322),160),
 std::span<std::uint8_t,256>(reinterpret_cast<std::uint8_t*>(p+362),256),p[426],p[427],p[428]};}
std::span<const std::uint8_t> resource(const Archive& archive,std::string_view name){
 const auto found=std::find_if(archive.entries().begin(),archive.entries().end(),[&](const auto& e){return canonical(e.name)==name;});
 if(found==archive.entries().end())throw std::runtime_error("actual frontend source archive entry missing");return archive.data(*found);}
void compare(const State& actual,const State& expected,unsigned group,unsigned step){
 for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i]){
  std::cerr<<"group "<<group<<" step "<<step<<" word "<<i<<" actual "<<std::hex<<actual[i]<<" expected "<<expected[i]<<std::dec<<'\n';
  throw std::runtime_error("frontend language retained bank mismatch");}}
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("expected frontend selection corpus");std::ifstream file(argv[1],std::ios::binary);
 std::array<char,8> magic{};read(file,magic);if(std::string_view(magic.data(),8)!="SGNFLNG1")throw std::runtime_error("frontend selection corpus magic");
 unsigned groups{};read(file,groups);if(groups==0||groups>128)throw std::runtime_error("frontend selection group bound");
 unsigned size{};read(file,size);Archive setup(bytes(file,size));read(file,size);Archive font_archive(bytes(file,size));
 const auto language=resource(setup,"setup\\languagepc.csv");
 FrontendFontInputs input{resource(font_archive,"graphics24\\fonts\\index.txt"),resource(font_archive,"graphics24\\fonts\\i32.tga"),
  resource(font_archive,"graphics24\\fonts\\i32_b05.tga"),resource(font_archive,"graphics24\\fonts\\i32_b025.tga")};
 std::uint64_t calls=0,continuations=0,font_calls=0,catalog_calls=0,selection_calls=0;
 for(unsigned group=0;group<groups;++group){
  State actual{};read(file,actual);unsigned steps{};read(file,steps);if(steps==0||steps>128)throw std::runtime_error("frontend selection sequence bound");
  for(unsigned step=0;step<steps;++step){
   unsigned op{},argument{};State before{},after{};std::array<std::uint32_t,3> expected{},result{};
   read(file,op);read(file,argument);read(file,before);read(file,after);read(file,expected);compare(actual,before,group,step);
   if(op==0){initialize_frontend_font_metrics(fonts(actual),input,std::bit_cast<std::int32_t>(argument));++font_calls;}
   else if(op==1){(void)initialize_frontend_language_catalog(catalog(actual),{language,1});++catalog_calls;}
   else if(op==2){const auto r=select_frontend_language_440c40(selection(actual),{language,1},fonts(actual),argument);result={std::bit_cast<std::uint32_t>(r.column),r.cursor,r.unresolved};++selection_calls;}
   else throw std::runtime_error("frontend unknown operation");
   compare(actual,after,group,step);if(result!=expected)throw std::runtime_error("frontend source return/cursor mismatch");
   if(op==2&&result[0]!=0xffffffff){
    const auto view=selection(actual);const FrontendLanguageStrings strings{view.arena,view.catalog.text_handles};
    for(unsigned id=0;id<frontend_text_count;++id){const auto identity=frontend_language_text_441170(strings,id);const auto text=resolve_frontend_language_text(strings,identity);
     if(identity.value){if(text.empty()||text.back()!=0||text.data()!=view.arena.data()+identity.value-1)throw std::runtime_error("frontend text identity/borrow mismatch");}
     else if(!text.empty())throw std::runtime_error("frontend null text identity mismatch");}
   }
   ++calls;continuations+=step!=0;
  }
 }
 if(file.peek()!=std::ifstream::traits_type::eof())throw std::runtime_error("trailing frontend selection corpus");
 std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"continuations\":"<<continuations<<",\"font_calls\":"<<font_calls
 <<",\"catalog_calls\":"<<catalog_calls<<",\"selection_calls\":"<<selection_calls<<",\"compared_state_bytes\":"<<2*calls*(state_size+font_size)<<",\"mismatches\":0}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
