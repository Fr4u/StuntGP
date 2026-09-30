#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
unsigned checks{},rejected{};
void require(bool value,const char* message){if(!value)throw FormatError(message);++checks;}
template<class T>void write(std::ofstream& file,const T& v){file.write(reinterpret_cast<const char*>(&v),sizeof(v));}
void word(std::ofstream& file,std::uint32_t v){write(file,v);}
void controls(FrontendSourceStorage& owner){
    const auto arena=owner.language_arena;const auto handles=owner.language_text_handles;
    const auto names=owner.language_names;const auto identity=owner.language_input_identity;
    const auto cold=owner.cold_suffix;const auto persistence=owner.persistence.bytes;
    const auto view=owner.language_strings();
    const auto reject=[&](auto operation){
        bool failed=false;try{operation();}catch(const std::exception&){failed=true;}
        require(failed,"invalid language view accepted");
        require(owner.language_arena==arena&&owner.language_text_handles==handles&&owner.language_names==names&&
            owner.language_input_identity==identity&&owner.cold_suffix==cold&&owner.persistence.bytes==persistence,"language rejection mutated source owner");++rejected;
    };
    reject([&]{(void)frontend_language_text_441170(view,frontend_text_count);});
    reject([&]{(void)frontend_language_text_441170(view,0xffffffffu);});
    reject([&]{(void)resolve_frontend_language_text(view,{frontend_language_arena_bytes+1});});
    reject([&]{(void)resolve_frontend_language_text(view,{0xffffffffu});});
    reject([&]{(void)owner.source_bytes(0x605cc4,8);});
    reject([&]{(void)owner.source_word(0x5ffcc8);}); //byte owner must not become a fabricated word reference
    owner.language_arena.back()='X';const auto unterminated=owner.language_arena;
    bool failed=false;try{(void)resolve_frontend_language_text(view,{frontend_language_arena_bytes});}catch(const std::invalid_argument&){failed=true;}
    require(failed&&owner.language_arena==unterminated&&owner.language_text_handles==handles,"unterminated text changed owner");++rejected;
    owner.language_arena=arena;
    require(resolve_frontend_language_text(view,{0}).empty(),"null language identity resolved to a string");
    require(resolve_frontend_language_text(view,{frontend_language_arena_bytes}).size()==1,"empty string lost its actual NUL byte");
    clear_frontend_cold_storage(owner);
    require(owner.language_arena==arena&&owner.language_text_handles==handles&&owner.language_names==names&&owner.language_input_identity==identity,"cold clear crossed into language owners");
    const Bytes input{'H',',','E','n','\n','F',',','1','\n'};
    (void)initialize_frontend_language_catalog(owner.language_catalog_view(),{input,17});
    require(owner.language_arena==arena,"catalog initializer cleared the text arena");
    require(std::ranges::all_of(view.text_handles,[](auto h){return h==0;}),"retained const string view missed catalog table reset");
    require(frontend_language_text_441170(view,14).value==0,"lookup cached a handle across catalog reset");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw FormatError("expected language binding output");std::ofstream file(argv[1],std::ios::binary);
    file.write("SGNFLB01",8);word(file,8);FrontendSourceStorage owner;
    const FrontendSourceStorage& readonly=owner;const auto strings=readonly.language_strings();
    require(strings.arena.data()==owner.language_arena.data()&&strings.text_handles.data()==owner.language_text_handles.data(),"language adapter copied its canonical banks");
    require(owner.source_bytes(0x5ffcc8,frontend_language_arena_bytes).data()==strings.arena.data(),"language arena byte mapping mismatch");
    std::uint64_t lookups=0,resolved_bytes=0;
    for(unsigned pass=0;pass<8;++pass){
        for(unsigned i=0;i<owner.language_arena.size();++i)owner.language_arena[i]=i%13==0?0:static_cast<std::uint8_t>(33+(i+pass)%210);
        owner.language_arena.back()=0;
        for(unsigned i=0;i<owner.language_text_handles.size();++i)
            owner.language_text_handles[i]=(i+pass)%11==0?0:1+(i*37+pass*29)%frontend_language_arena_bytes;
        owner.language_text_handles[0]=0;owner.language_text_handles[1]=1;
        owner.language_text_handles[frontend_text_count-1]=frontend_language_arena_bytes;
        const auto before=owner.language_arena;const auto handles=owner.language_text_handles;
        write(file,owner.language_arena);write(file,owner.language_text_handles);word(file,frontend_text_count);
        for(unsigned id=0;id<frontend_text_count;++id){
            const auto identity=frontend_language_text_441170(strings,id);
            const auto text=resolve_frontend_language_text(strings,identity);
            word(file,id);word(file,identity.value);word(file,static_cast<std::uint32_t>(text.size()));
            if(!text.empty())file.write(reinterpret_cast<const char*>(text.data()),static_cast<std::streamsize>(text.size()));
            if(identity.value){require(text.data()==owner.language_arena.data()+identity.value-1,"resolved string copied away from arena");
                // The returned view remains tied to the live character owner.
                if(text.size()>1){const auto offset=identity.value-1;const auto old=owner.language_arena[offset];
                    owner.language_arena[offset]^=0x10;require(text[0]==owner.language_arena[offset],"resolved span missed in-place text mutation");owner.language_arena[offset]=old;}}
            else require(text.empty(),"null text has bytes");
            ++lookups;resolved_bytes+=text.size();
        }
        require(owner.language_arena==before&&owner.language_text_handles==handles,"read-only lookup changed the retained source owner");
        require(strings.arena.data()==owner.language_arena.data()&&strings.text_handles.data()==owner.language_text_handles.data(),"retained text owner moved");
    }
    controls(owner);require(bool(file),"language corpus write failed");
    std::cout<<"{\"passed\":true,\"retained_passes\":8,\"lookups\":"<<lookups<<",\"resolved_bytes\":"<<resolved_bytes
        <<",\"checks\":"<<checks<<",\"atomic_rejections\":"<<rejected<<",\"arena_copies\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
