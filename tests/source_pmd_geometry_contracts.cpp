#include "source_pmd_geometry_fixture.hpp"
#include "sgn/live_draw_batch.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <iostream>
#include <limits>
#include <thread>

namespace {
using namespace geometry_receiving;
void matrices(const std::filesystem::path& root,const std::filesystem::path& fixtures,bool published=false){
    std::ifstream in(fixtures/"original-matrices.dat",std::ios::binary);magic(in,"SGNLM001");const auto count=word(in);check(count==8,"pinned original matrix case count");
    std::uint64_t total=0,parts=0;
    Fixture f(root);
    for(unsigned c=0;c<count;++c){const auto name=string(in);const auto lod=word(in),n=word(in);const auto id=f.load(name);const auto model=f.model(id);
        check(n==model.part_count(),"original matrix part count");CameraState camera;read(in,camera.position);read(in,camera.world_to_camera_rotation);CameraModelTransform outer;read(in,outer);
        for(unsigned i=0;i<n;++i){const auto flags=word(in);check(flags<=65535,"original flag domain");SourcePmdTransform transform;read(in,transform);
            model.part(lod,i).flags().set(static_cast<std::uint16_t>(flags));model.part(lod,i).transform().set(transform);}
        ViewMatrixArena arena;reset_view_matrix_cursor(arena);const auto identity=vehicle_identity();
        if(published)allocate_model_part_matrices(model,lod,arena);
        const auto before=snapshot(f.bank,model.allocation());const SourcePmdGeometryAdapter adapter(model,"");
        const auto views=published?adapter.capture_published_views(lod,camera,outer,arena,identity):adapter.capture_views(lod,camera,outer);
        check(views.parts.size()==n&&views.model_to_view.size()==n,"actual canonical matrix consumer count");
        for(unsigned i=0;i<n;++i){for(unsigned j=0;j<16;++j){const auto expected=word(in),actual=std::bit_cast<std::uint32_t>(views.model_to_view[i][j]);
                if(expected!=actual){std::cerr<<"original case="<<c<<" name="<<name<<" lod="<<lod<<" part="<<i<<" word="<<j<<" expected="<<std::hex<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("original matrix bit mismatch");}++total;}
            const auto visible=word(in);check(visible<=1&&views.parts[i].visible==(visible==1),"original visibility mismatch");++parts;}
        check(before==snapshot(f.bank,model.allocation()),"read-only view capture mutated actual source allocation");(void)f.bank.release_42e980(model.allocation());
    }
    check(in.peek()==std::char_traits<char>::eof()&&parts==166&&total==2656,"original corpus complete coverage");
    std::cout<<"{\"original_cases\":8,\"original_parts\":"<<parts<<",\"matrix_words\":"<<total<<",\"allowed_ulp\":0}\n";
}
void geometry(const std::filesystem::path& root,const std::filesystem::path& fixtures){
    std::ifstream in(fixtures/"static-geometry.dat",std::ios::binary);magic(in,"SGNPG001");const auto cases=word(in);check(cases==8,"static geometry case count");Fixture f(root);std::uint64_t vertices=0;
    for(unsigned c=0;c<cases;++c){const auto name=string(in),prefix=string(in);const auto slot=word(in),lod=word(in);const auto start=in.tellg();
        const auto kind=word(in),parts=word(in),lods=word(in),textures=word(in),verts=word(in),surfaces=word(in);(void)kind;(void)parts;(void)lods;
        check(textures<65536&&verts<1000000&&surfaces<1000000,"static corpus bounds");for(unsigned t=0;t<textures;++t)(void)string(in);
        in.seekg(std::streamoff(verts)*36+std::streamoff(surfaces)*28+24,std::ios::cur);const auto end=in.tellg();check(end>=start,"static corpus range");
        Bytes expected(static_cast<std::size_t>(end-start));in.seekg(start);in.read(reinterpret_cast<char*>(expected.data()),static_cast<std::streamsize>(expected.size()));check(bool(in),"truncated static geometry");
        const auto model=f.model(f.load(name));const auto before=snapshot(f.bank,model.allocation());const SourcePmdGeometryAdapter adapter(model,prefix);
        check(adapter.source_lod_index(slot)==lod,"source LOD alias mismatch");const auto result=adapter.capture(slot);
        check(encode(result)==expected,"canonical transport differs from independent raw-PMD/PE-table static fixture");
        check(before==snapshot(f.bank,model.allocation()),"geometry capture changed canonical source bytes");
        for(const auto& texture:result.model.textures){const auto image=f.assets.texture(texture);check(image.width&&image.height&&!image.rgba.empty(),"actual canonical texture unresolved");}
        vertices+=verts;(void)f.bank.release_42e980(model.allocation());
    }
    check(in.peek()==std::char_traits<char>::eof()&&vertices==109647,"static geometry complete coverage");
    std::cout<<"{\"static_cases\":8,\"transport_vertices\":"<<vertices<<",\"original_geometry_execution\":false}\n";
}
void execute(std::string_view name,const std::filesystem::path& root,const std::filesystem::path& fixtures){
    if(name=="original_matrices"||name=="original_published_matrices"){matrices(root,fixtures,name=="original_published_matrices");return;}if(name=="static_geometry"){geometry(root,fixtures);return;}
    Fixture f(root);const auto id=f.load();const auto model=f.model(id);const SourcePmdGeometryAdapter adapter(model,"");
    if(name=="mutations"){
        const auto original=adapter.capture(0);const auto body=model.part(0,0);const auto header=body.attributes().at(0);const auto a=header.value();const auto first=std::uint32_t(a[6])|(std::uint32_t(a[7])<<8);
        const auto ci=model.corner_indices().at(first).value();const auto coordinate=body.coordinates().at(ci);const auto c=coordinate.value();const auto vertex=body.vertices().at(c[2]&0xffff);const auto v=vertex.value();
        auto xyz=v;xyz.x+=1;vertex.set(xyz);check(encode(adapter.capture(0))!=encode(original),"canonical XYZ mutation ignored");vertex.set(v);
        auto uv=c;uv[0]=std::bit_cast<std::uint32_t>(std::bit_cast<float>(c[0])+0.25f);coordinate.set(uv);check(encode(adapter.capture(0))!=encode(original),"canonical UV mutation ignored");coordinate.set(c);
        auto color=c;color[3]^=0x123456;coordinate.set(color);check(encode(adapter.capture(0))!=encode(original),"canonical color mutation ignored");coordinate.set(c);
        auto h=a;h[1]=static_cast<std::uint8_t>((a[1]+1)%model.texture_count());header.set(h);check(adapter.capture(0).model.surfaces[0].texture==h[1],"canonical material selector ignored");header.set(a);
        const auto index=model.corner_indices().at(first);index.set(model.corner_indices().at(first+1).value());check(encode(adapter.capture(0))!=encode(original),"canonical corner index mutation ignored");index.set(ci);
        check(encode(adapter.capture(0))==encode(original),"mutation restore changed transport");
    }else if(name=="materials"){
        const auto part=model.part(0,0);const auto header=part.attributes().at(0);const auto original=header.value();const auto model_flags=model.descriptor().field<std::uint16_t>(2);const auto old_model=model_flags.value(),old_part=part.flags_02().value();
        const auto first=std::uint32_t(original[6])|(std::uint32_t(original[7])<<8);const auto coordinate=part.coordinates().at(model.corner_indices().at(first).value());const auto old_coordinate=coordinate.value();
        // Uniform color across this polygon isolates material/alpha from the independent strip-order test.
        std::vector<std::pair<SourcePmdField<std::array<std::uint32_t,4>>,std::array<std::uint32_t,4>>> saved;
        for(unsigned j=0;j<original[0];++j){auto field=part.coordinates().at(model.corner_indices().at(first+j).value());auto c=field.value();saved.emplace_back(field,c);c[3]=0x12345678;field.set(c);}
        for(unsigned forced=0;forced<2;++forced){model_flags.set(static_cast<std::uint16_t>(forced?8:0));part.flags_02().set(static_cast<std::uint16_t>(forced?0x180:0));
            for(unsigned flags=0;flags<256;++flags){auto a=original;a[2]=137;a[3]=static_cast<std::uint8_t>(flags);header.set(a);const auto out=adapter.capture(0);const bool trans=(flags&64)!=0||forced!=0;const auto mode=flags&0x78;
                const auto expected=!trans?LiveBlend::opaque:mode==0x70?LiveBlend::multiply:mode==0x58?LiveBlend::add:(mode==0x48||mode==0x50)?LiveBlend::alpha:LiveBlend::alpha_add;
                const auto color=!trans?0xff345678u:mode==0x50?0x12345678u:0x89345678u;
                check(out.materials[0].blend==expected&&out.model.surfaces[0].translucent==trans&&out.model.surfaces[0].two_sided==((flags&0x80)!=0),"source material selector branch mismatch");
                for(unsigned v=0;v<out.model.surfaces[0].count;++v)check(out.model.vertices[v].color==color,"source color branch mismatch");}}
        for(auto i=saved.rbegin();i!=saved.rend();++i)i->first.set(i->second);header.set(original);model_flags.set(old_model);part.flags_02().set(old_part);check(coordinate.value()==old_coordinate,"color restore");
    }else if(name=="strip_table"){
        const auto table=read_file(fixtures.parent_path()/"strip-table.bin");check(table.size()==1024,"PE count-indexed table size");
        const auto p=model.part(0,0);check(p.coordinates().size()>=15&&p.vertices().size()>0&&model.corner_indices().size()>=15,"actual car1 mutation storage domain");
        // Explicit receiving mutation in actual car1 bank storage. This is not a new game rule.
        model.descriptor().field<std::uint32_t>(0x24).set(1);model.descriptor().field<std::uint16_t>(2).set(0);p.flags_02().set(0);
        model.bytes().field<std::uint16_t>(p.location().offset+0x24).set(1);
        for(unsigned j=0;j<15;++j){model.corner_indices().at(j).set(static_cast<std::uint16_t>(j));auto c=p.coordinates().at(j).value();c[0]=std::bit_cast<std::uint32_t>(float(j)+0.25f);c[2]=0;p.coordinates().at(j).set(c);}
        auto a=p.attributes().at(0).value();a[6]=a[7]=0;
        for(unsigned n=3;n<=15;++n)for(unsigned transparent=0;transparent<2;++transparent){a[0]=static_cast<std::uint8_t>(n);a[3]=static_cast<std::uint8_t>(transparent?0x40:0);p.attributes().at(0).set(a);const auto out=adapter.capture(0);
            check(out.model.vertices.size()==3*(n-2)&&out.model.surfaces.size()==1,"single polygon table receiving extent");
            for(unsigned triangle=0;triangle<n-2;++triangle){std::array<unsigned,3> expected{0,triangle+1,triangle+2};
                if(transparent){const std::array ordinals{triangle+(triangle&1),triangle+1-(triangle&1),triangle+2};for(unsigned j=0;j<3;++j)std::memcpy(&expected[j],table.data()+n*64+ordinals[j]*4,4);}
                for(unsigned j=0;j<3;++j)check(out.model.vertices[triangle*3+j].u==float(expected[j])+0.25f,"PE table corner winding differs");}}
    }else if(name=="texture_prefix"){
        const auto track=f.model(f.load("track1"));const auto names=track.descriptor().field<std::uint32_t>(0x60).value();const auto name_at=track.bytes().field<std::uint32_t>(names).value();const auto original=track.texture_name(0);
        const SourcePmdGeometryAdapter prefixed(track,"TRACKSET02"),empty(track,"");
        const std::array<std::pair<std::string,std::string>,5> cases{{{"Trk_x","TRACKSET02\\Trk_x"},{"x\\Trk_y","TRACKSET02\\Trk_y"},{"Trk_\\plain","TRACKSET02\\plain"},{"\\Trk_z","TRACKSET02\\\\Trk_z"},{"trk_x","trk_x"}}};
        for(const auto& [input,expected]:cases){check(input.size()<=original.size(),"prefix mutation must stay within original string cell");
            for(std::uint32_t j=0;j<=input.size();++j)track.bytes().field<std::uint8_t>(name_at+j).set(j==input.size()?0:static_cast<std::uint8_t>(input[j]));
            check(prefixed.capture(0).model.textures[0]==expected&&empty.capture(0).model.textures[0]==input,"source42BB70 full-input/case/basename branch");}
        for(std::uint32_t j=0;j<=original.size();++j)track.bytes().field<std::uint8_t>(name_at+j).set(j==original.size()?0:static_cast<std::uint8_t>(original[j]));
    }else if(name=="shared_skin"){
        const auto other=f.model(f.load());const auto other_before=encode(SourcePmdGeometryAdapter(other,"").capture(0));std::array<LiveGeometry,4> before;
        for(unsigned slot=0;slot<4;++slot)before[slot]=adapter.capture(slot);
        std::array<LevelCarSlot,6> cars{};std::array<std::int8_t,6> random{};cars[0].source_word00=cars[1].source_word00=id;
        cars[0].settings.livery=3;cars[1].settings.livery=7;cars[0].settings.tyre=2;cars[1].settings.tyre=4;remap_level_vehicle_textures(f.resources,cars,random);
        unsigned checked=0;for(unsigned slot=0;slot<4;++slot){const auto after=adapter.capture(slot);check(after.model.textures!=before[slot].model.textures,"shared skin did not reach every LOD");
            for(const auto& text:after.model.textures){if(text.find("grid")!=text.npos){check(text.ends_with("grid7"),"shared last-slot livery");++checked;}if(text.find("tyre")!=text.npos){check(text.ends_with("tyre4"),"shared last-slot tyre");++checked;}}
            before[slot].model.textures=after.model.textures;check(encode(before[slot])==encode(after),"skin remap changed geometry or polygon-local texture indices");}
        check(checked==12&&other_before==encode(SourcePmdGeometryAdapter(other,"").capture(0)),"skin namespace instance isolation");
    }else if(name=="pointer_and_lod"){
        const auto before=encode(adapter.capture(0));const auto d=model.descriptor();const auto slot=d.field<std::uint32_t>(0x48);const auto old=slot.value();slot.set(d.field<std::uint32_t>(0x4c).value());
        check(encode(adapter.capture(0))==encode(adapter.capture(1)),"current source LOD pointer ignored");slot.set(old);
        const auto p=model.part(0,0);const auto pointer=model.bytes().field<std::uint32_t>(p.location().offset+0x2c);const auto offset=pointer.value();pointer.set(offset+1);format_rejects([&]{(void)adapter.capture(0);});pointer.set(offset);
        check(encode(adapter.capture(0))==before,"pointer restore");format_rejects([&]{(void)adapter.capture(4);});
    }else if(name=="bad_geometry"){
        const auto p=model.part(0,0);const auto h=p.attributes().at(0);const auto old=h.value();auto a=old;a[0]=2;h.set(a);format_rejects([&]{(void)adapter.capture(0);});h.set(old);
        a=old;a[1]=255;h.set(a);check(model.texture_count()<255,"invalid texture fixture domain");format_rejects([&]{(void)adapter.capture(0);});h.set(old);
        const auto first=std::uint32_t(old[6])|(std::uint32_t(old[7])<<8);const auto index=model.corner_indices().at(first);const auto ci=index.value();index.set(65535);format_rejects([&]{(void)adapter.capture(0);});index.set(ci);
        const auto coordinate=p.coordinates().at(ci);const auto c=coordinate.value();auto nc=c;nc[0]=0x7fc00000;coordinate.set(nc);format_rejects([&]{(void)adapter.capture(0);});coordinate.set(c);
        const auto v=p.vertices().at(c[2]&0xffff);auto pos=v.value();const auto old_pos=pos;pos.z=std::numeric_limits<float>::infinity();v.set(pos);format_rejects([&]{(void)adapter.capture(0);});v.set(old_pos);
    }else if(name=="free_and_reuse"||name=="scope"){
        const auto capture=adapter.capture(0);const auto retained=encode(capture);const auto old=model.allocation();if(name=="scope")f.bank.release_scope_42ea00();else (void)f.bank.release_42e980(old);
        allocation_rejects([&]{(void)adapter.capture(0);});allocation_rejects([&]{(void)adapter.capture_views(0,CameraState{},CameraModelTransform{});});allocation_rejects([&]{(void)adapter.source_lod_index(0);});allocation_rejects([&]{(void)adapter.allocation();});
        if(name=="scope")f.bank.begin_42ea80();const auto fresh=f.model(f.load());check(!(fresh.allocation()==old),"bank generation reused");allocation_rejects([&]{(void)adapter.capture(0);});check(encode(capture)==retained,"owned GPU transport invalidated by source free");
    }else if(name=="metadata_lifetime"){
        auto owner=std::make_unique<ModelResources>(f.assets,root);const auto token=owner->load_source(f.bank,f.files,"car1");const auto view=owner->source_at(*owner->resolve(token));const SourcePmdGeometryAdapter expired(view,"");const auto allocation=view.allocation();owner.reset();
        check(!f.bank.bytes(allocation).empty(),"model metadata destructor unexpectedly source-freed payload");allocation_rejects([&]{(void)expired.capture(0);});
    }else if(name=="foreign_thread"){
        const auto before=snapshot(f.bank,model.allocation());bool rejected=false;std::exception_ptr failure;
        std::thread thread([&]{try{allocation_rejects([&]{(void)adapter.capture(0);});allocation_rejects([&]{(void)adapter.capture_views(0,CameraState{},CameraModelTransform{});});rejected=true;}catch(...){failure=std::current_exception();}});thread.join();
        if(failure)std::rethrow_exception(failure);check(rejected&&before==snapshot(f.bank,model.allocation()),"foreign thread mutated or accessed source payload");
    }else if(name=="batching"){
        for(const auto& model_name:{"car1","track1"}){const auto source=f.model(f.load(model_name));const SourcePmdGeometryAdapter mesh(source,std::string_view(model_name)=="track1"?"TRACKSET02":"");
            for(unsigned slot=0;slot<4;++slot){auto out=mesh.capture(slot);const auto original=out;coalesce_live_draws(out);check(out.model.vertices.size()==original.model.vertices.size()&&std::memcmp(out.model.vertices.data(),original.model.vertices.data(),out.model.vertices.size()*sizeof(Vertex))==0,"batching changed vertex order");
                std::size_t at=0;for(std::size_t i=0;i<out.model.surfaces.size();++i){const auto& group=out.model.surfaces[i];std::uint64_t n=0;check(at<original.model.surfaces.size()&&group.first==original.model.surfaces[at].first,"batch initial range");
                    while(n<group.count){check(at<original.model.surfaces.size(),"batch range overrun");const auto& s=original.model.surfaces[at];check(s.first==group.first+n&&s.texture==group.texture&&s.part==group.part&&s.translucent==group.translucent&&s.two_sided==group.two_sided&&original.materials[at].blend==out.materials[i].blend,"batch changed ordered draw state");n+=s.count;++at;}check(n==group.count,"batch triangle range");}
                check(at==original.model.surfaces.size(),"batch lost source polygons");}}
    }else throw std::runtime_error("unknown geometry receiving case");
}
}
int main(int argc,char** argv){try{check(argc==4,"expected case, actual retail root, fixtures");execute(argv[1],argv[2],argv[3]);std::cout<<"{\"case\":\""<<argv[1]<<"\",\"errors\":0}\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
