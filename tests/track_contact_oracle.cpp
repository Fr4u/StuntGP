#include "sgn/track_contact.hpp"
#include "sgn/track_collision.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace {
using namespace sgn::retail::contact;
using Record=std::array<std::uint8_t,0x8400>;
std::uint32_t word(const Record& b,std::size_t at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void word(Record& b,std::size_t at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float real(const Record& b,std::size_t at){return std::bit_cast<float>(word(b,at));}
sgn::Vec3 vec(const Record& b,std::size_t at){return {real(b,at),real(b,at+4),real(b,at+8)};}
void real(Record& b,std::size_t at,float x){word(b,at,std::bit_cast<std::uint32_t>(x));}
void vec(Record& b,std::size_t at,sgn::Vec3 v){real(b,at,v.x);real(b,at+4,v.y);real(b,at+8,v.z);}
void transfer(Record& b,Pose& p,ContactBody& body,ContactProbe& probe,WheelContact& wheel,unsigned index,bool read){
    auto f=[&](float& x,std::size_t at){if(read)x=real(b,at);else real(b,at,x);};
    auto i=[&](std::int32_t& x,std::size_t at){if(read)x=std::bit_cast<std::int32_t>(word(b,at));else word(b,at,std::bit_cast<std::uint32_t>(x));};
    auto v=[&](Vector4& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);if(read)x.opaque=word(b,at+12);else word(b,at+12,x.opaque);};
    v(p.position,0x1bac);v(p.previous_position,0x1bbc);v(p.velocity,0x1c8c);
    v(body.position,0x7270);v(body.previous_position,0x7280);v(body.velocity,0x72a0);
    for(unsigned j=0;j<16;++j){f(p.orientation[j],0x1bcc+j*4);f(p.incremental[j],0x1c0c+j*4);f(body.orientation[j],0x72b8+j*4);f(body.incremental[j],0x72f8+j*4);}
    auto at=0x73b4+index*0x4c;v(probe.local,at);v(probe.current,at+16);v(probe.previous,at+32);v(probe.velocity,at+48);
    if(read)probe.contact_iterations=word(b,at+64);else word(b,at+64,probe.contact_iterations);f(probe.mass_fraction,at+68);f(probe.radius,at+72);
    f(body.mass,0x72b0);at=0x1fe0+index*0x98;
    f(wheel.filtered_load,at+12);f(wheel.current_load,at+128);v(wheel.normal,at+76);
    i(wheel.countdown,at+112);i(wheel.previous_type,at+116);i(wheel.type,at+120);i(wheel.changed,at+124);
    f(wheel.grip,at+132);f(wheel.roughness,at+136);f(wheel.rolling,at+140);
}
void apply(Record& b,std::uint32_t entry,unsigned index){
    Pose p;ContactBody body;ContactProbe probe;WheelContact wheel;transfer(b,p,body,probe,wheel,index,true);
    switch(entry){
    case 0x4088b0:pack_pose(p,body);break;
    case 0x408920:unpack_pose(body,p);break;
    case 0x421cc0:update_probe_position(body,probe);break;
    case 0x421d90:update_probe_velocity(body,probe);break;
    case 0x4091e0:apply_material({b[0x83e0],b[0x83e1],b[0x83e2],b[0x83e3]},std::bit_cast<std::int32_t>(word(b,0x83f0)),wheel);break;
    case 0x409e27:filter_wheel_load(real(b,0x83f4),wheel);break;
    case 0x421c30:{auto motion=vec(b,0x80e8);reflect_motion(vec(b,0x8198),real(b,0x83f4),motion);vec(b,0x80e8,motion);break;}
    case 0x422020:{Matrix4 inertia{},accumulated{};for(unsigned j=0;j<16;++j){inertia[j]=real(b,0x8280+j*4);accumulated[j]=real(b,0x82c0+j*4);}
        angular_response(body,probe,vec(b,0x8198),real(b,0x83f4),inertia,accumulated);for(unsigned j=0;j<16;++j)real(b,0x82c0+j*4,accumulated[j]);break;}
    case 0x4093e0:{ImpactParameters parameters;ImpactAccumulator accumulator;
        for(unsigned j=0;j<128;++j)parameters.restitution_curve[j]=real(b,0x6000+j*4);
        for(unsigned j=0;j<16;++j){parameters.inertia[j]=real(b,0x8280+j*4);accumulator.rotation_delta[j]=real(b,0x82c0+j*4);}
        parameters.gravity=real(b,0x6200);parameters.inverse_probe_count=real(b,0x6204);parameters.effective_mass=real(b,0x2618);
        accumulator.collision_count=word(b,0x71ec);accumulator.velocity_delta=vec(b,0x8300);accumulator.normal_speed=real(b,0x83f8);accumulator.touched=word(b,0x83fc)!=0;
        auto motion=vec(b,0x80e8);accumulate_impact(body,probe,vec(b,0x8198),motion,parameters,accumulator);vec(b,0x80e8,motion);
        for(unsigned j=0;j<16;++j)real(b,0x82c0+j*4,accumulator.rotation_delta[j]);vec(b,0x8300,accumulator.velocity_delta);real(b,0x83f8,accumulator.normal_speed);word(b,0x83fc,accumulator.touched);word(b,0x71ec,accumulator.collision_count);break;}
    case 0x420a70:{auto band=plane_band(vec(b,0x80c4),vec(b,0x80d4),vec(b,0x83c0),vec(b,0x83d0),real(b,0x8154));real(b,0x8168,band.endpoint_distance);real(b,0x816c,band.start_distance);word(b,0x83fc,band.accepted);break;}
    default:throw std::runtime_error("unknown contact case entry");
    }
    transfer(b,p,body,probe,wheel,index,false);
}
void read(std::ifstream& in,void* buffer,std::size_t n){if(!in.read(static_cast<char*>(buffer),static_cast<std::streamsize>(n)))throw std::runtime_error("short contact corpus");}
std::uint32_t read_word(std::ifstream& in){std::uint32_t x;read(in,&x,4);return x;}
int compare(const char* path){
    std::ifstream in(path,std::ios::binary);char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNTC001")throw std::runtime_error("wrong contact corpus");
    auto count=read_word(in);if(count>100000)throw std::runtime_error("too many contact cases");std::uint64_t words=0;unsigned errors=0;
    for(unsigned n=0;n<count;++n){auto entry=read_word(in),index=read_word(in);Record initial{},expected{};read(in,initial.data(),initial.size());read(in,expected.data(),expected.size());if(index>=4)throw std::runtime_error("wheel index out of bounds");auto actual=initial;apply(actual,entry,index);
        for(std::size_t at=0;at<actual.size();at+=4){
            if((entry==0x421cc0 && at==0x73d0+index*0x4c)||(entry==0x421d90 && at==0x73f0+index*0x4c))continue; // documented undefined retail stack lanes only
            if((entry==0x422020||entry==0x4093e0)&&at==0x82cc)continue; // 0044F8E0 undefined first-column fourth lane
            auto x=word(actual,at),y=word(expected,at);if(x==y){++words;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<y<<" actual "<<x<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing contact corpus bytes");
    std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<words<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
}
void remainder_witness(const TrackGeometry& track){
    // Original 00420FE0 on TRACK1 polygon 0 accepts a contact slightly behind
    // this start. Expected words below come from the unmodified retail code.
    SweepInput input{{14653.2978515625f,200,25772.3203125f},{0.001f,-0.0f,-0.0f},12,1};
    const auto first=sweep_sphere(track,input);
    if(!first.hit||std::bit_cast<std::uint32_t>(first.remaining)!=0xc0787fffu||
       std::bit_cast<std::uint32_t>(first.start.x)!=0x4664f4c6u)
        throw std::runtime_error("retail negative remainder witness differs");
    auto motion=input.motion;reflect_motion(first.normal,0.05f,motion);
    const auto second=sweep_sphere(track,{first.start,motion,input.radius,first.remaining});
    if(second.hit||std::bit_cast<std::uint32_t>(second.remaining)!=0xc0787fffu||
       std::bit_cast<std::uint32_t>(second.end.x)!=0x4664f4c6u||second.end.y!=200||second.end.z!=25772.3203125f)
        throw std::runtime_error("retail negative remainder chaining differs");
    const auto greater_than_one=sweep_sphere(track,{{14653.193359375f,200,25772.3203125f},{0.2f,0,0},12,2});
    if(!greater_than_one.hit||greater_than_one.remaining!=1||
       std::bit_cast<std::uint32_t>(greater_than_one.end.x)!=0x4664f660u||
       std::bit_cast<std::uint32_t>(greater_than_one.start.y)!=0x4348199au)
        throw std::runtime_error("retail movement scale above one differs");
    std::cout<<"TRACK1 signed remainder: two chained queries and scale above one agree with original\n";
}
int assets(const char* root){
    auto path=std::filesystem::path(root)/"meshdata";unsigned count=0;
    for(const auto& entry:std::filesystem::directory_iterator(path)){
        if(entry.path().extension()!=".PMD")continue;auto raw=sgn::read_file(entry.path());
        if(raw.size()<328||raw[0x18]!=1||std::string_view(reinterpret_cast<const char*>(raw.data()),9)!="PMD V1.83")continue;
        const auto track=decode_track_collision(raw);for(const auto& n:track.nodes){find_polygon_list(track,n.split,10);find_polygon_list(track,n.split,200);}
        if(sgn::canonical(entry.path().filename().string())=="track1.pmd")remainder_witness(track);
        ++count;std::cout<<entry.path().filename().string()<<": "<<track.polygons.size()<<" polygon slots, "<<track.nodes.size()<<" node slots\n";
    }
    if(count<25)throw std::runtime_error("expected at least 25 retail tracks");std::cout<<"decoded "<<count<<" original track collision meshes\n";return 0;
}
int geometry_corpus(const char* root,const char* destination){
    std::ofstream out(destination,std::ios::binary);out.write("SGNTG001",8);
    auto write_word=[&](std::uint32_t x){out.write(reinterpret_cast<const char*>(&x),4);};
    auto write_real=[&](float x){write_word(std::bit_cast<std::uint32_t>(x));};
    auto write_vec=[&](sgn::Vec3 x){write_real(x.x);write_real(x.y);write_real(x.z);};
    auto folder=std::filesystem::path(root)/"meshdata";
    std::mt19937 rng(0x504d4443);
    for(const auto& item:std::filesystem::directory_iterator(folder)){
        if(item.path().extension()!=".PMD")continue;auto raw=sgn::read_file(item.path());
        if(raw.size()<328||raw[0x18]!=1||std::string_view(reinterpret_cast<const char*>(raw.data()),9)!="PMD V1.83")continue;
        auto track=decode_track_collision(raw);auto name=item.path().filename().string();write_word(static_cast<std::uint32_t>(name.size()));out.write(name.data(),static_cast<std::streamsize>(name.size()));
        constexpr unsigned samples=160;write_word(samples*2);
        for(unsigned i=0;i<samples;++i){const auto& node=track.nodes[rng()%track.nodes.size()];auto point=node.split;
            if(i%2){point.x+=static_cast<float>(static_cast<int>(rng()%2001)-1000);point.y+=static_cast<float>(static_cast<int>(rng()%2001)-1000);point.z+=static_cast<float>(static_cast<int>(rng()%2001)-1000);}
            float radius=i%2?track.radius_threshold[0]+1:track.radius_threshold[0];
            write_word(0);write_word(0);write_vec(point);write_real(radius);write_word(find_polygon_list(track,point,radius));
        }
        for(unsigned i=0;i<samples;++i){auto id=static_cast<unsigned>(rng()%track.polygons.size());while(track.polygons[id].vertex_count<3)id=(id+1)%static_cast<unsigned>(track.polygons.size());
            const auto& polygon=track.polygons[id];auto point=track.vertices.at(track.indices.at(polygon.first_index));
            if(i%3==1){point={};for(unsigned k=0;k<polygon.vertex_count;++k)point=point+track.vertices.at(track.indices.at(polygon.first_index+k));point=point*(1.f/polygon.vertex_count);}
            if(i%3==2){point.x+=10000;point.z-=15000;}
            write_word(1);write_word(id);write_vec(point);write_real(0);write_word(inside_polygon(track,polygon,point));
        }
    }
    write_word(0);if(!out)throw std::runtime_error("cannot write geometry corpus");return 0;
}
int sweep_corpus(const char* root,const char* destination,const char* filter){
    std::ofstream out(destination,std::ios::binary);out.write("SGNTS002",8);
    auto u=[&](std::uint32_t x){out.write(reinterpret_cast<const char*>(&x),4);};
    auto f=[&](float x){u(std::bit_cast<std::uint32_t>(x));};auto v=[&](sgn::Vec3 x){f(x.x);f(x.y);f(x.z);};
    std::mt19937 rng(0x53574550);
    for(const auto& file:std::filesystem::directory_iterator(std::filesystem::path(root)/"meshdata")){
        auto name=file.path().filename().string();if(file.path().extension()!=".PMD"||(*filter && name!=filter))continue;
        auto raw=sgn::read_file(file.path());if(raw.size()<328||raw[0x18]!=1||std::string_view(reinterpret_cast<const char*>(raw.data()),9)!="PMD V1.83")continue;
        auto track=decode_track_collision(raw);u(static_cast<unsigned>(name.size()));out.write(name.data(),static_cast<std::streamsize>(name.size()));
        const unsigned cases=name=="track1.PMD"?67u:64u;u(cases);SweepInput chained{};
        for(unsigned i=0;i<cases;++i){auto id=static_cast<unsigned>(rng()%track.polygons.size());while(track.polygons[id].vertex_count<3)id=(id+1)%static_cast<unsigned>(track.polygons.size());auto p=track.polygons[id];
            const auto a=track.vertices.at(track.indices.at(p.first_index)),b=track.vertices.at(track.indices.at(p.first_index+1));sgn::Vec3 point{};
            for(unsigned k=0;k<p.vertex_count;++k)point=point+track.vertices.at(track.indices.at(p.first_index+k));point=point*(1.f/p.vertex_count);
            if(i%3==1)point=a;else if(i%3==2)point=(a+b)*0.5f;
            auto n=track.normals.at(p.normal);const float radius=std::array{4.f,12.f,30.f,60.f,125.f,250.f}[i%6];
            SweepInput input{point+n*(radius*(0.5f+static_cast<float>(i%5))),n*(-radius*(1.f+static_cast<float>(i%4))),radius,std::array{1.f,0.5f,0.1f,0.f}[i%4]};
            if(i%7==0)input.motion={};if(i%8==0)input.motion.x+=radius*0.2f;
            if(i==64)input={{14653.2978515625f,200,25772.3203125f},{0.001f,-0.0f,-0.0f},12,1};
            else if(i==65)input=chained;
            else if(i==66)input={{14653.193359375f,200,25772.3203125f},{0.2f,0,0},12,2};
            auto result=sweep_sphere(track,input);
            if(i==64){auto motion=input.motion;reflect_motion(result.normal,0.05f,motion);chained={result.start,motion,input.radius,result.remaining};}
            v(input.start);v(input.motion);f(input.radius);f(input.remaining);
            u(result.hit);v(result.start);v(result.end);v(result.direction);v(result.step);f(result.remaining);f(result.step_length);
            u(result.polygon);v(result.normal);v(result.center);v(result.surface);u(result.normal_payload_defined);u(result.normal_payload);
        }
    }
    u(0);if(!out)throw std::runtime_error("cannot write sweep corpus");return 0;
}
int parallel_edge_regression(const char* path){
    auto track=decode_track_collision(sgn::read_file(path));
    PolygonQuery q{{34357.828125f,1368.720947265625f,19105.94921875f},{34357.828125f,1618.720947265625f,19105.94921875f},{0,250,0},{0,1,0},250,62500,250};
    auto hit=intersect_polygon(track,742,q);
    if(!hit.hit||std::bit_cast<std::uint32_t>(hit.center.y)!=std::bit_cast<std::uint32_t>(1392.8216552734375f)||
       std::bit_cast<std::uint32_t>(hit.normal.x)!=0xbf0b4b55u||std::bit_cast<std::uint32_t>(hit.normal.y)!=0xbf4db891u||std::bit_cast<std::uint32_t>(hit.normal.z)!=0xbe76f728u)
        throw std::runtime_error("parallel edge replaced the earlier retail collision");
    auto p=track.polygons[742];auto a=track.vertices[track.indices[p.first_index+1]],b=track.vertices[track.indices[p.first_index+2]];
    if(intersect_edge(a,b,q,1000000).hit)throw std::runtime_error("parallel edge incorrectly accepted NaN");
    std::cout<<"parallel-edge/NaN witness agrees with original TRACK7 polygon742\n";
    return 0;
}
}
int main(int argc,char** argv){try{if(argc==3&&std::string(argv[1])=="--remainder-witness"){remainder_witness(decode_track_collision(sgn::read_file(argv[2])));return 0;}if(argc==3&&std::string(argv[1])=="--parallel-edge")return parallel_edge_regression(argv[2]);if((argc==4||argc==5)&&std::string(argv[1])=="--sweep-corpus")return sweep_corpus(argv[2],argv[3],argc==5?argv[4]:"");if(argc==4&&std::string(argv[1])=="--geometry-corpus")return geometry_corpus(argv[2],argv[3]);if(argc==3&&std::string(argv[1])=="--assets")return assets(argv[2]);if(argc==2)return compare(argv[1]);throw std::runtime_error("usage: track_contact_oracle corpus | --assets retail-root | --remainder-witness track1.PMD");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
