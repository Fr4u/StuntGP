#include "sgn/presentation_interpolation.hpp"
#include "sgn/clock.hpp"
#include <atomic>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
#include <numbers>

namespace {std::atomic<std::uint64_t> allocations{};bool count_allocations{};}
void* operator new(std::size_t size){if(count_allocations)++allocations;if(void* value=std::malloc(size?size:1))return value;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete[](void* value) noexcept{std::free(value);}
void operator delete(void* value,std::size_t) noexcept{std::free(value);}
void operator delete[](void* value,std::size_t) noexcept{std::free(value);}

namespace {
using namespace sgn;using Matrix=std::array<float,16>;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void expect_near(float value,double expected,const char* message,double tolerance=2e-5){require(std::abs(value-expected)<=tolerance,message);}
Matrix pose(double degrees,Vec3 position={}){
    const auto angle=degrees*std::numbers::pi/180;const auto c=static_cast<float>(std::cos(angle)),s=static_cast<float>(std::sin(angle));
    return {c,s,0,0,-s,c,0,0,0,0,1,0,position.x,position.y,position.z,1};
}
Matrix product(const Matrix& a,const Matrix& b){
    Matrix result{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){double value=0;for(unsigned k=0;k<4;++k)value+=double(a[4*i+k])*b[4*k+j];result[4*i+j]=static_cast<float>(value);}return result;
}
std::shared_ptr<PresentationSnapshot> frame(std::uint64_t tick,const Matrix& world,double camera_angle=0,Vec3 camera_position={}){
    auto out=std::make_shared<PresentationSnapshot>();out->tick=tick;
    auto view=pose(-camera_angle);for(unsigned i=0;i<3;++i)view[12+i]=-(camera_position.x*view[i]+camera_position.y*view[4+i]+camera_position.z*view[8+i]);
    out->view.world_to_view=view;out->view.eye=camera_position;
    auto geometry=std::make_shared<LiveGeometry>();geometry->model.parts=1;geometry->model.textures={"test"};
    PresentationObject object;object.identity=7;object.mesh=0;object.geometry=std::move(geometry);
    object.parts.resize(1);object.model_to_view={product(world,view)};
    object.pose={{world[0],world[1],world[2]},{world[4],world[5],world[6]},{world[8],world[9],world[10]},
        {world[12],world[13],world[14]},1};out->objects.push_back(std::move(object));return out;
}
std::uint64_t fingerprint(const PresentationSnapshot& s){
    std::uint64_t result=s.tick+s.camera_continuity;
    const auto word=[&](std::uint32_t x){result=(result^x)*1099511628211ull;};
    for(float v:*s.view.world_to_view)word(std::bit_cast<std::uint32_t>(v));
    for(const auto& object:s.objects){word(static_cast<unsigned>(object.identity));word(static_cast<unsigned>(object.continuity));word(static_cast<unsigned>(object.mesh));
        for(const auto& matrix:object.model_to_view)for(float v:matrix)word(std::bit_cast<std::uint32_t>(v));
        for(const auto& part:object.parts)word(part.visible);for(const auto& name:object.geometry->model.textures)for(unsigned char c:name)word(c);}
    return result;
}
void rigid_and_camera(){
    PresentationInterpolator interpolation;
    auto a=frame(1,pose(0)),b=frame(2,pose(90,{10,0,0}));const auto ah=fingerprint(*a),bh=fingerprint(*b);
    interpolation.publish(a);interpolation.publish(b);
    require(interpolation.sample(0).objects[0].model_to_view[0]==a->objects[0].model_to_view[0],"previous endpoint changed source matrix");
    require(interpolation.sample(1).objects[0].model_to_view[0]==b->objects[0].model_to_view[0],"current endpoint changed source matrix");
    const auto& mid=interpolation.sample(0.5).objects[0].model_to_view[0];
    expect_near(mid[12],5,"translation midpoint");expect_near(mid[0],std::sqrt(0.5),"rotation midpoint x");expect_near(mid[1],std::sqrt(0.5),"rotation midpoint y");
    expect_near(mid[0]*mid[0]+mid[1]*mid[1],1,"matrix blending shrank rotation");
    const auto baseline=allocations.load();count_allocations=true;
    for(unsigned i=0;i<10000;++i)(void)interpolation.sample(double(i%101)/100);
    count_allocations=false;require(allocations.load()==baseline,"presentation sampling allocated heap storage");
    require(ah==fingerprint(*a)&&bh==fingerprint(*b),"sampling mutated immutable input snapshots");
    PresentationInterpolator wrap;wrap.publish(frame(1,pose(179)));wrap.publish(frame(2,pose(-179)));
    expect_near(wrap.sample(0.5).objects[0].model_to_view[0][0],-1,"rotation crossed long 358 degree path");
    PresentationInterpolator camera;const auto fixed=pose(0,{10,0,0});camera.publish(frame(1,fixed));camera.publish(frame(2,fixed,90,{10,0,0}));
    const auto& image=camera.sample(0.5).objects[0].model_to_view[0];
    expect_near(image[12],5*std::sqrt(0.5),"camera/world separation lost static object x");
    expect_near(image[13],-5*std::sqrt(0.5),"camera/world separation lost static object y");
}
void affine(){
    for(unsigned kind=0;kind<5;++kind){
        Matrix first=pose(0),last=pose(90);
        if(kind==0){for(unsigned i=0;i<3;++i){first[i]*=-2;last[i]*=-2;first[4+i]*=3;last[4+i]*=3;}}
        if(kind==1){for(unsigned i=0;i<3;++i){first[4+i]=0;last[4+i]=0;}}
        if(kind==2){for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)first[row*4+col]=last[row*4+col]=0;}
        if(kind==3){first[4]=0.6f;last=first;}
        if(kind==4){first[0]=0;first[5]=0;last=first;}
        PresentationInterpolator interpolation;interpolation.publish(frame(1,first));interpolation.publish(frame(2,last));
        const auto output=interpolation.sample(0.5).objects[0].model_to_view[0];
        for(float value:output)require(std::isfinite(value),"singular or mirrored affine became nonfinite");
        if(kind==0)expect_near(std::sqrt(output[0]*output[0]+output[1]*output[1]),2,"mirrored nonuniform scale changed");
        if(kind==1)expect_near(output[4]*output[4]+output[5]*output[5]+output[6]*output[6],0,"rank two flattened axis inflated");
        if(kind>=2)for(unsigned i=0;i<16;++i)expect_near(output[i],first[i],"constant singular/sheared matrix changed");
    }
}
void discontinuities(){
    for(unsigned kind=0;kind<5;++kind){
        PresentationInterpolator interpolation;auto a=frame(1,pose(0)),b=frame(kind==3?5:2,pose(0,{1000,0,0}));
        if(kind==0)++b->objects[0].continuity;if(kind==1)++b->camera_continuity;if(kind==2)++b->objects[0].mesh;
        interpolation.publish(a);if(kind==4)interpolation.invalidate_history();interpolation.publish(b);
        expect_near(interpolation.sample(0.2).objects[0].model_to_view[0][12],1000,"teleport/cut/LOD/pause blended old transform");
    }
    PresentationInterpolator paused;paused.publish(frame(1,pose(0)));paused.publish(frame(2,pose(0,{10,0,0})));
    paused.invalidate_history();expect_near(paused.sample(0).objects[0].model_to_view[0][12],10,"pause rewound completed frame");
    paused.publish(frame(3,pose(0,{11,0,0})));expect_near(paused.sample(0).objects[0].model_to_view[0][12],11,"resume consumed old interpolation history");
    auto changed=frame(4,pose(0,{12,0,0}));changed->objects[0].identity=8;paused.publish(changed);
    expect_near(paused.sample(0.1).objects[0].model_to_view[0][12],12,"new identity inherited old object transform");
    unsigned rejected=0;try{(void)paused.sample(-0.1);}catch(const FormatError&){++rejected;}
    try{(void)paused.sample(std::numeric_limits<double>::quiet_NaN());}catch(const FormatError&){++rejected;}
    try{paused.publish(changed);}catch(const FormatError&){++rejected;}
    auto duplicate=frame(5,pose(0));duplicate->objects.push_back(duplicate->objects.front());
    try{paused.publish(duplicate);}catch(const FormatError&){++rejected;}
    require(rejected==4,"invalid alpha, timestamp or identity accepted");
    const auto prior=paused.current();const auto prior_publications=paused.stats().publications;
    for(unsigned kind=0;kind<4;++kind){auto invalid=frame(5,pose(0));const float bad=std::numeric_limits<float>::quiet_NaN();
        if(kind==0)invalid->view.eye.x=bad;
        if(kind==1)invalid->view.far_plane=bad;
        if(kind==2){invalid->view.source_projection=pose(0);(*invalid->view.source_projection)[2]=bad;}
        if(kind==3){invalid->view.retail_viewport.emplace();invalid->view.retail_viewport->clip_far=bad;}
        try{paused.publish(invalid);}catch(const FormatError&){++rejected;}
        require(paused.current()==prior&&paused.stats().publications==prior_publications,"invalid publication replaced completed snapshot");
    }
    require(rejected==8,"nonfinite camera or projection accepted");
}
void cadence(){
    std::uint32_t expected_rng{};std::uint64_t total_samples{};
    for(unsigned hz:{30u,60u,75u,120u,144u,165u,240u,360u}){
        TickClock clock;PresentationInterpolator interpolation;std::uint32_t rng=17;std::int64_t previous{};
        for(std::int64_t rendered=1;rendered<=hz*2;++rendered){const auto now=rendered*1'000'000'000/hz;
            clock.advance(std::chrono::nanoseconds(now-previous),[&](auto tick){rng=rng*0x343fdu+0x269ec3u;interpolation.publish(frame(tick+1,pose(0,{static_cast<float>(tick+1),0,0})));});previous=now;
            const auto before_rng=rng;const auto before_tick=clock.ticks();
            if(auto snapshot=interpolation.current()){
                const auto before=fingerprint(*snapshot);const auto& draw=interpolation.sample(clock.alpha());
                const auto expected=clock.ticks()>1?double(clock.ticks()-1)+clock.alpha():1;
                expect_near(draw.objects[0].model_to_view[0][12],expected,"presentation cadence changed interpolation phase");
                require(fingerprint(*snapshot)==before,"cadence sample changed source snapshot");++total_samples;
            }
            require(rng==before_rng&&clock.ticks()==before_tick,"Present changed simulation RNG or tick count");
        }
        require(clock.ticks()==120,"display cadence changed fixed step count");
        if(hz==30)expected_rng=rng;else require(rng==expected_rng,"display cadence changed RNG sequence");
    }
    std::cout<<"{\"passed\":true,\"cadences\":8,\"ticks_each\":120,\"cadence_samples\":"<<total_samples<<",\"allocation_free_samples\":10000}\n";
}
}
int main(){try{rigid_and_camera();affine();discontinuities();cadence();return 0;}catch(const std::exception& error){count_allocations=false;std::cerr<<error.what()<<'\n';return 1;}}
