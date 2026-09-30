#include "source_pmd_track_phase_fixture.hpp"
#include <map>
using namespace sgn;using namespace sgn::track_phase_test;
int main(int argc,char** argv)try{
    check(argc==3,"expected canonical track phase corpus and retail root");const auto records=corpus(argv[1]);
    SourceAllocationBank allocations;allocations.initialize_42eab0();AssetStore assets(argv[2]);ModelResources models(assets,argv[2]);std::map<std::string,std::size_t> handles;
    unsigned selected=0,skipped=0,errors=0;std::uint64_t calls=0,words=0,unchanged=0;
    for(const auto& r:records){if(r.op!=1){++skipped;continue;}++selected;finite(r.before,r.count);auto actual=r.before;
        std::optional<std::size_t> handle;Bytes before;SourceAllocationRef ref;
        if(r.count!=0xffffffffu){if(!handles.contains(r.name))handles[r.name]=*models.resolve(models.load_source(allocations,r.name.substr(0,r.name.size()-4)));
            handle=handles.at(r.name);const auto m=models.source_at(*handle);m.descriptor().field<unsigned>(0x24).set(r.count);ref=m.allocation();before=snapshot(allocations,ref);}
        for(unsigned step=0;step<r.steps;++step){advance_track_part_animations(actual,models,handle);++calls;}
        const auto a=std::bit_cast<std::array<unsigned,576>>(actual),e=std::bit_cast<std::array<unsigned,576>>(r.expected);
        for(unsigned i=0;i<a.size();++i){++words;if(a[i]!=e[i]){if(errors++<8)std::cerr<<"canonical track phase mismatch case "<<selected<<" word "<<i<<'\n';}}
        if(handle){check(before==snapshot(allocations,ref),"canonical track phase wrote PMD payload");unchanged+=before.size();}
    }
    check(selected>0,"canonical track phase no selected cases");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"selected\":"<<selected<<",\"skipped_loads\":"<<skipped<<",\"calls\":"<<calls<<",\"compared_words\":"<<words<<",\"unchanged_pmd_bytes\":"<<unchanged<<",\"mismatches\":"<<errors<<",\"excluded_words\":0,\"allowed_ulp\":0}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
