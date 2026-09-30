#include "sgn/model_resources.hpp"
#include <bit>
#include <cstring>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::uint32_t word(const Bytes& b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
template<class F>void rejects(F action){bool thrown=false;try{action();}catch(const std::exception&){thrown=true;}require(thrown,"invalid resource request accepted");}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail data root");
    const std::filesystem::path root=argv[1];AssetStore assets(root);ModelResources resources(assets,root);
    unsigned models=0,lods=0,parts=0,aliases=0,unsupported=0,adjacent=0;std::size_t raw_bytes=0;
    for(const auto& name:assets.models()){
        const auto bytes=read_file(root/"meshdata"/(name+".PMD"));
        if(std::string(reinterpret_cast<const char*>(bytes.data()),9)=="PMD V1.82"){rejects([&]{resources.load(name);});++unsupported;continue;}
        const std::string texture_set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";
        const auto id=resources.load(name,texture_set);const auto handle=resources.resolve(id);require(handle.has_value(),"non-null model has no handle");
        require(resources.identity(handle)==id,"resource identity unstable");
        require(resources.source_name(*handle)==name,"resource provenance differs from exact load request");
        auto& model=resources.at(*handle);
        const auto corners=model.source_corner_indices();require(corners.size()*2==word(bytes,0xc4),"source corner-index byte count differs");
        require(corners.empty()||std::memcmp(corners.data(),bytes.data()+328+word(bytes,0x30),corners.size_bytes())==0,"source corner-index bytes differ");
        const std::size_t descriptor=328+word(bytes,0x4c),transform_start=328+word(bytes,0x44),part_start=328+word(bytes,0x48);
        require(model.source_model_flags_02()==static_cast<std::uint16_t>(word(bytes,descriptor)>>16),"source model descriptor+2 flags differ");
        require(model.previous_detail_level()==word(bytes,descriptor+0x64),"previous detail level differs from source descriptor");
        require(model.source_model_ambience()==static_cast<std::uint16_t>(word(bytes,descriptor+4)),"model ambience word differs");
        require(std::memcmp(&model.source_model_vector_10(),bytes.data()+descriptor+0x10,16)==0,"model descriptor10 vector differs");
        require(model.source_attribute_table().size()*8==word(bytes,0xb8),"model whole attribute view differs");
        for(unsigned slot=0;slot<4;++slot){
            const auto authored=word(bytes,descriptor+0x48+slot*4);std::optional<unsigned> expected;
            for(unsigned lod=0;lod<model.lod_count();++lod)if(word(bytes,descriptor+0x48+lod*4)==authored){expected=lod;break;}
            if(expected){const auto resolved=model.source_lod_index(slot);require(resolved==*expected,"source LOD slot maps to wrong authored span");
                require(&model.source_part(resolved,0).transform==&model.source_part(*expected,0).transform,"source LOD slot duplicates transform identity");}
            else rejects([&]{(void)model.source_lod_index(slot);});
        }
        rejects([&]{(void)model.source_lod_index(4);});
        std::map<unsigned,ModelTransform*> transform_owners;std::map<unsigned,std::uint16_t*> flag_owners,flag02_owners;
        std::map<unsigned,const void*> attribute_owners,coordinate_owners;
        for(unsigned lod=0;lod<model.lod_count();++lod){++lods;const auto start=word(bytes,descriptor+0x48+lod*4);
            require(model.part_count(lod)==model.geometry(lod).parts,"incomplete LOD parts");
            for(unsigned part=0;part<model.part_count(lod);++part){++parts;auto bound=model.part(lod,part);const auto source_part=start+part;
                const auto tr=word(bytes,part_start+source_part*64+8);
                const auto remaining=word(bytes,0xd8)/80-tr;const auto run=model.source_transform_run(lod,part,remaining);
                require(run.size()==remaining&&run.data()==&bound.transform,"continuous source transform run lost base identity");
                for(unsigned t=0;t<run.size();++t)require(std::memcmp(&run[t],bytes.data()+transform_start+(tr+t)*80,80)==0,"continuous transform run follows part pointers instead of source table");
                rejects([&]{model.source_transform_run(lod,part,remaining+1);});
                require(std::memcmp(&bound.transform,bytes.data()+transform_start+tr*80,80)==0,"authored transform words differ");raw_bytes+=80;
                std::uint16_t flags;std::memcpy(&flags,bytes.data()+part_start+source_part*64,2);require(bound.flags==flags,"authored flags differ");
                auto& flags02=model.source_part_flags_02(lod,part);require(flags02==static_cast<std::uint16_t>(word(bytes,part_start+source_part*64)>>16),"source part+2 flags differ");
                const auto [flag02,inserted02]=flag02_owners.emplace(source_part,&flags02);if(!inserted02)require(flag02->second==&flags02,"part+2 aliases duplicated");
                const auto saved02=flags02;flags02^=4;require(bound.flags==flags,"part+2 write altered visibility flags");flags02=saved02;
                const auto [t,added]=transform_owners.emplace(tr,&bound.transform);if(!added){require(t->second==&bound.transform,"shared source transform was duplicated");++aliases;}
                const auto [f,new_part]=flag_owners.emplace(source_part,&bound.flags);if(!new_part)require(f->second==&bound.flags,"shared LOD flags duplicated");
                const auto source=part_start+source_part*64;const auto first_vertex=word(bytes,source+0x30);const auto count=word(bytes,source+0x28)&0xffff;
                const auto attrs=model.source_attributes(lod,part);const auto coords=model.source_coordinates(lod,part);
                require(attrs.size()==(word(bytes,source+0x24)&0xffff)&&coords.size()==(word(bytes,source+0x24)>>16),"source attribute/coordinate count differs");
                for(unsigned a=0;a<attrs.size();++a){const auto index=word(bytes,source+0x34)+a;require(std::memcmp(attrs[a].data(),bytes.data()+328+word(bytes,0x24)+index*8,8)==0,"source attribute bytes differ");
                    const auto [found,attribute_added]=attribute_owners.emplace(index,&attrs[a]);if(!attribute_added)require(found->second==&attrs[a],"shared source attribute duplicated");}
                for(unsigned c=0;c<coords.size();++c){const auto index=word(bytes,source+0x2c)+c;require(std::memcmp(coords[c].data(),bytes.data()+328+word(bytes,0x28)+index*16,16)==0,"source coordinate bits differ");
                    const auto [found,coordinate_added]=coordinate_owners.emplace(index,&coords[c]);if(!coordinate_added)require(found->second==&coords[c],"shared source coordinate duplicated");}
                const auto vertices=328+std::size_t(word(bytes,0x2c));float maximum=0;
                const auto vertex_view=model.source_vertices(lod,part);require(vertex_view.size()==word(bytes,source+0x28),"source vertex32 count differs");
                require(vertex_view.empty()||std::memcmp(vertex_view.data(),bytes.data()+vertices+first_vertex*16,vertex_view.size_bytes())==0,"source vertex view differs from PMD");
                for(unsigned k=0;k<count;++k){const auto z=std::bit_cast<float>(word(bytes,vertices+(first_vertex+k)*16+8));if(maximum<z)maximum=z;}
                require(model.maximum_source_vertex_z(lod,part)==maximum,"source maximum omitted authored vertices");
            }
        }
        for(unsigned lod=0;lod<model.lod_count();++lod){const auto count=static_cast<unsigned>(model.part_count(lod));const auto start=word(bytes,descriptor+0x48+lod*4);
            for(unsigned beyond=0;beyond<2;++beyond){const auto global=start+count+beyond;const auto found=flag_owners.find(global);
                if(found!=flag_owners.end()){require(&model.source_part(lod,count+beyond).flags==found->second,"adjacent source LOD identity lost");++adjacent;}
                else rejects([&]{model.source_part(lod,count+beyond);});
            }
            rejects([&]{model.part(lod,count);});
        }
        // All aliases must immediately observe one mutation, even after another
        // resource is appended. This catches copied PmdPart transform storage.
        if(!transform_owners.empty()){
            auto* owner=transform_owners.begin()->second;const float saved=owner->scale.x;owner->scale.x=123.25f;
            for(unsigned lod=0;lod<model.lod_count();++lod)for(unsigned j=0;j<model.part_count(lod);++j){auto part=model.part(lod,j);if(&part.transform==owner)require(part.transform.scale.x==123.25f,"alias mutation lost");}
            owner->scale.x=saved;
        }
        ++models;
    }
    require(models==78&&unsupported==10&&aliases>0&&adjacent>0,"retail model/alias coverage changed");
    auto& car=resources.at(*resources.resolve(resources.load("car16")));
    auto p=car.presentation_parts(0);const auto trail=car.trail_model();
    require(&p.part19.scale==&car.part(0,19).transform.scale,"presentation scale must borrow actual transform");
    require(std::bit_cast<std::uint32_t>(trail.base_forward.w)==car.part(0,0).transform.forward_w,"trail W must come from model owner");
    const auto before=resources.size();rejects([&]{resources.load("../car16");});rejects([&]{resources.load("__missing_model__");});
    require(resources.size()==before,"failed resource load exposed partial identity");
    require(!resources.resolve(0)&&resources.identity({})==0,"null resource mapping");
    rejects([&]{resources.resolve(static_cast<std::uint32_t>(resources.size()+1));});rejects([&]{resources.identity(resources.size());});
    rejects([&]{car.part(99,0);});rejects([&]{car.part(0,999999);});
    auto* retained=&car.part(0,19).transform;
    const auto duplicate=resources.load("car16");auto& other=resources.at(*resources.resolve(duplicate));
    require(&car!=&other&&&car.part(0,19).transform!=&other.part(0,19).transform&&&car.part(0,19).flags!=&other.part(0,19).flags,"separate requests alias mutable PMD state");
    const auto other_scale=other.part(0,19).transform.scale.x;const auto other_flags=other.part(0,19).flags;
    const auto other_flags02=other.source_part_flags_02(0,19);car.source_part_flags_02(0,19)^=4;
    require(other.source_part_flags_02(0,19)==other_flags02,"independent model flags02 share mutations");
    car.part(0,19).transform.scale.x=1234;car.part(0,19).flags^=0x20;
    require(other.part(0,19).transform.scale.x==other_scale&&other.part(0,19).flags==other_flags,"one car animated another copy of the same model");
    auto attrs=car.source_attributes(0,5);auto other_attrs=other.source_attributes(0,5);require(!attrs.empty()&&!other_attrs.empty(),"car brake attributes missing");
    const auto attr_before=other_attrs[0];attrs[0][2]^=0x80;require(other_attrs[0]==attr_before,"separate model attribute tables share mutations");
    auto coords=car.source_coordinates(0,19);auto other_coords=other.source_coordinates(0,19);require(!coords.empty()&&!other_coords.empty(),"car effect coordinates missing");
    const auto coord_before=other_coords[0];coords[0][1]^=0x100;require(other_coords[0]==coord_before,"separate model coordinate tables share mutations");
    require(&car.part(0,19).transform==retained,"resource append invalidated references");
    std::array<LevelCarSlot,6> cars{};for(auto& c:cars){c.source_word00=91;c.source_word04=92;}
    ModelBindingSlots slots{};std::array<SelectionRecord,6> selection{};selection[3].kind=2;selection[5].kind=2;
    const std::array<LevelAssetRequest,4> requests{{{"car1",4},{"car16",5},{"car16",4},{"car1",7}}};
    const auto batch_start=resources.size();bind_level_model_resources(resources,requests,slots,selection,cars);
    require(resources.size()==batch_start+4&&slots[4]==batch_start+2&&slots[5]==batch_start+1&&slots[7]==batch_start+3,"request ordering or separate allocations lost");
    require(cars[3].source_word00==resources.identity(slots[4])&&cars[3].source_word04==resources.identity(slots[5])&&cars[5].source_word04==resources.identity(slots[7])&&cars[5].source_word00==0&&cars[0].source_word00==91,"sparse physical selection binding/preserved slots");
    const auto saved=cars;const std::array<LevelAssetRequest,2> invalid{{{"car1",4},{"__missing_model__",5}}};
    const auto saved_slots=slots;rejects([&]{bind_level_model_resources(resources,invalid,slots,selection,cars);});require(std::memcmp(saved.data(),cars.data(),sizeof(cars))==0&&slots==saved_slots,"failed resource batch published bindings");
    std::vector<LevelAssetRequest> pairs;for(unsigned j=4;j<16;++j)pairs.push_back({"car1",j});bind_level_model_resources(resources,pairs,slots,selection,cars);
    for(unsigned mask=0;mask<64;++mask){for(unsigned j=0;j<6;++j){selection[j].kind=(mask&(1u<<j))?2u:((j%2)?1u:3u);cars[j].source_word00=91;cars[j].source_word04=92;}
        bind_level_model_resources(resources,{},slots,selection,cars);unsigned compact=0;
        for(unsigned j=0;j<6;++j){if(selection[j].kind==2){require(cars[j].source_word00==resources.identity(slots[4+compact*2])&&cars[j].source_word04==resources.identity(slots[5+compact*2]),"compact request mapped to wrong physical car");++compact;}
            else require(cars[j].source_word00==91&&cars[j].source_word04==92,"non-kind2 car changed by resource fulfillment");}
    }
    std::cout<<"models="<<models<<" lods="<<lods<<" parts="<<parts<<" shared_transform_references="<<aliases<<" original_transform_bytes="<<raw_bytes<<" adjacent_source_parts="<<adjacent<<" errors=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
