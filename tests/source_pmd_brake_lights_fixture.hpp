#pragma once
#include "sgn/vehicle_model_effects.hpp"
#include <bit>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
namespace sgn::brake_test {
using namespace retail;
inline void check(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
template<class Error,class F>void rejects(F fn){bool caught=false;try{fn();}catch(const Error&){caught=true;}check(caught,"missing typed brake rejection");}
inline Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef allocation){const auto b=bank.bytes(allocation);return {b.begin(),b.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root,std::string name="car1"):assets(root),models(assets,root){bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,name));}
    SourcePmdModelView view(){return models.source_at(handle);}
};
inline std::size_t footprint(const Bytes& before,const Bytes& after,const std::set<std::uint32_t>& allowed){
    check(before.size()==after.size(),"PMD allocation resized by brake lights");std::size_t unchanged{};
    for(std::uint32_t i=0;i<before.size();++i)if(!allowed.contains(i)){check(before[i]==after[i],"brake write outside owned footprint");++unchanged;}
    return unchanged;
}
inline std::set<std::uint32_t> allowed_writes(SourcePmdPartView part,bool attributes){
    const auto flag=part.flags().location().offset;std::set<std::uint32_t> out{flag,flag+1};
    if(attributes)for(unsigned i=0;i<part.attribute_count().value();++i){out.insert(part.attribute_byte(i,2).location().offset);out.insert(part.attribute_byte(i,3).location().offset);}
    return out;
}
}
