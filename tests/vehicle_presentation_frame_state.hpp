#pragma once
#include "vehicle_presentation_source_image.hpp"
#include "vehicle_tick_vehicle_state.hpp"
#include "sgn/retail_time_trial.hpp"

namespace sgn::retail::presentation_test {
struct State {
    VehicleStorage vehicles;
    ModelResources resources;
    std::vector<ModelImage> models;
    std::array<std::uint32_t,6> lod{};
    std::uint32_t current{},quality{},camera_vehicle{},split{},screen_count{};
    CameraState camera;
    CameraLifecycle lifecycle;
    CameraPresentationState presentation;
    ControllerOverlayTransition overlay;
    VehicleStripGeometry strip;
    WorldModelDrawState draws;
    ViewMatrixArena matrices;
    VehicleMatrix basis{};
    TrailProfileBank profiles=initial_trail_profile_bank();
    SourceTrailPolygon staging;
    SourceSpritePacketStorage sprites;
    std::array<SourceScreenPolygon,256> screen;
    ViewportState viewport;
    State(AssetStore& assets,const std::filesystem::path& root,SourceImage& image):resources(assets,root){
        models.reserve(12);
        for(unsigned i=0;i<12;++i){const auto name="car"+std::to_string(i/2+1)+(i%2?"sh":"");
            const auto token=resources.load(name);auto& model=resources.at(*resources.resolve(token));
            models.emplace_back(image.region(1,i).address,read_file(root/"meshdata"/(name+".PMD")),model,token);
        }
        for(unsigned i=0;i<6;++i)models[i*2].model->bind_current_detail_level(lod[i]);
    }
    VehiclePresentationFrame bindings(){return {vehicles,resources,lod,current,quality,camera_vehicle,split,
        {camera,lifecycle,presentation,overlay},strip,draws,matrices,basis,profiles,staging,sprites,{MutableScreenPolygonBaseReader::fixed_fixture(screen),screen_count},viewport};}
    void transfer(SourceImage& image,bool input){
        const auto strip_descriptor=image.region(2,5).address,sprite_descriptor=image.region(3,5).address;
        const auto strip_part=image.region(2,4).address,sprite_part=image.region(3,4).address;
        for(auto& model:models)model.transfer(image,input);
        for(unsigned i=0;i<6;++i){
            std::array<std::uint8_t,0x7800> raw{};const auto address=vehicle_base+i*vehicle_stride;
            auto source=image.at(address,vehicle_stride);std::memcpy(raw.data(),source.data(),source.size());
            auto get=[&](unsigned at){std::uint32_t value;std::memcpy(&value,raw.data()+at,4);return value;};
            auto put=[&](unsigned at,std::uint32_t value){std::memcpy(raw.data()+at,&value,4);};
            for(unsigned at:{0x26b0u,0x26b4u})put(at,model_token(models,get(at),strip_descriptor,sprite_descriptor));
            // This corpus was captured from initialization, with no replay or
            // previous skid-corner handles. Reject a wider domain explicitly.
            require(get(0x7268)==0,"presentation fixture replay domain");
            for(unsigned k=0;k<4;++k)require(get(0x3970+k*0xd10)==0,"presentation fixture skid handle domain");
            tick_test::transfer_vehicle(raw,vehicles,i,input,resources);
            if(!input){for(unsigned at:{0x26b0u,0x26b4u})put(at,model_address(models,get(at),strip_descriptor,sprite_descriptor));
                std::memcpy(source.data(),raw.data(),source.size());}
        }
        image.field(quality,0x61297c,input);image.field(split,0x576be0,input);
        if(input){const auto pointer=image.get(0x4d5bb8);
            require(pointer>=vehicle_base&&pointer<vehicle_base+6*vehicle_stride&&(pointer-vehicle_base)%vehicle_stride==0,"presentation current source slot");
            current=(pointer-vehicle_base)/vehicle_stride;
        }else {require(current<6,"presentation current native slot");image.put(0x4d5bb8,vehicle_base+current*vehicle_stride);}
        image.field(basis,0x4d5b30,input);image.field(staging,0x571330,input);
        transfer_pose(image,draws.template_pose,0x4864e8,false,input);
        transfer_pose(image,strip.part_transform,0x474820,true,input);
        transfer_pose(image,draws.sprite_part_transform,0x4788c0,true,input);
        transfer_queue(image,draws.regular,0x62f5d8,0x4955e0,models,strip_descriptor,sprite_descriptor,input);
        transfer_queue(image,draws.background,0x632cf8,0x4955e4,models,strip_descriptor,sprite_descriptor,input);
        image.field(matrices.matrices,matrix_base,input);image.field(matrices.count,0x6227d0,input);
        if(input){const auto cursor=image.get(0x6227cc);
            require(cursor>=matrix_base&&cursor<=matrix_base+384*64&&(cursor-matrix_base)%64==0,"presentation matrix cursor");matrices.cursor=cursor-matrix_base;
        }else {require(matrices.cursor&&*matrices.cursor<=384*64,"presentation native matrix cursor");image.put(0x6227cc,matrix_base+*matrices.cursor);}
        image.field(strip.polygons,image.region(2,0).address,input);
        image.field(strip.corner_indices,image.region(2,1).address,input);
        image.field(strip.vertices,image.region(2,2).address,input);
        image.field(strip.corners,image.region(2,3).address,input);
        image.field(strip.normals,0x570ad8,input);image.field(strip.texture_names,0x5712e0,input);image.field(strip.texture_id,0x5712fc,input);
        image.field(strip.field_5712f0,0x5712f0,input);image.field(strip.field_5712f4,0x5712f4,input);image.field(strip.field_5712f8,0x5712f8,input);
        image.field(strip.part_flags,strip_part,input);image.field(strip.part_flags_02,strip_part+2,input);image.field(strip.model_flags_02,strip_descriptor+2,input);
        image.field(strip.model_field20,strip_descriptor+0x20,input);image.field(strip.model_field22,strip_descriptor+0x22,input);
        image.field(strip.part_field20,strip_part+0x20,input);image.field(strip.part_field22,strip_part+0x22,input);
        image.field(strip.polygon_count,strip_part+0x24,input);image.field(strip.corner_count,strip_part+0x26,input);image.field(strip.vertex_count,strip_part+0x28,input);
        auto outer=[&](const ModelTransform*& value,std::uint32_t descriptor){
            if(input){const auto pointer=image.get(descriptor+12);require(pointer==0||pointer==0x4864e8,"presentation generated outer reference");value=pointer?&draws.template_pose:nullptr;}
            else {require(!value||value==&draws.template_pose,"presentation generated native outer reference");image.put(descriptor+12,value?0x4864e8:0);}
        };
        outer(strip.outer_transform,strip_descriptor);outer(draws.sprite_outer_transform,sprite_descriptor);
        image.field(sprites.headers,image.region(3,0).address,input);image.field(sprites.indices,image.region(3,1).address,input);
        image.field(sprites.positions,image.region(3,2).address,input);image.field(sprites.attributes,image.region(3,3).address,input);
        auto sprite_cursor=[&](std::uint32_t& value,unsigned index,std::uint32_t cursor,unsigned stride){
            const auto& region=image.region(3,index);
            if(input){const auto pointer=image.get(cursor);require(pointer>=region.address&&pointer<=region.address+region.length&&(pointer-region.address)%stride==0,"presentation sprite cursor");value=(pointer-region.address)/stride;}
            else {require(value<=region.length/stride,"presentation native sprite cursor");image.put(cursor,region.address+value*stride);}
        };
        sprite_cursor(sprites.header_bytes,0,0x5da24c,1);sprite_cursor(sprites.index_count,1,0x5da250,2);
        sprite_cursor(sprites.position_count,2,0x5da254,16);sprite_cursor(sprites.attribute_count,3,0x5da258,16);
        image.field(sprites.polygons,sprite_part+0x24,input);image.field(sprites.vertices,sprite_part+0x26,input);image.field(sprites.corners,sprite_part+0x28,input);
        image.field(sprites.right,0x5da1e0,input);image.field(sprites.up,0x5da1f0,input);image.field(sprites.forward,0x5da200,input);
        image.field(draws.sprite_part_flags,sprite_part,input);image.field(draws.sprite_part_flags_02,sprite_part+2,input);
        image.field(draws.sprite_model_flags,sprite_descriptor+2,input);image.field(draws.previous_sprite_polygons,0x6249b0,input);
        image.field(screen,image.region(4).address,input);image.field(screen_count,0x61c604,input);
        const auto camera_address=image.get(0x4722d0);
        require(camera_address>=0x498248&&camera_address<0x498248+4*0x74c&&(camera_address-0x498248)%0x74c==0,"presentation camera selector");
        const auto target=image.get(camera_address+0xac);
        require(target>=vehicle_base&&target<vehicle_base+6*vehicle_stride&&(target-vehicle_base)%vehicle_stride==0,"presentation camera vehicle");
        if(input)camera_vehicle=(target-vehicle_base)/vehicle_stride;else require(camera_vehicle==(target-vehicle_base)/vehicle_stride,"presentation camera target changed");
        image.field(camera.mode,camera_address+4,input);image.field(camera.position,camera_address+8,input);
        image.field(camera.lens,camera_address+0xb0,input);image.field(camera.near_clip,camera_address+0xb4,input);
        image.field(camera.far_factor,camera_address+0xb8,input);image.field(presentation.forward_axis,camera_address+0x9c,input);
        image.field(presentation.planes,camera_address+0x118,input);image.field(lifecycle.clip_near,camera_address+0x198,input);
        image.field(lifecycle.clip_far,camera_address+0x19c,input);image.field(overlay.active,camera_address+0x1e8,input);
        image.field(camera.camera_to_world_rotation,image.get(camera_address+0x50),input);
        image.field(viewport.clip_screen,0x576c14,input);image.field(viewport.clip_expanded,0x576c24,input);
    }
};
}
