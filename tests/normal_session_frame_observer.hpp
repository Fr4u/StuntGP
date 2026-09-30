#pragma once
// Independent native frame observations. No encoded-image or guest-address
// helpers are used here. Pointer values always preserve the full host width.
#include "normal_session_native_observer.hpp"

namespace normal_native_observer {
template<class T,std::size_t N>void frame_view(Observations& out,std::string name,std::span<T,N> view){
    out.value(name+"/address",reinterpret_cast<std::uint64_t>(view.data()));
    out.value(name+"/count",static_cast<std::uint64_t>(view.size()));
    out.value(name+"/native-stride",static_cast<std::uint32_t>(sizeof(T)));
}
inline void frame_packet_pointer(Observations& out,const std::string& key,const WorldPolygonPacketPointer& pointer){
    out.value(key+"-null",pointer.is_null()?1u:0u);
    out.value(key+"-offset",static_cast<std::uint64_t>(pointer.offset().value_or(0)));
    if(!pointer.is_null())frame_view(out,key+"-view",pointer.storage());
}
inline void capture_frame(RaceSession& session,const std::filesystem::path& output){
    auto& s=session.state();auto& f=s.polygon_frame;
    if(!f.allocations)throw std::runtime_error("native frame observation has no allocation owner");
    auto& a=*f.allocations;Observations out;
    out.span("vertices",std::span(a.vertices));out.span("clip/attributes",std::span(a.clip_attributes));
    out.span("clip/headers",std::span(a.clip_headers));out.span("clip/unused",std::span(a.clip_unused_indices));
    out.span("indexed/pointers",std::span(a.indexed));out.span("clip/sequential",std::span(f.clip.sequential_indices));
    out.span("light",std::span(s.light_vertices));out.ref("light-offset",s.light_vertex_offset);
    frame_view(out,"transformed-view",f.transformed.allocation);out.ref("transformed-count",f.transformed.count);
    if(!f.indexed.base().is_null())frame_view(out,"indexed-base-view",f.indexed.base().storage());
    if(!f.indexed.current().is_null())frame_view(out,"indexed-current-view",f.indexed.current().storage());
    out.value("indexed-base-null",f.indexed.base().is_null()?1u:0u);
    out.value("indexed-current-null",f.indexed.current().is_null()?1u:0u);
    out.value("indexed-base-offset",static_cast<std::uint64_t>(f.indexed.base().offset().value_or(0)));
    out.value("indexed-current-offset",static_cast<std::uint64_t>(f.indexed.current().offset().value_or(0)));
    frame_view(out,"clip/attribute-view",f.clip.attributes);frame_view(out,"clip/header-view",f.clip.headers);
    frame_view(out,"clip/unused-view",f.clip.unused_indices);frame_view(out,"clip/vertex-view",f.clip.vertices);
    out.ref("clip/attribute-count",f.clip.attribute_count);out.ref("clip/header-count",f.clip.header_count);
    out.ref("clip/unused-count",f.clip.unused_index_count);out.ref("clip/vertex-count",f.clip.vertex_count);
    for(unsigned p=0;p<4;++p){const auto prefix="packets/"+std::to_string(p);frame_view(out,prefix+"/owner",std::span(a.packets[p]));
        frame_packet_pointer(out,prefix+"/base",f.packets[p].base());frame_packet_pointer(out,prefix+"/current",f.packets[p].current());
        out.ref(prefix+"/total",f.totals[p]);
        std::vector<std::array<std::uint64_t,8>> references;references.reserve(a.packets[p].size());
        for(const auto& record:a.packets[p]){const auto attributes=record.attributes.bytes();references.push_back({
            reinterpret_cast<std::uint64_t>(record.header.identity()),reinterpret_cast<std::uint64_t>(record.indices.data()),static_cast<std::uint64_t>(record.indices.size()),
            reinterpret_cast<std::uint64_t>(attributes.data()),static_cast<std::uint64_t>(attributes.size()),
            reinterpret_cast<std::uint64_t>(record.vertices.data()),static_cast<std::uint64_t>(record.vertices.size()),record.inside_mask});}
        out.values(prefix+"/references",std::span(references));
        for(unsigned group=0;group<4;++group){const auto key="groups/"+std::to_string(group)+"/"+std::to_string(p);const auto& range=f.groups[group][p];
            frame_packet_pointer(out,key+"/first",range.first);out.ref(key+"/count",range.count);}
    }
    for(unsigned p=0;p<2;++p){const auto prefix="spatial/"+std::to_string(p);frame_view(out,prefix+"/owner",std::span(a.spatial[p]));frame_view(out,prefix+"/view",f.spatial[p].records);out.ref(prefix+"/count",f.spatial[p].count);
        std::vector<std::array<std::uint64_t,4>> references;references.reserve(a.spatial[p].size());
        for(const auto& record:a.spatial[p])references.push_back({reinterpret_cast<std::uint64_t>(record.polygons.data()),static_cast<std::uint64_t>(record.polygons.size()),reinterpret_cast<std::uint64_t>(record.vertices.data()),static_cast<std::uint64_t>(record.vertices.size())});
        out.values(prefix+"/references",std::span(references));
    }
    out.ref("model-view/local-camera",f.model_view.local_camera);out.ref("model-view/normalized-camera",f.model_view.normalized_camera);
    out.ref("model-view/matrix",f.model_view.model_view);out.ref("bound-matrix",f.bound_matrix);
    out.ref("frame-stamp",f.frame_stamp);out.ref("remaining-first",f.remaining_first);out.ref("spatial-batch-count",f.spatial_batch_count);
    out.ref("lighting-stride",f.lighting_stride);out.ref("low-quality-initialized",f.low_quality_initialized);out.ref("accepted-models",f.accepted_models);
    out.value("active-track-address",reinterpret_cast<std::uint64_t>(f.active_track.native_model()));
    const auto base_part=f.active_track_base_part.native_part();
    out.value("active-track-base-owner",reinterpret_cast<std::uint64_t>(base_part.owner));
    out.value("active-track-base-lod",base_part.lod);
    out.value("active-track-base-part",base_part.part);
    for(std::size_t i=0;i<session.models().size();++i)out.value("model-address/"+std::to_string(i),reinterpret_cast<std::uint64_t>(&session.models().at(i)));
    out.save(output);
}
}
