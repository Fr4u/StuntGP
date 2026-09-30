#include "sgn/retail_spline.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <map>

using namespace sgn;
using namespace sgn::retail;
namespace {
Bytes file(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open input");
    const auto n=f.tellg();if(n<0||n>64*1024*1024)throw std::runtime_error("input too large");
    Bytes b(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);
    if(!f)throw std::runtime_error("short input");return b;
}
struct Reader {
    std::span<const std::uint8_t> data;std::size_t at{};
    std::uint32_t word() {
        if(at>data.size()||data.size()-at<4)throw std::runtime_error("truncated corpus word");
        const auto p=at;at+=4;return std::uint32_t(data[p])|(std::uint32_t(data[p+1])<<8)|
            (std::uint32_t(data[p+2])<<16)|(std::uint32_t(data[p+3])<<24);
    }
    float real(){return std::bit_cast<float>(word());}
    std::int32_t integer(){return std::bit_cast<std::int32_t>(word());}
    std::span<const std::uint8_t> bytes(std::size_t n) {
        if(at>data.size()||n>data.size()-at)throw std::runtime_error("truncated corpus block");
        const auto result=data.subspan(at,n);at+=n;return result;
    }
};
void push(std::vector<std::uint32_t>& out,float f){out.push_back(std::bit_cast<std::uint32_t>(f));}
void push(std::vector<std::uint32_t>& out,Vec3 v){push(out,v.x);push(out,v.y);push(out,v.z);}
void push(std::vector<std::uint32_t>& out,VehicleVector v){push(out,Vec3{v.x,v.y,v.z});push(out,v.w);}
std::vector<std::uint32_t> execute(std::uint32_t op,SplineSet& set,Reader& in) {
    std::vector<std::uint32_t> out;
    if(op==6){for(float value:decode_respawn_mask(in.bytes(in.data.size())))push(out,value);return out;}
    if(op==5){for(const auto& route:ai_spline_routes(set))out.push_back(route.descriptor.id);return out;}
    const auto spline=set.find(in.word());if(!spline)throw FormatError("unknown spline ID");
    if(op==0){const auto& d=spline->descriptor;out={d.id,d.point_index,d.point_count,d.opaque[0],d.opaque[1],d.opaque[2],std::bit_cast<std::uint32_t>(d.length),static_cast<std::uint32_t>(d.point_offset)};}
    else if(op==1)push(out,sample_spline(*spline,in.real()).value);
    else if(op==7){const auto result=sample_spline_indexed(*spline,in.real());push(out,result.value);out.push_back(result.selected_index);}
    else if(op==8){const auto result=prepare_distance_start(*spline,in.real());push(out,result.position);push(out,result.heading);
        out.push_back(result.selected_index);out.push_back(result.progress_current);out.push_back(result.progress_previous);
        push(out,result.progress_percent);for(auto i:result.route_indices)out.push_back(i);}
    else if(op==2){const Vec3 pos{in.real(),in.real(),in.real()};const auto result=project_spline(*spline,pos,in.integer());out.push_back(std::bit_cast<std::uint32_t>(result.segment));push(out,result.percent);}
    else if(op==3){std::array<std::int32_t,6> active{};for(auto& v:active)v=in.integer();const auto grid=starting_grid(*spline,active,in.integer());
        for(const auto& p:grid){out.push_back(p?1u:0u);if(!p){out.insert(out.end(),15,0);continue;}push(out,p->position);for(auto b:p->basis)push(out,b);out.push_back(p->physical_rank);out.push_back(p->copied_position_w?1u:0u);push(out,p->copied_position_w.value_or(0));}}
    else if(op==4){const auto requested=in.integer();std::array<float,128> mask{};for(auto& v:mask)v=in.real();const auto result=select_respawn(*spline,requested,mask);push(out,result.position);push(out,result.heading);out.push_back(std::bit_cast<std::uint32_t>(result.persistent_segment));for(auto i:result.route_indices)out.push_back(std::bit_cast<std::uint32_t>(i));}
    else throw std::runtime_error("invalid corpus operation");
    return out;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("usage: retail_spline_oracle corpus asset_directory");
        const auto bytes=file(argv[1]);Reader corpus{bytes};const auto magic=corpus.bytes(8);
        if(std::string(magic.begin(),magic.end())!="SGNSP001")throw std::runtime_error("invalid corpus signature");
        const auto count=corpus.word();if(!count||count>1000000)throw std::runtime_error("empty or excessive corpus");
        std::map<std::string,SplineSet> models;std::uint64_t words=0;std::uint32_t rejected=0;
        for(std::uint32_t c=0;c<count;++c) {
            const auto op=corpus.word(),reject=corpus.word(),name_size=corpus.word(),input_size=corpus.word(),expected_size=corpus.word();
            if(reject>1||name_size>256||input_size>131072||expected_size>16384||expected_size%4)throw std::runtime_error("invalid corpus lengths");
            const auto name_bytes=corpus.bytes(name_size);const std::string name(name_bytes.begin(),name_bytes.end());
            if(name.empty()||name.find_first_of("/\\:")!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("invalid asset name");
            Reader input{corpus.bytes(input_size)},expected{corpus.bytes(expected_size)};
            if(!models.contains(name))models.emplace(name,SplineSet(file(std::filesystem::path(argv[2])/name)));
            try {
                const auto out=execute(op,models.at(name),input);
                if(reject){std::cerr<<"case "<<c<<" op "<<op<<" expected bounds rejection\n";return 1;}
                if(input.at!=input.data.size()||out.size()*4!=expected_size)throw std::runtime_error("corpus shape differs from operation");
                for(std::size_t w=0;w<out.size();++w){const auto reference=expected.word();++words;if(out[w]!=reference){std::cerr<<"case "<<c<<" model "<<name<<" op "<<op<<" word "<<w<<" native "<<std::hex<<out[w]<<" original "<<reference<<'\n';return 1;}}
            } catch(const FormatError& error) {
                if(!reject){std::cerr<<"case "<<c<<" model "<<name<<" op "<<op<<" unexpected contract error: "<<error.what()<<'\n';return 1;}++rejected;
            }
        }
        if(corpus.at!=bytes.size())throw std::runtime_error("trailing corpus bytes");
        std::cout<<count<<" cases; "<<words<<" words; "<<rejected<<" explicit bounds rejections; 0 mismatches; tolerance 0 ULP\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
