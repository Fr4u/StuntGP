#include "sgn/retail_font_resources.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>void read(std::istream& f,T& value){require(bool(f.read(reinterpret_cast<char*>(&value),sizeof(value))),"short font corpus");}
Bytes bytes(std::istream& f,std::size_t count){require(count<=16*1024*1024,"font corpus payload bound");Bytes value(count);require(bool(f.read(reinterpret_cast<char*>(value.data()),static_cast<std::streamsize>(count))),"short font corpus bytes");return value;}
std::uint32_t word(std::span<const std::uint8_t> b,std::size_t at){std::uint32_t value{};require(at+4<=b.size(),"font snapshot read bound");std::memcpy(&value,b.data()+at,4);return value;}
void put(Bytes& b,std::uint32_t value){const auto at=b.size();b.resize(at+4);std::memcpy(b.data()+at,&value,4);}
void compare(std::span<const std::uint8_t> a,std::span<const std::uint8_t> b,const char* label){
 require(a.size()==b.size(),"font source size differs");
 for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i]){std::cerr<<label<<" byte "<<i<<" actual "<<unsigned(a[i])<<" expected "<<unsigned(b[i])<<'\n';throw std::runtime_error(label);}}
struct State {
 SourceAllocationBank bank;
 SourceTgaDecodeState decode;
 RetailFontResources fonts;
 std::array<std::uint32_t,17> scalars{};
 std::array<std::uint32_t,255*7> materials{};
 std::array<std::uint8_t,255> transparency{};
 std::array<std::uint8_t,8160> names{};
 std::array<std::uint32_t,16> reload{};
 explicit State(std::span<const std::uint8_t> b){
  bank.initialize_42eab0(); //explicit receiving fixture source boot, not hidden producer initialization
  require(b.size()==30247,"font snapshot schema");std::size_t at=0;
  for(unsigned f=0;f<2;++f)for(unsigned i=0;i<retail_font_record_words;++i){fonts.source_word(f,i*4)=word(b,at);at+=4;}
  auto copy=[&](auto& target){std::memcpy(target.data(),b.data()+at,sizeof(target));at+=sizeof(target);};
  copy(fonts.index);copy(fonts.glyph_map);copy(scalars);copy(materials);copy(transparency);copy(names);copy(reload);
  fonts.current=scalars[0];fonts.count=scalars[1];fonts.digit_advance_bits=scalars[2];require(scalars[16]==0,"font group must start before scratch allocation");
 }
 Bytes snapshot() const{
  Bytes result;for(unsigned f=0;f<2;++f)for(unsigned i=0;i<retail_font_record_words;++i)put(result,fonts.source_word(f,i*4));
  auto copy=[&](const auto& source){const auto* first=reinterpret_cast<const std::uint8_t*>(source.data());result.insert(result.end(),first,first+sizeof(source));};
  copy(fonts.index);copy(fonts.glyph_map);auto s=scalars;s[0]=fonts.current;s[1]=fonts.count;s[2]=fonts.digit_advance_bits;s[16]=static_cast<std::uint32_t>(fonts.scratch_allocations());
  copy(s);copy(materials);copy(transparency);copy(names);copy(reload);return result;
 }
};
struct Files final:MaterialFileSource {
 AssetStore assets;RetailMaterialFileSystem actual;SourceAllocationBank* bank{};std::vector<std::string> calls;
 explicit Files(const std::filesystem::path& root):assets(root),actual(assets,root){}
 std::uint32_t source_file_size(const MaterialResourcePath& path) override{
  calls.push_back("size:"+path.relative_name);return actual.source_file_size(path);
 }
 MaterialFileRead read_source_file(const MaterialResourcePath& path,std::span<std::uint8_t> target) override{
  calls.push_back("read:"+path.relative_name);
  if(path.relative_name!="\\Fonts\\INDEX.TXT"){
   require(bank!=nullptr,"font file receiving bank missing");
   const auto records=bank->source_records();const auto address=reinterpret_cast<std::uintptr_t>(target.data());
   const auto found=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.aligned==address&&r.raw!=0;});
   require(found!=records.end()&&found->requested_bytes==target.size()&&found->category==0,
    "actual font file read did not target a source bank allocation");
  }
  return actual.read_source_file(path,target);
 }
};
struct ExpectedFontAtlas {unsigned font{};Bytes rgb;};
struct Upload {unsigned token{};Bytes pixels;};
//External4325E0 completion fixture only. It consumes actual source COM pixels
//and source-normalized texture tokens, preserving the full retained table.
//This is not a D3D12 backend or native qualification of4325E0 COM calls.
struct Target final:FontAtlasUploadTarget,FontAtlasObserver,FontBankObserver {
 State& state;const std::vector<ExpectedFontAtlas>& images;const std::vector<Upload>& uploads;
 unsigned observed{},uploaded{},transparency_reads{};
 SourceAllocationRef atlas,outer,input;std::vector<std::uint32_t> sites;unsigned allocations{},releases{};
 std::uint32_t expected_cursor_end{};
 void observe(const FontBankEvent& event,const SourceAllocationBank& bank) override{
  require(&bank==&state.bank,"observer received a different source bank");sites.push_back(event.source_site);
  require(event.category==0,"font producer changed source category");
  if(event.kind==FontBankEventKind::allocation){
   require(bool(event.allocation),"source corpus unexpectedly exhausted bank");++allocations;
   const auto bytes=bank.bytes(event.allocation);
   require(bytes.size()==event.requested_bytes&&reinterpret_cast<std::uintptr_t>(bytes.data())%32==0,
    "observer allocation size/alignment");
   require(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==0;}),"new source payload not zero");
   if(event.source_site==0x428e75)outer=event.allocation;
   if(event.source_site==0x4623f9)input=event.allocation;
  }else if(event.kind==FontBankEventKind::read){
   require(event.allocation==input&&state.decode.file==input,"decoder file global did not receive actual nested allocation");
   const auto raw=bank.bytes(input);require(raw.size()>=18,"actual font input header extent");
   const auto width=std::uint32_t(raw[12])|(std::uint32_t(raw[13])<<8);
   const auto height=std::uint32_t(raw[14])|(std::uint32_t(raw[15])<<8);
   const auto extent=std::uint64_t(width)*height*3;
   require(18u+raw[0]+extent<=raw.size(),"actual font file pixel extent");
   expected_cursor_end=static_cast<std::uint32_t>(18u+raw[0]+extent);
  }else if(event.kind==FontBankEventKind::decoded){
   require(event.allocation==outer&&state.decode.file==input&&state.decode.pixel_cursor_allocation==input,
    "decoder consumed/published different actual bank allocations");
   const auto raw=bank.bytes(input),rgb=bank.bytes(outer);
   const auto width=std::uint32_t(raw[12])|(std::uint32_t(raw[13])<<8);
   const auto height=std::uint32_t(raw[14])|(std::uint32_t(raw[15])<<8);
   const auto extent=std::size_t(width)*height*3;
   require(state.scalars[12]==width&&state.scalars[13]==height&&state.scalars[14]==width&&state.scalars[15]==height&&
    state.decode.pixel_depth==24&&state.decode.pixel_cursor_offset==expected_cursor_end,"decoded globals disagree with actual file header/end");
   require(rgb.size()==raw.size()&&extent<=rgb.size(),"RGB owner must retain its full source file-size allocation");
   require(std::all_of(rgb.begin()+static_cast<std::ptrdiff_t>(extent),rgb.end(),[](auto b){return b==0;}),
    "decoder wrote beyond its pixel extent into the zero-filled outer tail");
  }else if(event.kind==FontBankEventKind::release){
   bool rejected=false;try{(void)bank.bytes(event.allocation);}catch(const SourceAllocationError&){rejected=true;}
   require(rejected,"released source payload is still readable");++releases;
  }else if(event.kind==FontBankEventKind::packed)atlas=event.allocation;
 }
 Target(State& s,const std::vector<ExpectedFontAtlas>& i,const std::vector<Upload>& u):state(s),images(i),uploads(u){}
 void packed_atlas(std::size_t font,std::span<const std::uint8_t> rgb) override{
  require(state.bank.bytes(atlas).data()==rgb.data(),"packed atlas observer got a shadow vector");
  require(observed<images.size()&&images[observed].font==font,"font packed atlas callback order");
  require(state.fonts.current==font+1&&state.fonts.count==font,"font callback selection/count order");
  compare(rgb,images[observed].rgb,"font packed RGB differs from original428C00");++observed;
 }
 std::uint8_t& current_transparency() override{
  require(state.scalars[8]<state.transparency.size(),"font transparency cursor range");++transparency_reads;return state.transparency[state.scalars[8]];
 }
 std::uint32_t upload_font_atlas(PreparedMaterialPixels p) override{
  require(reinterpret_cast<const std::uint8_t*>(p.words.data())==state.bank.bytes(atlas).data(),
   "upload did not consume the actual bank atlas in place");
  require(uploaded<uploads.size()&&p.width==256&&p.height==256,"font upload count/dimensions");
  require(state.fonts.current==uploaded/2+1&&state.fonts.count==uploaded/2,"font upload occurred after count publication");
  compare({reinterpret_cast<const std::uint8_t*>(p.words.data()),p.words.size_bytes()},uploads[uploaded].pixels,"font converted words differ from original430280/4327C0");
  const auto index=state.scalars[3];require(index<255,"font material fixture cursor range");state.materials[index*7]=uploads[uploaded].token;
  ++state.scalars[3];++state.scalars[7];++uploaded;return index;
 }
};
unsigned owner_contract(){
 RetailFontResources owner;std::set<const std::uint32_t*> addresses;
 for(unsigned f=0;f<2;++f)for(unsigned i=0;i<retail_font_record_words;++i){auto& v=owner.source_word(f,i*4);require(addresses.insert(&v).second,"font record words alias incorrectly");v=static_cast<std::uint32_t>(f*retail_font_record_words+i);}
 auto metrics=owner.metrics();
 for(unsigned f=0;f<2;++f){
  require(&metrics.records[f*161]==&owner.source_word(f,8),"font width has duplicate owner");
  for(unsigned glyph=0;glyph<160;++glyph)require(&metrics.records[f*161+glyph+1]==&owner.source_word(f,0x28+44*glyph),"font advance has duplicate owner");
 }
 require(&metrics.current==&owner.current&&&metrics.count==&owner.count&&&metrics.digit_advance_bits==&owner.digit_advance_bits,"font scalar owner duplicated");
 require(metrics.index.data()==owner.index.data()&&metrics.glyph_map.data()==owner.glyph_map.data(),"font map/index owner duplicated");
 owner.source_word(1,0x28)=123;require(metrics.records[162]==123,"font record update not visible in metrics");
 metrics.records[162]=456;require(owner.source_word(1,0x28)==456,"font metric update not visible in record");
 unsigned rejects=0;auto reject=[&](auto op){try{op();}catch(const FormatError&){++rejects;return;}throw std::runtime_error("invalid font owner accepted");};
 reject([&]{(void)owner.source_word(2,0);});reject([&]{(void)owner.source_word(0,retail_font_record_bytes);});reject([&]{(void)owner.source_word(0,1);});
 return rejects;
}
}
int main(int argc,char** argv){try{
 require(argc==3,"expected font corpus and actual retail root");const auto rejects=owner_contract();
 std::ifstream file(argv[1],std::ios::binary);std::array<char,8> magic{};read(file,magic);require(std::string_view(magic.data(),8)=="SGNFONT1","font corpus magic");
 unsigned length{};read(file,length);const auto embedded=bytes(file,length);
 compare(embedded,read_file(std::filesystem::path(argv[2])/"wads"/"fonts.wad"),"actual font WAD differs from source oracle corpus");
 Files files(argv[2]);unsigned groups{};read(file,groups);require(groups>0&&groups<=32,"font group count");
 unsigned calls=0,uploads=0,atlases=0;std::uint64_t compared_bytes=0;
 for(unsigned group=0;group<groups;++group){
  State actual(bytes(file,30247));files.bank=&actual.bank;unsigned steps{};read(file,steps);require(steps>0&&steps<=16,"font step count");
  for(unsigned step=0;step<steps;++step){
   std::int32_t width{};unsigned enabled{};read(file,width);read(file,enabled);const auto before=bytes(file,30247),after=bytes(file,30247);
   compare(actual.snapshot(),before,"font retained before snapshot differs");
   unsigned count{};read(file,count);require(count<=2,"font RGB count");std::vector<ExpectedFontAtlas> images(count);
   for(auto& image:images){read(file,image.font);image.rgb=bytes(file,256*256*3);}
   read(file,count);require(count<=4,"font upload count");std::vector<Upload> expected(count);
   for(auto& upload:expected){read(file,upload.token);upload.pixels=bytes(file,256*256*2);}
   Target target(actual,images,expected);files.calls.clear();
   auto& s=actual.scalars;
   initialize_retail_font_resources(actual.fonts,{actual.bank,actual.decode,&target},files,{s[12],s[13],s[14],s[15]},
    {actual.names,s[10],actual.reload,s[11],s[4]},"D:\\Game\\Graphics24\\",width,enabled,target,&target);
   require(target.observed==images.size()&&target.uploaded==expected.size(),"font source call count differs");
   require(target.transparency_reads==(enabled==1?images.size():0),"font transparency accessed on wrong branch");
   compare(actual.snapshot(),after,"font retained after snapshot differs");
   std::vector<std::string> expected_calls{"read:\\Fonts\\INDEX.TXT"};
   for(unsigned i=0;i<images.size();++i){const std::string name=i==1?"I32_b025":width>=1152?"I32":"I32_b05";const std::string path="\\Fonts\\"+name+".TGA";expected_calls.push_back("size:"+path);expected_calls.push_back("size:"+path);expected_calls.push_back("read:"+path);}
   require(files.calls==expected_calls,"font typed file request order differs");
   std::vector<std::uint32_t> expected_sites;
   for(std::size_t i=0;i<images.size();++i){
    for(auto site:{0x428e75u,0x4623f9u,0x462408u,0x46241au,0x462425u,0x428e91u,0x428c58u,0x4327e0u})expected_sites.push_back(site);
    if(enabled==1){expected_sites.push_back(0x43280d);expected_sites.push_back(0x432832);}
    expected_sites.push_back(0x428eb7);expected_sites.push_back(0x428ebe);
   }
   require(target.sites==expected_sites&&target.allocations==4*images.size()&&target.releases==3*images.size(),
    "actual source allocation/free sequence differs");
   require(actual.bank.live_count()==actual.fonts.scratch_allocations()&&
    actual.bank.source_words().ordinary_bytes==actual.fonts.scratch_allocations()*1060&&
    actual.bank.source_words().next_ordinal==actual.fonts.scratch_allocations()*4,"actual retained scratch bank history");
   for(unsigned i=0;i<images.size();++i){const auto ref=actual.fonts.source_scratch_allocation(i,actual.bank);
    require(actual.bank.bytes(ref).size()==1024,"actual source scratch ID is not bound to its payload");}
   bool dead=false;try{(void)actual.bank.bytes(actual.decode.file);}catch(const SourceAllocationError&){dead=true;}
   require(dead&&actual.decode.file==actual.decode.pixel_cursor_allocation&&actual.decode.pixel_depth==24&&
    actual.decode.pixel_cursor_offset==target.expected_cursor_end,
    "source decoder globals must retain the freed allocation generation");
   ++calls;uploads+=target.uploaded;atlases+=target.observed;
   compared_bytes+=2*30247ull+target.uploaded*256*256*2ull+target.observed*256*256*3ull;
   std::cout<<"group="<<group<<" step="<<step<<" width="<<width<<" passed\n";
  }
 }
 require(file.peek()==std::ifstream::traits_type::eof(),"font trailing corpus data");
 std::cout<<"{\"passed\":true,\"source_bank_observed\":true,\"actual_file_adapter\":true,\"calls\":"<<calls<<",\"packed_atlases\":"<<atlases<<",\"upload_boundaries\":"<<uploads<<",\"compared_bytes\":"<<compared_bytes<<",\"owner_rejections\":"<<rejects<<",\"mismatches\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
