#include "sgn/world_packet_join.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <functional>
#include <algorithm>
#include <cstring>

thread_local bool forbid_allocations=false;
void* operator new(std::size_t n){if(forbid_allocations)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}

using namespace sgn;using namespace sgn::retail;using namespace sgn::diagnostics;
unsigned checks=0,cases=0;
void check(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
template<class F> bool no_alloc(F f){forbid_allocations=true;try{const bool b=f();forbid_allocations=false;return b;}catch(...){forbid_allocations=false;throw;}}
struct Fixture {
    std::array<WorldPolygonHeader,12> headers{};
    std::array<WorldPolygonHeader,2> clipped{};
    std::array<std::array<WorldPolygonPacket,8>,4> pools{};
    std::array<WorldPolygonVertex,16> vertices{};
    std::array<WorldPolygonAttribute,16> attributes{};
    std::array<std::uint16_t,16> indices{};
    std::array<PolygonSubmissionMaterial,4> materials{{{7,9,0},{36,9,0},{36,9,0},{88,9,0}}};
    std::array<TextureMotion,1> motions{};std::array<SkyTextureMotion,2> sky{};
    std::uint32_t sky_base=0,depth=1,quality=1,restriction=0,multi=1;
    ViewportState viewport{};PolygonSubmissionState scratch;
    WorldJoinFrameView frame;
    Fixture(){
        viewport.width=1280;viewport.height=720;viewport.selector=1;
        for(unsigned i=0;i<16;++i){indices[i]=static_cast<std::uint16_t>(i);attributes[i].vertex=static_cast<std::uint16_t>(i);attributes[i].color=0xffffffff;
            vertices[i].words={0x3f800000,0x40000000,0,0,0,0,0,0x3f000000};}
        for(unsigned i=0;i<12;++i){headers[i][0]=4;headers[i][1]=1;headers[i][2]=255;headers[i][3]=0x50;}
        headers[0][1]=0;headers[1][0]=3;headers[3][0]=5;headers[3][1]=2;
        for(unsigned p=0;p<4;++p)for(unsigned i=0;i<8;++i){auto& x=pools[p][i];x.header=world_polygon_header_ref(headers[(p*2+i)%12]);x.indices=indices;x.attributes=WorldPolygonAttributeView(std::span(attributes));x.vertices=vertices;x.inside_mask=63;}
        pools[0][2].header={};
        for(unsigned i=0;i<2;++i){pools[2][i].header=world_polygon_header_ref(headers[4+i]);pools[1][i].header=world_polygon_header_ref(headers[6+i]);pools[3][i].header=world_polygon_header_ref(headers[8+i]);}
        headers[6][3]=0x70;headers[7][3]=0x58;headers[8][3]=0x48;headers[9][3]=0x50;
        for(unsigned p=0;p<4;++p)frame.pools[p]=pools[p];
        frame.lists={WorldJoinRange{0,0,1},{0,1,3},{2,0,2},{0,1,1},{2,0,1},{0,3,1},{2,1,1},{1,0,2},{3,0,2}};
        frame.clipping={WorldJoinRange{0,0,1},{0,1,3},{1,0,2},{0,1,1},{1,0,0},{0,3,1},{1,0,0}};
        scratch.indexed_vertices.resize(256);scratch.alpha_vertices.resize(16);scratch.indices.resize(768);
    }
    PolygonSubmissionInputs input() const{return {materials,motions,sky,sky_base,depth,quality,restriction,multi,viewport};}
    PolygonSubmissionLists lists() const{PolygonSubmissionLists r;for(unsigned i=0;i<9;++i){auto x=frame.lists[i];auto s=x.first?frame.pools[x.pool].subspan(*x.first,x.count):std::span<const WorldPolygonPacket>{};if(i<7)r.categories[i]=s;else r.alpha[i-7]=s;}return r;}
    std::vector<WorldHeaderRange> owners() const{
        WorldHeaderOwner whole;whole.kind=WorldHeaderKind::model_table;whole.registry_token=123;whole.model_handle=122;whole.source_name="SAME.PMD";
        WorldHeaderOwner part=whole;part.kind=WorldHeaderKind::model_part;part.lod=0;part.part=0;part.source_lods={0,0,0,0};
        WorldHeaderOwner alias=part;alias.lod=1;alias.part=2;
        WorldHeaderOwner other=whole;other.registry_token=124;other.model_handle=123;
        return {{whole,WorldPolygonHeaderView(std::span(headers))},{part,WorldPolygonHeaderView(std::span(headers).first(6))},
            {alias,WorldPolygonHeaderView(std::span(headers).first(6))},{other,WorldPolygonHeaderView(std::span<const WorldPolygonHeader>{})}};
    }
};
struct Built {
    std::unique_ptr<WorldPacketJoinCapture> join;
    ImmutablePolygonSubmission submission;
};
Built build(bool enabled=true,unsigned views=1,bool segmented=false,unsigned mode=0,bool clipping=false,WorldJoinLimits limits={}){
    Fixture f;f.quality=mode%3;f.depth=(mode/3)%2;f.multi=(mode/6)%2;f.restriction=(mode/12)%2;
    PolygonDeviceContext device({});auto join=std::make_unique<WorldPacketJoinCapture>(enabled,limits);
    auto owners=f.owners();const auto began=join->begin(939,939,device,owners);check(began==enabled,"begin/off");
    auto out=std::make_shared<CapturedPolygonSubmission>();out->device=device.identity();out->source_tick=939;out->initial=device.snapshot();
    device.begin_capture(939,6,{{20000,2000000},30000});check(device.begin_scene()==0,"begin CPU scene");
    for(unsigned v=0;v<views;++v){
        f.viewport.selector=v+1;const auto in=f.input();
        if(enabled)check(no_alloc([&]{return join->preclip(f.frame,v);}),"allocation-free preclip");
        if(clipping&&v==0){f.clipped[0]=f.headers[1];f.clipped[0][0]=6;f.pools[0][1].header=world_polygon_header_ref(f.clipped[0]);f.frame.clip_headers=std::span(f.clipped).first(1);f.pools[0][3].header={};}
        if(enabled)check(no_alloc([&]{return join->before_submission(f.frame,in,device);}),"allocation-free before");
        submit_world_polygons(f.lists(),in,f.scratch,device);
        if(enabled)check(no_alloc([&]{return join->after_submission(device);}),"allocation-free after");
        if(segmented&&v+1<views){auto frame=device.seal_capture();out->segments.push_back({frame,{}});device.begin_capture(939,7+v,{{20000,2000000},30000});}
    }
    check(device.end_scene()==0,"end CPU scene");out->segments.push_back({device.seal_capture(),{}});out->final=device.snapshot();
    for(const auto& s:out->segments){out->commands+=s.device_frame->polygon_commands->commands.size();out->events+=s.device_frame->events.size();out->payload_bytes+=s.device_frame->polygon_commands->payload_bytes;}
    return {std::move(join),out}; // All source packet/header/context objects die here.
}
bool equal_commands(const CapturedPolygonFrame& a,const CapturedPolygonFrame& b){
    if(a.commands.size()!=b.commands.size()||a.payload_bytes!=b.payload_bytes)return false;
    for(std::size_t i=0;i<a.commands.size();++i){const auto& x=a.commands[i];const auto& y=b.commands[i];if(x.index()!=y.index())return false;
        if(const auto* r=std::get_if<CapturedRenderState>(&x)){const auto& s=std::get<CapturedRenderState>(y);if(r->state!=s.state||r->value!=s.value)return false;}
        if(const auto* t=std::get_if<CapturedTexture>(&x)){const auto& s=std::get<CapturedTexture>(y);if(t->stage!=s.stage||t->token!=s.token)return false;}
        if(const auto* t=std::get_if<CapturedTextureStageState>(&x)){const auto& s=std::get<CapturedTextureStageState>(y);if(t->stage!=s.stage||t->state!=s.state||t->value!=s.value)return false;}
        if(const auto* d=std::get_if<CapturedPolygonDraw>(&x)){const auto& e=std::get<CapturedPolygonDraw>(y);if(d->type!=e.type||d->fvf!=e.fvf||d->flags!=e.flags||d->indexed!=e.indexed||world_join_viewport(d->viewport)!=world_join_viewport(e.viewport)||d->indices!=e.indices||d->vertices.size()!=e.vertices.size())return false;
            for(std::size_t j=0;j<d->vertices.size();++j)if(d->vertices[j].words!=e.vertices[j].words)return false;}
    }return true;
}
void positive_matrix(){
    for(unsigned mode=0;mode<24;++mode){++cases;auto on=build(true,2,true,mode);auto off=build(false,2,true,mode);
        check(on.join->finish(on.submission),"join route matrix");check(on.join->result().status==WorldJoinStatus::derived_checked,"DERIVED_CHECKED");
        check(on.join->result().batches.size()==16,"batch boundaries per list/material");
        check(on.join->result().invocations.size()==2,"split views retained");
        check(on.join->result().batches[8].segment==1,"resource segment ordinal");
        check(on.join->matches_displayed(on.submission),"display match after source owners died");
        for(std::size_t i=0;i<2;++i){const auto& a=*on.submission->segments[i].device_frame;const auto& b=*off.submission->segments[i].device_frame;
            check(equal_commands(*a.polygon_commands,*b.polygon_commands),"on/off raw commands and payload unchanged");
            check(a.events.size()==b.events.size(),"on/off event count");
            for(std::size_t j=0;j<a.events.size();++j){const auto& x=a.events[j];const auto& y=b.events[j];check(x.sequence==y.sequence&&x.operation==y.operation&&x.arguments==y.arguments&&x.hresult==y.hresult&&x.cpu_accepted==y.cpu_accepted&&x.rejection==y.rejection&&x.polygon_command==y.polygon_command,"on/off exact event order");}
        }
        const auto& result=on.join->result();check(result.origins[0].membership_count==3,"all aliased memberships");
        check(result.owners[0].registry_token!=result.owners[3].registry_token&&result.owners[0].source_name==result.owners[3].source_name,"same filename distinct resource");
    }
}
void lifetime(){++cases;auto a=build();const auto recipe_count=a.submission.use_count();const auto frame_count=a.submission->segments[0].device_frame.use_count();
    check(a.join->finish(a.submission),"lifetime join");check(a.submission.use_count()==recipe_count,"no strong recipe retention");
    check(a.submission->segments[0].device_frame.use_count()==frame_count,"no strong nested-frame retention");
    auto clone=std::make_shared<CapturedPolygonSubmission>(*a.submission);a.submission.reset();check(a.join->matches_displayed(clone),"recovery clone survives expired top-level recipe");
    auto back=build();check(back.join->finish(back.submission),"back join");std::array<ImmutablePolygonSubmission,2> roles{clone,back.submission};
    check(a.join->matches_displayed(roles[0])&&!a.join->matches_displayed(roles[1]),"same tick and serial cannot substitute another front");
    std::swap(roles[0],roles[1]);check(!a.join->matches_displayed(roles[0])&&a.join->matches_displayed(roles[1]),"front/back exchange");
    auto replacement=std::make_shared<CapturedPolygonSubmission>(*clone);replacement->segments[0].device_frame=std::make_shared<CapturedPolygonDeviceFrame>(*clone->segments[0].device_frame);
    check(!a.join->matches_displayed(replacement),"equal contents new nested owner rejected");
    auto modified=std::make_shared<CapturedPolygonSubmission>(*clone);++modified->commands;check(!a.join->matches_displayed(modified),"root metadata mismatch");
    check(!a.join->matches_displayed({}),"no front");
    PolygonDeviceContext d({});check(!a.join->begin(940,940,d,{}),"pinned record not overwritten by940");
}
void clipping_and_bits(){++cases;auto a=build(true,1,false,1,true);check(a.join->finish(a.submission),"clip replacement/rejection join");
    bool replaced=false,rejected=false;for(const auto& p:a.join->result().occurrences){if(p.relation==WorldPreclipRelation::replaced){replaced=true;check(p.clip_header==0&&p.header[0]==6,"clip slot retained");check(a.join->result().origins[p.origin].header[0]==3,"preclip bytes retained");}rejected|=p.relation==WorldPreclipRelation::rejected;}
    check(replaced&&rejected,"both clip outcomes");
    ViewportState viewport;const std::uint32_t signaling=0x7f800001;std::memcpy(&viewport.origin_x,&signaling,4);check(world_join_viewport(viewport)[20]==signaling,"sNaN viewport payload copied as bits");
}
void mutations(){
    for(unsigned mutation=0;mutation<12;++mutation){++cases;auto a=build();auto s=std::make_shared<CapturedPolygonSubmission>(*a.submission);
        auto f=std::make_shared<CapturedPolygonDeviceFrame>(*s->segments[0].device_frame);auto c=std::make_shared<CapturedPolygonFrame>(*f->polygon_commands);f->polygon_commands=c;s->segments[0].device_frame=f;
        auto event=std::find_if(f->events.begin(),f->events.end(),[](const auto& e){return e.operation==PolygonDeviceOperation::draw_indexed;});
        auto& draw=std::get<CapturedPolygonDraw>(c->commands[*event->polygon_command]);
        switch(mutation){case 0:draw.indices[0]=1;break;case 1:draw.vertices.pop_back();break;case 2:draw.viewport.selector=99;break;
        case 3:event->sequence+=1;break;case 4:event->polygon_command=0;break;case 5:event->cpu_accepted=false;break;
        case 6:++s->source_tick;break;case 7:f->events.pop_back();break;case 8:++s->commands;break;
        case 9:for(auto& cmd:c->commands)if(auto* t=std::get_if<CapturedTexture>(&cmd)){++t->token;break;}break;
        case 10:{auto state=std::find_if(f->events.begin(),f->events.end(),[](const auto& e){return e.operation==PolygonDeviceOperation::render_state;});
            ++state->arguments[1];++std::get<CapturedRenderState>(c->commands[*state->polygon_command]).value;break;}
        case 11:draw.flags=0x14;event->arguments[2]=0x14;break;}
        check(!a.join->finish(s),"mutated capture rejected");check(a.join->result().batches.empty(),"no partial batch claims on mismatch");check(!a.join->matches_displayed(s),"bad record cannot match");
    }
}
void paused_identity_and_bank(){
    ++cases;Fixture f;PolygonDeviceContext device({});std::array<std::unique_ptr<WorldPacketJoinCapture>,2> joins;
    std::array<ImmutablePolygonSubmission,2> recipes;
    for(unsigned i=0;i<2;++i){joins[i]=std::make_unique<WorldPacketJoinCapture>(true);auto owners=f.owners();check(joins[i]->begin(939+i,100,device,owners),"paused distinct serial begin");
        device.begin_capture(100,6,{{20000,2000000},30000});device.begin_scene();check(joins[i]->preclip(f.frame,0),"paused preclip");
        check(joins[i]->before_submission(f.frame,f.input(),device),"paused before");submit_world_polygons(f.lists(),f.input(),f.scratch,device);check(joins[i]->after_submission(device),"paused after");device.end_scene();
        auto frame=device.seal_capture();auto recipe=std::make_shared<CapturedPolygonSubmission>();recipe->device=device.identity();recipe->source_tick=100;recipe->initial=frame->initial;recipe->final=frame->final;
        auto bank=std::make_shared<PolygonGpuTextures>();std::weak_ptr<const PolygonGpuTextures> weak_bank=bank;recipe->segments.push_back({frame,bank});recipe->commands=frame->polygon_commands->commands.size();recipe->events=frame->events.size();
        check(joins[i]->finish(recipe),"paused finish");check(bank.use_count()==2,"no strong texture bank retained");bank.reset();recipe->segments[0].textures.reset();check(weak_bank.expired(),"released bank expires with diagnostic alive");recipes[i]=recipe;
    }
    check(joins[0]->result().accepted_serial==939&&joins[1]->result().accepted_serial==940,"serials copied independently");
    check(joins[0]->matches_displayed(recipes[0])&&!joins[0]->matches_displayed(recipes[1]),"same device/tick distinct segment identities");
    check(joins[1]->matches_displayed(recipes[1])&&!joins[1]->matches_displayed(recipes[0]),"back940 cannot stand for front939");
    for(unsigned kind=0;kind<2;++kind){++cases;WorldJoinLimits limits;if(kind==0)limits.events=0;else limits.segments=0;auto a=build(true,1,false,0,false,limits);check(!a.join->finish(a.submission)&&a.join->result().error==WorldJoinError::capacity,"offline bound rejects without a partial result");}
}
void boundaries(){
    Fixture f;PolygonDeviceContext device({});auto owners=f.owners();
    for(unsigned kind=0;kind<6;++kind){++cases;WorldJoinLimits limits;if(kind==0)limits.origins=0;if(kind==1)limits.memberships=0;if(kind==2)limits.views=0;if(kind==3)limits.occurrences=0;if(kind==4)limits.owners=0;
        auto j=std::make_unique<WorldPacketJoinCapture>(true,limits);const auto ok=j->begin(1,1,device,owners);
        if(kind==4){check(!ok,"owner capacity");continue;}check(ok,"boundary begin");
        if(kind<3){check(!no_alloc([&]{return j->preclip(f.frame,0);}),"precopy budget rejection");continue;}
        if(kind==5){check(!j->after_submission(device),"after without preclip");continue;}
        check(j->preclip(f.frame,0),"preclip before post capacity");check(!j->before_submission(f.frame,f.input(),device),"postcopy budget rejection");
    }
    ++cases;auto j=std::make_unique<WorldPacketJoinCapture>(true);check(j->begin(1,1,device,owners),"foreign sink begin");check(j->preclip(f.frame,0),"foreign sink preclip");
    PolygonDeviceContext other({});check(!j->before_submission(f.frame,f.input(),other),"different exact sink rejected");
    ++cases;auto null=build();check(!null.join->finish({})&&null.join->result().status==WorldJoinStatus::no_recipe,"nonrendered result cannot publish");
    ++cases;auto empty=std::make_unique<WorldPacketJoinCapture>(true);check(empty->begin(1,1,device,owners),"empty lists begin");
    for(auto& r:f.frame.lists)r.count=0;check(empty->preclip(f.frame,0),"empty lists preclip");
    device.begin_capture(1,1,{{200,20000},200});device.begin_scene();check(empty->before_submission(f.frame,f.input(),device),"empty before");submit_world_polygons(f.lists(),f.input(),f.scratch,device);check(empty->after_submission(device),"empty after");device.end_scene();
    auto df=device.seal_capture();auto s=std::make_shared<CapturedPolygonSubmission>();s->device=device.identity();s->source_tick=1;s->initial=df->initial;s->final=df->final;s->segments.push_back({df,{}});s->events=df->events.size();s->commands=df->polygon_commands->commands.size();
    check(empty->finish(s)&&empty->result().batches.empty(),"empty source lists retain state operations without fake draw");
}
int main(){try{positive_matrix();lifetime();clipping_and_bits();mutations();boundaries();paused_identity_and_bank();std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"gpu_calls\":0,\"source_fixture_only\":true}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
