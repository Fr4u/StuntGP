#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned state_size=0x900c,font_size=1716;
using Expected=std::array<std::uint32_t,(state_size+font_size)/4>;
constexpr std::array<std::pair<unsigned,unsigned>,9> blocks{{
    {0x5e37b8,0x128},{0x5ff8c4,0x404},{0x5ffcc8,0x6000},{0x605cc8,0xb70},
    {0x606838,0x24},{0x606860,0x1f40},{0x6087a0,4},{0x6087a4,4},{0x47b1a0,4}}};
unsigned checks{},atomic_rejections{};std::uint64_t compared{},guard_bytes{},borrowed_texts{};
void require(bool value,const char* message){if(!value)throw FormatError(message);++checks;}
template<class T>void read(std::ifstream& file,T& value){if(!file.read(reinterpret_cast<char*>(&value),sizeof(value)))throw FormatError("short selection binding corpus");}
Bytes bytes(std::ifstream& file,unsigned size){require(size<=8*1024*1024,"selection archive bound");Bytes b(size);if(!file.read(reinterpret_cast<char*>(b.data()),size))throw FormatError("short selection archive");return b;}
std::span<const std::uint8_t> resource(const Archive& archive,std::string_view name){
    const auto found=std::find_if(archive.entries().begin(),archive.entries().end(),[&](const auto& e){return canonical(e.name)==name;});
    require(found!=archive.entries().end(),"selected archive resource missing");return archive.data(*found);
}
struct FontProjection { //Fixture-only projection; no second runtime font owner.
    std::array<std::uint32_t,font_size/4> words{};
    FrontendFontMetricsViews view(){return {std::span<std::uint32_t,322>{words.data(),322},
        std::span<std::uint8_t,160>{reinterpret_cast<std::uint8_t*>(words.data()+322),160},
        std::span<std::uint8_t,256>{reinterpret_cast<std::uint8_t*>(words.data()+362),256},words[426],words[427],words[428]};}
};
void seed(FrontendSourceStorage& owner,FontProjection& fonts,const Expected& expected){
    owner.cold_suffix.fill(0xe7925483u);owner.persistence.bytes.fill(0x72);std::size_t offset=0;
    const auto* input=reinterpret_cast<const std::uint8_t*>(expected.data());
    for(const auto [address,size]:blocks){std::memcpy(owner.source_bytes(address,size).data(),input+offset,size);offset+=size;}
    require(offset==state_size,"selection source block layout mismatch");std::memcpy(fonts.words.data(),input+offset,font_size);
}
void compare(FrontendSourceStorage& owner,const FontProjection& fonts,const Expected& expected){
    const auto* input=reinterpret_cast<const std::uint8_t*>(expected.data());std::size_t offset=0;
    for(const auto [address,size]:blocks){const auto actual=owner.source_bytes(address,size);
        require(std::memcmp(actual.data(),input+offset,size)==0,"actual frontend selection bank differs from original");offset+=size;compared+=size;}
    require(std::memcmp(fonts.words.data(),input+state_size,font_size)==0,"actual font projection differs from original");compared+=font_size;
}
void aliases(FrontendSourceStorage& owner){
    const auto view=owner.language_selection_view();const auto strings=std::as_const(owner).language_strings();
    require(view.catalog.catalog.data()==&owner.source_word(0x5e37b8),"selection copied cold catalog");
    require(view.scratch.data()==owner.language_scratch.data()&&view.arena.data()==strings.arena.data(),"selection copied scratch/arena");
    require(view.descriptors.data()==owner.language_descriptors.data()&&&view.remaining==&owner.language_remaining,"selection copied descriptor/remaining owners");
    require(view.catalog.text_handles.data()==strings.text_handles.data(),"selection/lookup use separate text tables");
    require(&owner.source_word(0x60879c)==&owner.language_descriptors.back(),"last descriptor word lost physical owner");
    require(&owner.source_word(0x47b1a0)==&view.remaining&&&owner.source_word(0x6087a4)==&owner.language_empty_sentinel,"scalar identity mismatch");
}
void cold_guard(FrontendSourceStorage& owner,const std::array<std::uint32_t,(FrontendSourceStorage::cold_bytes-persistence_size)/4>& before){
    constexpr auto begin=(0x5e37b8-FrontendSourceStorage::suffix_base)/4,end=begin+0x128/4;
    require(std::equal(before.begin(),before.begin()+begin,owner.cold_suffix.begin())&&
        std::equal(before.begin()+end,before.end(),owner.cold_suffix.begin()+end),"selection overwrote unrelated cold state");
    require(std::ranges::all_of(owner.persistence.bytes,[](auto b){return b==0x72;}),"selection overwrote persistence");guard_bytes+=FrontendSourceStorage::cold_bytes-0x128;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw FormatError("expected selection binding corpus");std::ifstream file(argv[1],std::ios::binary);std::array<char,8> magic{};read(file,magic);
    require(std::string_view(magic.data(),8)=="SGNFLNG1","selection binding magic");unsigned groups{};read(file,groups);require(groups>0&&groups<=128,"selection binding group bound");
    unsigned size{};read(file,size);Archive setup(bytes(file,size));read(file,size);Archive font_archive(bytes(file,size));
    const auto language=resource(setup,"setup\\languagepc.csv");
    const FrontendFontInputs inputs{resource(font_archive,"graphics24\\fonts\\index.txt"),resource(font_archive,"graphics24\\fonts\\i32.tga"),
        resource(font_archive,"graphics24\\fonts\\i32_b05.tga"),resource(font_archive,"graphics24\\fonts\\i32_b025.tga")};
    unsigned calls=0,continuations=0,font_calls=0,catalog_calls=0,selection_calls=0;
    for(unsigned group=0;group<groups;++group){
        FrontendSourceStorage owner;require(owner.language_remaining==0x6000&&owner.language_empty_sentinel==0,"frontend scalar PE initialization differs");
        Expected initial{};read(file,initial);FontProjection fonts;seed(owner,fonts,initial);aliases(owner);
        unsigned steps{};read(file,steps);require(steps>0&&steps<=128,"selection binding sequence bound");
        for(unsigned step=0;step<steps;++step){
            unsigned op{},argument{};Expected before{},after{};std::array<std::uint32_t,3> expected{},actual{};
            read(file,op);read(file,argument);read(file,before);read(file,after);read(file,expected);compare(owner,fonts,before);const auto cold=owner.cold_suffix;
            if(op==0){initialize_frontend_font_metrics(fonts.view(),inputs,std::bit_cast<std::int32_t>(argument));++font_calls;}
            else if(op==1){(void)initialize_frontend_language_catalog(owner.language_catalog_view(),{language,1});++catalog_calls;}
            else if(op==2){const auto result=select_frontend_language_440c40(owner.language_selection_view(),{language,1},fonts.view(),argument);
                actual={std::bit_cast<std::uint32_t>(result.column),result.cursor,result.unresolved};++selection_calls;}
            else throw FormatError("unknown selection binding operation");
            compare(owner,fonts,after);require(actual==expected,"selection source return/cursor differs");cold_guard(owner,cold);aliases(owner);
            if(op==2&&actual[0]!=0xffffffffu){
                const auto strings=std::as_const(owner).language_strings();
                for(unsigned id=0;id<frontend_text_count;++id){const auto handle=frontend_language_text_441170(strings,id);const auto text=resolve_frontend_language_text(strings,handle);
                    require(handle.value?(!text.empty()&&text.back()==0&&text.data()==owner.language_arena.data()+handle.value-1):text.empty(),"selected text no longer borrows the actual frontend arena");++borrowed_texts;}
            }
            require(language.data()==resource(setup,"setup\\languagepc.csv").data(),"selection input lost actual archive ownership");++calls;continuations+=step!=0;
        }
        const auto arena=owner.language_arena;const auto scratch=owner.language_scratch;const auto descriptors=owner.language_descriptors;
        const auto handles=owner.language_text_handles;const auto names=owner.language_names;const auto remaining=owner.language_remaining;
        for(const auto [address,length]:std::array<std::pair<unsigned,unsigned>,4>{{{0x5ffcc4,8},{0x60879c,8},{0x6087a0,8},{0x60685c,4}}}){
            bool failed=false;try{(void)owner.source_bytes(address,length);}catch(const FormatError&){failed=true;}
            require(failed,"view crossed distinct frontend owners");++atomic_rejections;
        }
        clear_frontend_cold_storage(owner);
        require(owner.language_arena==arena&&owner.language_scratch==scratch&&owner.language_descriptors==descriptors&&
            owner.language_text_handles==handles&&owner.language_names==names&&owner.language_remaining==remaining,"cold reset changed retained language storage");
        require(std::ranges::all_of(owner.language_catalog_view().catalog,[](auto w){return w==0;}),"cold reset missed catalog alias");aliases(owner);
    }
    require(file.peek()==std::ifstream::traits_type::eof(),"trailing selection binding corpus");
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"continuations\":"<<continuations<<",\"font_calls\":"<<font_calls
        <<",\"catalog_calls\":"<<catalog_calls<<",\"selection_calls\":"<<selection_calls<<",\"compared_state_bytes\":"<<compared<<",\"cold_guard_bytes\":"<<guard_bytes
        <<",\"borrowed_texts\":"<<borrowed_texts<<",\"checks\":"<<checks<<",\"atomic_rejections\":"<<atomic_rejections<<",\"mismatches\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
