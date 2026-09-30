#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <type_traits>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned source_state_size=0x900c;
using SourceState=std::array<std::uint32_t,source_state_size/4>;
constexpr std::array<std::pair<unsigned,unsigned>,8> source_blocks{{
    {0x5e37b8,0x128},{0x5ff8c4,0x404},{0x5ffcc8,0x6000},{0x605cc8,0xb70},
    {0x606838,0x24},{0x606860,0x1f40},{0x6087a0,8},{0x47b1a0,4}}};
constexpr std::array<std::pair<unsigned,unsigned>,4> owned_blocks{{
    {0x5e37b8,0x128},{0x605cc8,0xb70},{0x606838,0x24},{0x6087a0,4}}};
static_assert(!std::is_copy_constructible_v<FrontendSourceStorage> && !std::is_move_constructible_v<FrontendSourceStorage>);
std::uint64_t checks{},atomic_rejections{},compared_bytes{},cold_guard_bytes{};
void require(bool value,const char* message){if(!value)throw FormatError(message);++checks;}
template<class T>void read(std::ifstream& f,T& v){if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw FormatError("short catalog binding corpus");}
Bytes read_bytes(std::ifstream& file,unsigned size){
    require(size<=4*1024*1024,"catalog binding payload limit");Bytes out(size);
    if(!file.read(reinterpret_cast<char*>(out.data()),size))throw FormatError("short catalog binding payload");return out;
}
std::span<const std::uint8_t> source_region(const SourceState& source,unsigned address,unsigned size){
    std::size_t offset=0;
    for(const auto [base,length]:source_blocks){
        if(address>=base&&address-base<=length&&size<=length-(address-base))
            return {reinterpret_cast<const std::uint8_t*>(source.data())+offset+address-base,size};
        offset+=length;
    }
    throw FormatError("unmapped catalog binding source region");
}
void seed(FrontendSourceStorage& owner,const SourceState& source){
    owner.cold_suffix.fill(0x913fd274u);owner.persistence.bytes.fill(0x5d);
    for(auto [address,size]:owned_blocks){const auto input=source_region(source,address,size);
        std::ranges::copy(input,owner.source_bytes(address,size).begin());}
}
void compare(FrontendSourceStorage& owner,const SourceState& source){
    for(auto [address,size]:owned_blocks){
        require(std::ranges::equal(owner.source_bytes(address,size),source_region(source,address,size)),"catalog binding original/native state mismatch");
        compared_bytes+=size;
    }
}
Bytes snapshot(FrontendSourceStorage& owner){
    Bytes result(owner.persistence.bytes.begin(),owner.persistence.bytes.end());
    const auto suffix=owner.source_bytes(FrontendSourceStorage::suffix_base,sizeof(owner.cold_suffix));
    result.insert(result.end(),suffix.begin(),suffix.end());
    for(const auto [address,size]:owned_blocks){if(address==0x5e37b8)continue;const auto v=owner.source_bytes(address,size);result.insert(result.end(),v.begin(),v.end());}
    return result;
}
void check_aliases(FrontendSourceStorage& owner){
    const auto view=owner.language_catalog_view();
    require(view.catalog.data()==&owner.source_word(0x5e37b8),"catalog is not the cold suffix owner");
    require(reinterpret_cast<std::uint8_t*>(view.catalog.data())==owner.source_bytes(0x5e37b8,0x128).data(),"catalog byte/word alias mismatch");
    require(view.text_handles.data()==owner.language_text_handles.data(),"text table copied");
    require(view.language_names.data()==owner.language_names.data(),"language names copied");
    require(&view.input_identity==&owner.language_input_identity,"input identity copied");
    require(&owner.source_word(0x606834)==&owner.language_text_handles.back(),"last text handle identity mismatch");
    require(&owner.source_word(0x606858)==&owner.language_names.back(),"last language name identity mismatch");
    require(&owner.source_word(0x6087a0)==&owner.language_input_identity,"input byte/word alias mismatch");
}
void compare_cold_guards(FrontendSourceStorage& owner,const Bytes& before){
    const auto after=snapshot(owner);constexpr auto begin=0x5e37b8-FrontendSourceStorage::cold_base,end=begin+0x128;
    require(std::equal(before.begin(),before.begin()+begin,after.begin()),"catalog overwrote cold prefix");
    require(std::equal(before.begin()+end,before.begin()+FrontendSourceStorage::cold_bytes,after.begin()+end),"catalog overwrote cold suffix");
    cold_guard_bytes+=FrontendSourceStorage::cold_bytes-0x128;
}
void atomic_controls(FrontendSourceStorage& owner){
    const auto before=snapshot(owner);const auto check=[&](auto&& operation){
        bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid catalog binding was accepted");require(snapshot(owner)==before,"catalog rejection mutated owners");++atomic_rejections;
    };
    for(const auto [address,size]:std::array<std::pair<unsigned,unsigned>,5>{{
        {0x606834,8},{0x606858,8},{0x6087a0,8},{0x60685c,4},{0x6087a4,8}}})
        check([&]{(void)owner.source_bytes(address,size);});
    check([&]{(void)owner.source_word(0x605cc9);});
    check([&]{(void)owner.source_word(0x60685c);});
    const auto view=owner.language_catalog_view();
    const Bytes invalid_flag{'H',',','A','\n','F',',','x','\n'};
    check([&]{(void)initialize_frontend_language_catalog(view,{invalid_flag,47});});
    const Bytes name_overflow(40,'a');
    check([&]{Bytes csv{'H',','};csv.insert(csv.end(),name_overflow.begin(),name_overflow.end());(void)initialize_frontend_language_catalog(view,{csv,48});});
    check([&]{(void)initialize_frontend_language_catalog(view,{invalid_flag,0});});
    check([&]{(void)initialize_frontend_language_catalog(view,{owner.source_bytes(0x5e37b8,0x128),49});});
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw FormatError("expected source catalog corpus");std::ifstream file(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(file,magic);require(std::string_view(magic.data(),8)=="SGNFCAT1","catalog binding magic");
    unsigned groups{};read(file,groups);require(groups>0&&groups<=4096,"catalog binding group bound");
    std::uint64_t calls=0,continuations=0,resource_bytes=0,cold_resets=0;
    for(unsigned group=0;group<groups;++group){
        unsigned size{};read(file,size);Archive resource_owner(read_bytes(file,size));
        const auto found=std::find_if(resource_owner.entries().begin(),resource_owner.entries().end(),[](const auto& e){return canonical(e.name)=="setup\\languagepc.csv";});
        require(found!=resource_owner.entries().end(),"real archive catalog resource missing");
        const auto input=resource_owner.data(*found);read(file,size);const auto expected_input=read_bytes(file,size);
        require(std::ranges::equal(input,expected_input),"native selected resource differs from original bytes");resource_bytes+=input.size();
        SourceState initial{};read(file,initial);FrontendSourceStorage owner;seed(owner,initial);check_aliases(owner);
        // Diagnostic identity registry borrows the actual Archive allocation.
        // No resource copy is stored in FrontendSourceStorage, and the resource
        // owner outlives this retained owner and all three source calls.
        std::map<std::uint32_t,std::span<const std::uint8_t>> identities;
        unsigned steps{};read(file,steps);require(steps>0&&steps<=32,"catalog binding sequence bound");
        for(unsigned step=0;step<steps;++step){
            SourceState before{},after{};unsigned identity{},cursor{};read(file,before);read(file,after);read(file,identity);read(file,cursor);
            compare(owner,before);const auto saved=snapshot(owner);const auto view=owner.language_catalog_view();
            require(identity!=0&&identities.emplace(identity,input).second,"source resource identity is not distinct");
            const auto actual_cursor=initialize_frontend_language_catalog(view,{identities.at(identity),identity});
            require(actual_cursor==cursor,"source catalog cursor mismatch");compare(owner,after);compare_cold_guards(owner,saved);check_aliases(owner);
            const auto resolved=identities.at(owner.language_input_identity);
            require(resolved.data()==input.data()&&resolved.size()==input.size(),"published identity lost actual resource owner");
            require(input.data()==resource_owner.data(*found).data(),"source archive allocation moved");
            require(std::ranges::equal(input,expected_input),"catalog mutated resource bytes");++calls;continuations+=step!=0;
        }
        atomic_controls(owner);
        const auto handles=owner.language_text_handles;const auto names=owner.language_names;const auto identity=owner.language_input_identity;
        const auto view=owner.language_catalog_view();clear_frontend_cold_storage(owner);
        require(std::ranges::all_of(view.catalog,[](auto word){return word==0;}),"cold reset missed aliased catalog");
        require(owner.language_text_handles==handles&&owner.language_names==names&&owner.language_input_identity==identity,"cold reset changed external language owners");
        check_aliases(owner);++cold_resets;
    }
    require(file.peek()==std::ifstream::traits_type::eof(),"trailing catalog binding corpus");
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"continuations\":"<<continuations
        <<",\"owned_state_bytes\":"<<compared_bytes<<",\"cold_guard_bytes\":"<<cold_guard_bytes<<",\"resource_bytes\":"<<resource_bytes
        <<",\"checks\":"<<checks<<",\"atomic_rejections\":"<<atomic_rejections<<",\"cold_resets\":"<<cold_resets<<",\"mismatches\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
