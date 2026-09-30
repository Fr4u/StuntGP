#pragma once
#include "sgn/source_pmd_geometry.hpp"
#include "sgn/model_resources.hpp"
#include <bit>
#include <cstring>
#include <fstream>

namespace geometry_receiving {
using namespace sgn;using namespace sgn::retail;
inline void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>void read(std::ifstream& in,T& value){in.read(reinterpret_cast<char*>(&value),sizeof(value));check(bool(in),"truncated geometry receiving corpus");}
inline std::uint32_t word(std::ifstream& in){std::uint32_t value;read(in,value);return value;}
inline std::string string(std::ifstream& in){const auto size=word(in);check(size<4096,"receiving string extent");std::string out(size,'\0');in.read(out.data(),size);check(bool(in),"truncated receiving string");return out;}
inline void magic(std::ifstream& in,std::string_view expected){char bytes[8];in.read(bytes,8);check(bool(in)&&std::string_view(bytes,8)==expected,"receiving schema");}
template<class T>void append(Bytes& out,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(value));}
inline Bytes encode(const LiveGeometry& geometry){
    // Portable field serialization: no padding bytes from Surface/LiveMaterial.
    Bytes out;const auto& m=geometry.model;append(out,m.kind);append(out,m.parts);append(out,m.lod_count);
    append(out,static_cast<std::uint32_t>(m.textures.size()));append(out,static_cast<std::uint32_t>(m.vertices.size()));append(out,static_cast<std::uint32_t>(m.surfaces.size()));
    for(const auto& text:m.textures){append(out,static_cast<std::uint32_t>(text.size()));out.insert(out.end(),text.begin(),text.end());}
    for(const auto& v:m.vertices)append(out,v);
    check(m.surfaces.size()==geometry.materials.size(),"surface/material count");
    for(std::size_t i=0;i<m.surfaces.size();++i){const auto& s=m.surfaces[i];append(out,s.first);append(out,s.count);append(out,s.texture);
        append(out,std::uint32_t(s.translucent));append(out,std::uint32_t(s.two_sided));append(out,s.part);append(out,std::uint32_t(geometry.materials[i].blend));}
    append(out,m.minimum);append(out,m.maximum);return out;
}
inline Bytes snapshot(const SourceAllocationBank& bank,const SourceAllocationRef& allocation){const auto bytes=bank.bytes(allocation);return {bytes.begin(),bytes.end()};}
struct Fixture {
    AssetStore assets;SourceAllocationBank bank;SourcePmdFileSystem files;ModelResources resources;
    explicit Fixture(const std::filesystem::path& root):assets(root),files(root),resources(assets,root){bank.initialize_42eab0();}
    std::uint32_t load(std::string name="car1"){return resources.load_source(bank,files,name);}
    SourcePmdModelView model(std::uint32_t id){return resources.source_at(*resources.resolve(id));}
};
template<class F>void allocation_rejects(F&& action){bool rejected=false;try{action();}catch(const SourceAllocationError&){rejected=true;}check(rejected,"missing typed bank lifetime/thread rejection");}
template<class F>void format_rejects(F&& action){bool rejected=false;try{action();}catch(const FormatError&){rejected=true;}check(rejected,"missing typed geometry domain rejection");}
}
