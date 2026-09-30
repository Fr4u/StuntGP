#pragma once
// Private code-only continuation of the accepted model-owner codec.
// Pending full normal-parent qualification; never used by the game runtime.
#include "normal_session_model_export.hpp"

namespace normal_session_owner {
struct FrameGuestLayout {
    static constexpr std::uint32_t vertices=0x72000000,attributes=0x72080000,headers=0x72090000;
    static constexpr std::uint32_t unused=0x720a0000,indexed=0x720b0000;
    static constexpr std::array<std::uint32_t,4> packets{0x720c0000,0x720d0000,0x720e0000,0x720f0000};
    static constexpr std::array<std::uint32_t,2> spatial{0x72100000,0x72110000};
    static constexpr std::uint32_t light=0x72120000;
};
class EncodedWords {
    std::vector<std::uint8_t> bytes_;
    std::vector<Claim> claims_;
public:
    void append(std::uint32_t value,Origin origin,std::string name){
        const auto at=narrow(bytes_.size());bytes_.resize(bytes_.size()+4);std::memcpy(bytes_.data()+at,&value,4);
        claims_.push_back({at,4,origin,std::move(name)});
    }
    std::uint32_t save(Image& image,std::string name,std::uint32_t address)&&{
        return image.create_encoded(std::move(name),address,bytes_,std::move(claims_));
    }
};
inline void frame_pointer(Image& out,std::string name,std::uint32_t source,std::uint32_t target){
    EncodedWords word;word.append(target,Origin::relocation,name);
    (void)std::move(word).save(out,name,source);
    if(target)out.alias(name+"-target",target,0,true);
}
inline void frame_scalar(Image& out,std::string name,std::uint32_t source,std::uint32_t value){
    out.create_live(std::move(name),source,std::span(&value,1));
}
template<class T,std::size_t N>std::uint32_t frame_view_address(const NativeAddresses& addresses,std::span<T,N> view){
    return addresses.address(view.data(),view.size_bytes(),view.empty());
}
inline std::uint32_t frame_target(Image& out,std::string name,std::uint32_t address,std::size_t extent){
    if(!address){require(extent==0,"null frame pointer has nonzero source extent");return 0;}
    const auto alias=out.reference(name,address,narrow(extent),extent==0);
    if(alias.one_past)out.alias(std::move(name),address,0,true);
    return address;
}
inline std::uint32_t frame_packet_address(const WorldPolygonPacketPointer& pointer,const WorldPolygonPacketStorageLifetime& owner,std::uint32_t guest){
    if(pointer.is_null())return 0;
    pointer.require_storage(owner);
    return add(guest,*pointer.offset()*20);
}
// Source 42E430 allocates the complete pools. Cursor reset42E800/427240,
// group42B100/140/180 and totals42B200 retain unused records, so export all of
// them, including pointers in records beyond the current append position.
inline void export_frame_owners(RaceSessionState& s,Image& out,NativeAddresses& addresses,const ModelsExport& models){
    using L=FrameGuestLayout;auto& f=s.polygon_frame;
    require(bool(f.allocations),"normal frame has no actual allocation owner");auto& a=*f.allocations;
    // The actual native owner is registered once. Clip vertices are an interior
    // view of this same allocation, not a copied second allocation.
    addresses.array("frame-vertices",std::span(a.vertices),L::vertices);
    addresses.array("frame-clip-attributes",std::span(a.clip_attributes),L::attributes);
    addresses.array("frame-clip-headers",std::span(a.clip_headers),L::headers);
    addresses.array("frame-clip-unused",std::span(a.clip_unused_indices),L::unused);
    addresses.array("frame-indexed",std::span(a.indexed),L::indexed,4);
    addresses.array("frame-sequential",std::span(f.clip.sequential_indices),0x6244a8);
    for(unsigned p=0;p<4;++p)addresses.array("frame-packets-"+std::to_string(p),std::span(a.packets[p]),L::packets[p],20);
    for(unsigned p=0;p<2;++p)addresses.array("frame-spatial-"+std::to_string(p),std::span(a.spatial[p]),L::spatial[p],8);
    require(s.light_vertices.size()==0x4800,"normal light buffer differs from full42E430 allocation");
    addresses.array("frame-light",std::span(s.light_vertices),L::light);
    out.create_live("frame-vertices",L::vertices,std::span(a.vertices));
    out.create_live("frame-clip-attributes",L::attributes,std::span(a.clip_attributes));
    out.create_live("frame-clip-headers",L::headers,std::span(a.clip_headers));
    out.create_live("frame-clip-unused",L::unused,std::span(a.clip_unused_indices));
    out.create_live("frame-sequential",0x6244a8,std::span(f.clip.sequential_indices));
    out.create_live("frame-light",L::light,std::span(s.light_vertices));
    const auto clip_vertices=L::vertices+WorldPolygonFrameAllocations::clip_vertex_first*32;
    addresses.check_view(f.transformed.allocation,L::vertices);if(!f.indexed.base().is_null()){f.indexed.base().require_storage(a.indexed_lifetime);addresses.check_view(f.indexed.base().storage(),L::indexed);}
    if(!f.indexed.current().is_null())f.indexed.current().require_storage(a.indexed_lifetime);
    addresses.check_view(f.clip.attributes,L::attributes);addresses.check_view(f.clip.headers,L::headers);
    addresses.check_view(f.clip.unused_indices,L::unused);addresses.check_view(f.clip.vertices,clip_vertices);
    require(f.transformed.allocation.size()==a.vertices.size(),"frame cursor view is not the full actual owner");
    require(f.clip.attributes.size()==a.clip_attributes.size()&&f.clip.headers.size()==a.clip_headers.size()&&f.clip.unused_indices.size()==a.clip_unused_indices.size()&&f.clip.vertices.size()==3072,"clip view extent differs");
    out.alias("frame-clip-vertex-view",clip_vertices,narrow(f.clip.vertices.size_bytes()));
    EncodedWords indexed;
    for(unsigned i=0;i<a.indexed.size();++i){const auto* vertex=a.indexed[i];indexed.append(addresses.address(vertex,vertex?32:0),Origin::relocation,"indexed-"+std::to_string(i));}
    (void)std::move(indexed).save(out,"frame-indexed",L::indexed);
    for(unsigned p=0;p<4;++p){
        const auto name="frame-packets-"+std::to_string(p);EncodedWords packets;
        // Actual owner registration is independent of NULL/retained source pointers.
        // Counts are raw source DWORDs: interrupted group clearing and signed
        // differences can make them unsuitable for rendering without losing data.
        for(unsigned i=0;i<a.packets[p].size();++i){const auto& record=a.packets[p][i];const auto prefix=name+"-"+std::to_string(i);const auto attributes=record.attributes.bytes();
            packets.append(frame_target(out,prefix+"-header-target",addresses.address(record.header.identity(),record.header?8:0),record.header?8:0),Origin::relocation,prefix+"-header");
            packets.append(frame_target(out,prefix+"-indices-target",frame_view_address(addresses,record.indices),record.indices.size_bytes()),Origin::relocation,prefix+"-indices");
            packets.append(frame_target(out,prefix+"-attributes-target",frame_view_address(addresses,attributes),attributes.size_bytes()),Origin::relocation,prefix+"-attributes");
            packets.append(frame_target(out,prefix+"-vertices-target",frame_view_address(addresses,record.vertices),record.vertices.size_bytes()),Origin::relocation,prefix+"-vertices");
            packets.append(record.inside_mask,Origin::live,prefix+"-inside-mask");
        }
        (void)std::move(packets).save(out,name,L::packets[p]);
    }
    for(unsigned p=0;p<2;++p){
        const auto name="frame-spatial-"+std::to_string(p);EncodedWords batches;
        addresses.check_view(f.spatial[p].records,L::spatial[p]);require(f.spatial[p].records.size()==a.spatial[p].size()&&f.spatial[p].count<=a.spatial[p].size(),"spatial view/cursor differs");
        for(unsigned i=0;i<a.spatial[p].size();++i){const auto& record=a.spatial[p][i];const auto prefix=name+"-"+std::to_string(i);
            batches.append(frame_target(out,prefix+"-polygons-target",frame_view_address(addresses,record.polygons),record.polygons.size_bytes()),Origin::relocation,prefix+"-polygons");
            batches.append(frame_target(out,prefix+"-vertices-target",frame_view_address(addresses,record.vertices),record.vertices.size_bytes()),Origin::relocation,prefix+"-vertices");
        }
        (void)std::move(batches).save(out,name,L::spatial[p]);
    }
    for(const auto [source,target]:std::array<std::pair<unsigned,unsigned>,12>{{
        {0x61c50c,L::vertices},{0x6244c8,L::attributes},{0x6244cc,L::headers},{0x6244d0,L::unused},
        {0x6244d4,clip_vertices},{0x61c5ec,f.indexed.base().is_null()?0u:add(L::indexed,*f.indexed.base().offset()*4)},
        {0x61c5fc,frame_packet_address(f.packets[0].base(),a.packet_lifetimes[0],L::packets[0])},{0x61c600,frame_packet_address(f.packets[1].base(),a.packet_lifetimes[1],L::packets[1])},
        {0x61c5f4,frame_packet_address(f.packets[2].base(),a.packet_lifetimes[2],L::packets[2])},{0x61c5f8,frame_packet_address(f.packets[3].base(),a.packet_lifetimes[3],L::packets[3])},{0x61c518,L::spatial[0]},{0x61c51c,L::spatial[1]}}})
        frame_pointer(out,"frame-root-"+std::to_string(source),source,target);
    frame_pointer(out,"frame-light-root",0x624530,L::light);
    require(f.transformed.count<=a.vertices.size()&&f.remaining_first<=a.vertices.size(),"frame vertex/index cursor exceeds owner");
    frame_pointer(out,"frame-transformed-cursor",0x61c510,add(L::vertices,std::size_t(f.transformed.count)*32));
    frame_pointer(out,"frame-remaining-first",0x61c514,add(L::vertices,std::size_t(f.remaining_first)*32));
    frame_pointer(out,"frame-indexed-cursor",0x61c520,f.indexed.current().is_null()?0u:add(L::indexed,*f.indexed.current().offset()*4));
    require(s.light_vertex_offset<=s.light_vertices.size(),"light cursor exceeds owner");frame_pointer(out,"frame-light-cursor",0x624574,add(L::light,s.light_vertex_offset));
    for(unsigned p=0;p<2;++p)frame_pointer(out,"frame-spatial-cursor-"+std::to_string(p),0x61c524+p*4,add(L::spatial[p],std::size_t(f.spatial[p].count)*8));
    for(unsigned p=0;p<4;++p){
        frame_pointer(out,"frame-packet-cursor-"+std::to_string(p),0x6243c0+p*8,frame_packet_address(f.packets[p].current(),a.packet_lifetimes[p],L::packets[p]));
        frame_scalar(out,"frame-packet-total-"+std::to_string(p),0x6243c4+p*8,f.totals[p]);
        for(unsigned group=0;group<4;++group){const auto& range=f.groups[group][p];const auto prefix="frame-group-"+std::to_string(group)+"-"+std::to_string(p);
            const auto pointer=frame_packet_address(range.first,a.packet_lifetimes[p],L::packets[p]);
            frame_pointer(out,prefix+"-first",0x6243e0+group*32+p*8,pointer);
            frame_scalar(out,prefix+"-count",0x6243e4+group*32+p*8,range.count);
        }
    }
    const std::array<unsigned,4> bases{L::attributes,L::headers,L::unused,clip_vertices};
    const std::array<unsigned,4> counts{f.clip.attribute_count,f.clip.header_count,f.clip.unused_index_count,f.clip.vertex_count};
    const std::array<unsigned,4> strides{16,8,2,32};const std::array<std::size_t,4> capacities{f.clip.attributes.size(),f.clip.headers.size(),f.clip.unused_indices.size(),f.clip.vertices.size()};
    for(unsigned p=0;p<4;++p){require(counts[p]<=capacities[p],"clip cursor exceeds actual owner");frame_pointer(out,"frame-clip-cursor-"+std::to_string(p),0x6244d8+p*4,add(bases[p],std::size_t(counts[p])*strides[p]));}
    frame_pointer(out,"frame-active-track",0x61c52c,models.descriptor_for(f.active_track.native_model()));
    frame_pointer(out,"frame-active-track-base-part",0x61c530,models.track_part_for(f.active_track_base_part));
    frame_scalar(out,"frame-stamp",0x61c508,f.frame_stamp);frame_scalar(out,"frame-spatial-batch-count",0x61c550,f.spatial_batch_count);
    frame_scalar(out,"frame-lighting-stride",0x62497c,f.lighting_stride);frame_scalar(out,"frame-low-quality-initialized",0x57b41c,f.low_quality_initialized);frame_scalar(out,"frame-accepted-models",0x6249ac,f.accepted_models);
    out.create_live("frame-local-camera",0x61c4e8,std::span(&f.model_view.local_camera,1));
    out.create_live("frame-normalized-camera",0x61c4f8,std::span(&f.model_view.normalized_camera,1));
    out.create_live("frame-model-view",0x61c428,std::span(&f.model_view.model_view,1));
    out.create_live("frame-bound-matrix",0x577a90,std::span(&f.bound_matrix,1));
}
}
