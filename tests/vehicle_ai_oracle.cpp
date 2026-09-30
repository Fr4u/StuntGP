#include "sgn/vehicle_ai.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace sgn::retail;
constexpr std::uint32_t base=0x49a1f8,stride=0x7724;
using RacerImage=std::array<std::uint32_t,0x28a0/4>;
using FieldImage=std::array<RacerImage,6>;
using FrameImage=std::array<std::uint32_t,10>;
template<class T> void read(std::ifstream& f,T& value) {
    if(!f.read(reinterpret_cast<char*>(&value),sizeof(value))) throw std::runtime_error("truncated AI corpus");
}
float fp(const RacerImage& b,std::size_t at) { return std::bit_cast<float>(b.at(at/4)); }
std::int32_t si(const RacerImage& b,std::size_t at) {return std::bit_cast<std::int32_t>(b.at(at/4));}
void put(RacerImage& b,std::size_t at,float value) { b.at(at/4)=std::bit_cast<std::uint32_t>(value); }
void put(RacerImage& b,std::size_t at,std::int32_t value) { b.at(at/4)=std::bit_cast<std::uint32_t>(value); }
sgn::Vec3 vec(const RacerImage& b,std::size_t at) {return {fp(b,at),fp(b,at+4),fp(b,at+8)};}
void putv(RacerImage& b,std::size_t at,sgn::Vec3 value) {put(b,at,value.x);put(b,at+4,value.y);put(b,at+8,value.z);}
AiField decode(const FieldImage& image) {
    AiField result;
    for(std::size_t i=0;i<6;++i) {
        auto& s=result[i];const auto& b=image[i];
        s.active=si(b,0x1ba8);s.position=vec(b,0x1bac);s.right=vec(b,0x1cbc);s.forward=vec(b,0x1cdc);
        s.route_position=vec(b,0x24b0);s.route_right=vec(b,0x24d0);
        s.speed=fp(b,0x1cf0);s.throttle=fp(b,0x1cf8);s.route_progress=fp(b,0x23b0);
        s.contact_class=si(b,0x1fd8);s.bend=fp(b,0x2668);s.race_index=si(b,0x23a4);
        for(std::size_t k=0;k<6;++k) s.rival_counters[k]=b[(0x25f4+k*4)/4];
        const auto pointer=b[0x2688/4];
        if(pointer!=0) {
            if(pointer<base || (pointer-base)%stride!=0 || (pointer-base)/stride>=6)
                throw std::runtime_error("invalid AI corpus opponent pointer");
            s.passing_target=(pointer-base)/stride;
        }
        s.passing_ticks=si(b,0x268c);s.passing_offset=fp(b,0x2690);s.following_ticks=si(b,0x26a0);
        s.steering=fp(b,0x1cf4);s.max_steering=fp(b,0x24c);s.up_y=fp(b,0x1cd0);
        s.driving_factor=fp(b,0x2660);s.steering_divisor_signal=fp(b,0x289c);
        s.steering_ramp_ticks=si(b,0x26a4);s.reverse_ticks=si(b,0x2664);s.gear=si(b,0x1f10);
    }
    return result;
}
void encode(const AiField& field,FieldImage& image) {
    for(std::size_t i=0;i<6;++i) {
        const auto& s=field[i];auto& b=image[i];
        put(b,0x1ba8,s.active);putv(b,0x1bac,s.position);putv(b,0x1cbc,s.right);putv(b,0x1cdc,s.forward);
        putv(b,0x24b0,s.route_position);putv(b,0x24d0,s.route_right);
        put(b,0x1cf0,s.speed);put(b,0x1cf8,s.throttle);put(b,0x23b0,s.route_progress);
        put(b,0x1fd8,s.contact_class);put(b,0x2668,s.bend);put(b,0x23a4,s.race_index);
        for(std::size_t k=0;k<6;++k) b[(0x25f4+k*4)/4]=s.rival_counters[k];
        b[0x2688/4]=s.passing_target?base+stride*static_cast<std::uint32_t>(*s.passing_target):0;
        put(b,0x268c,s.passing_ticks);put(b,0x2690,s.passing_offset);put(b,0x26a0,s.following_ticks);
        put(b,0x1cf4,s.steering);put(b,0x24c,s.max_steering);put(b,0x1cd0,s.up_y);
        put(b,0x2660,s.driving_factor);put(b,0x289c,s.steering_divisor_signal);
        put(b,0x26a4,s.steering_ramp_ticks);put(b,0x2664,s.reverse_ticks);put(b,0x1f10,s.gear);
    }
}
AiTactics decode(const FrameImage& frame) {
    auto at=[&](std::size_t i){return std::bit_cast<float>(frame[i]);};
    return {{at(0),at(1),at(2)},at(3),at(4),at(5),at(6),{at(7),at(8),at(9)}};
}
FrameImage encode(const AiTactics& a) {
    return std::bit_cast<FrameImage>(std::array<float,10>{a.target.x,a.target.y,a.target.z,
        a.target_speed,a.response_scale,a.spline_length,a.course_length,a.world_up.x,a.world_up.y,a.world_up.z});
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: vehicle_ai_oracle corpus");
        std::ifstream input(argv[1],std::ios::binary);std::array<char,8> magic{};read(input,magic);
        if(std::string(magic.data(),magic.size())!="SGNAI002") throw std::runtime_error("wrong AI corpus signature");
        std::uint32_t count{};read(input,count);
        if(!count || count>32768) throw std::runtime_error("invalid AI corpus count");
        std::size_t failures=0;std::uint64_t compared=0;
        for(std::uint32_t n=0;n<count;++n) {
            std::array<std::uint32_t,3> header{};read(input,header);
            const auto [entry,self,selected]=header;
            FrameImage frame{},expected_frame{};FieldImage image{},expected{};
            read(input,frame);read(input,image);read(input,expected_frame);read(input,expected);
            auto field=decode(image);auto a=decode(frame);std::optional<std::size_t> result;
            switch(entry) {
                case 0x407a70:result=ai_select_ahead(field,self);break;
                case 0x407b20:result=ai_select_behind(field,self);break;
                case 0x4073e0:ai_choose_passing_side(field,self);break;
                case 0x407c10:ai_follow_rival(field,self,a);break;
                case 0x407510:ai_pass_rival(field,self,a);break;
                case 0x407de0:ai_yield_to_rival(field,self,a);break;
                case 0x406700:ai_steer_to_target(field,self,a);break;
                case 0x405b50:ai_recover_stalled(field,self);break;
                default:throw std::runtime_error("unknown AI corpus entry");
            }
            encode(field,image);frame=encode(a);
            auto compare=[&](std::uint32_t actual,std::uint32_t want,const std::string& where) {
                ++compared;if(actual==want)return;
                if(++failures<=20) std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" "<<where
                    <<" expected "<<want<<" got "<<actual<<std::dec<<'\n';
            };
            compare(result?static_cast<std::uint32_t>(*result):0xffffffffu,selected,"selected");
            for(std::size_t i=0;i<frame.size();++i) compare(frame[i],expected_frame[i],"frame["+std::to_string(i)+"]");
            for(std::size_t i=0;i<6;++i) for(std::size_t k=0;k<image[i].size();++k)
                compare(image[i][k],expected[i][k],"racer["+std::to_string(i)+"] offset "+std::to_string(k*4));
        }
        if(input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("trailing AI corpus bytes");
        std::cout<<count<<" cases; "<<compared<<" words compared; "<<failures<<" mismatches; tolerance 0 ULP\n";
        return failures?1:0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
