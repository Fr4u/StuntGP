#pragma once
#include "shadow_draw_codec.hpp"
namespace {
void read_parts(std::ifstream& in,SourcePmdModelView model,unsigned lod,bool input){
    const auto count=word(in);require(count==model.part_count(),"draw model part count mismatch");
    for(unsigned i=0;i<count;++i){const auto pair=word(in);SourcePmdTransform transform;read(in,transform);const auto p=model.part(lod,i);
        if(input){p.flags().set(static_cast<std::uint16_t>(pair));p.flags_02().set(static_cast<std::uint16_t>(pair>>16));
            transform.tail[0]=matrix_token(transform.tail[0]);transform.tail[1]=matrix_token(transform.tail[1]);p.transform().set(transform);}
        else{compare(std::uint32_t(p.flags().value())|(std::uint32_t(p.flags_02().value())<<16),pair,"part flag pair");auto actual=p.transform().value();
            actual.tail[0]=matrix_address(actual.tail[0]);actual.tail[1]=matrix_address(actual.tail[1]);compare(actual,transform,"model transform");}}
}
}
