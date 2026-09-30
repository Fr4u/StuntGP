// CPU-only SDK-shaped transport; no XAudio2 engine, device or COM creation.
#include <windows.h>
#include <xaudio2.h>
#include "sgn/audio_output_matrix.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
// Explicit per-destination L/R records establish layout without an S/D index formula.
struct StereoDestination {float from_left,from_right;};
static_assert(std::is_standard_layout_v<StereoDestination>&&sizeof(StereoDestination)==2*sizeof(float)&&
    offsetof(StereoDestination,from_right)==sizeof(float));
struct Destinations {StereoDestination front_left,front_right,center,lfe,back_left,back_right;};
static_assert(std::is_standard_layout_v<Destinations>&&sizeof(Destinations)==12*sizeof(float)&&
    offsetof(Destinations,front_right)==2*sizeof(float)&&offsetof(Destinations,back_right)==10*sizeof(float));
struct Native final:IXAudio2Voice {
    Destinations levels;
    unsigned reads{},writes{};
    bool invalid{};
    explicit Native(Destinations value):levels(value){}
    virtual ~Native()=default;
    void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice* destination,UINT32 sources,UINT32 destinations,float* out)override{
        ++reads;
        if(destination||sources!=2||destinations!=6||!out){invalid=true;return;}
        std::memcpy(out,&levels,sizeof(levels));
    }
    HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice* destination,UINT32 sources,UINT32 destinations,const float* in,UINT32 operation)override{
        ++writes;
        if(destination||sources!=2||destinations!=6||!in||operation!=XAUDIO2_COMMIT_NOW){invalid=true;return E_INVALIDARG;}
        std::memcpy(&levels,in,sizeof(levels));return S_OK;
    }
    // Every other SDK method is forbidden for this CPU transport fixture.
#define FORBIDDEN(type,name,args) type STDMETHODCALLTYPE name args override{throw std::runtime_error("unexpected SDK call: " #name);}
    FORBIDDEN(void,GetVoiceDetails,(XAUDIO2_VOICE_DETAILS*))
    FORBIDDEN(HRESULT,SetOutputVoices,(const XAUDIO2_VOICE_SENDS*))
    FORBIDDEN(HRESULT,SetEffectChain,(const XAUDIO2_EFFECT_CHAIN*))
    FORBIDDEN(HRESULT,EnableEffect,(UINT32,UINT32))
    FORBIDDEN(HRESULT,DisableEffect,(UINT32,UINT32))
    FORBIDDEN(void,GetEffectState,(UINT32,BOOL*))
    FORBIDDEN(HRESULT,SetEffectParameters,(UINT32,const void*,UINT32,UINT32))
    FORBIDDEN(HRESULT,GetEffectParameters,(UINT32,void*,UINT32))
    FORBIDDEN(HRESULT,SetFilterParameters,(const XAUDIO2_FILTER_PARAMETERS*,UINT32))
    FORBIDDEN(void,GetFilterParameters,(XAUDIO2_FILTER_PARAMETERS*))
    FORBIDDEN(HRESULT,SetOutputFilterParameters,(IXAudio2Voice*,const XAUDIO2_FILTER_PARAMETERS*,UINT32))
    FORBIDDEN(void,GetOutputFilterParameters,(IXAudio2Voice*,XAUDIO2_FILTER_PARAMETERS*))
    FORBIDDEN(HRESULT,SetVolume,(float,UINT32))
    FORBIDDEN(void,GetVolume,(float*))
    FORBIDDEN(HRESULT,SetChannelVolumes,(UINT32,const float*,UINT32))
    FORBIDDEN(void,GetChannelVolumes,(UINT32,float*))
    FORBIDDEN(void,DestroyVoice,())
#undef FORBIDDEN
};
}
int main(){try{
    const Destinations labeled{{1,2},{3,4},{5,6},{7,8},{9,10},{11,12}};
    Native native(labeled);IXAudio2Voice& voice=native;std::array<float,12> matrix{};
    voice.GetOutputMatrix(nullptr,2,6,matrix.data());
    require(matrix==std::array<float,12>{1,2,3,4,5,6,7,8,9,10,11,12},"SDK labeled destination layout differs");
    require(voice.SetOutputMatrix(nullptr,2,6,matrix.data(),XAUDIO2_COMMIT_NOW)==S_OK,"SDK roundtrip failed");
    require(std::memcmp(&native.levels,&labeled,sizeof(labeled))==0,"Get/Set pan0 roundtrip changed labeled levels");
    require(sgn::audio_host::scale_output_matrix_destination(matrix,2,6,1,0)&&
        sgn::audio_host::scale_output_matrix_destination(matrix,2,6,5,0.1f),"SDK matrix dimensions rejected");
    require(voice.SetOutputMatrix(nullptr,2,6,matrix.data(),XAUDIO2_COMMIT_NOW)==S_OK,"SDK setter failed");
    require(native.levels.front_right.from_left==0&&native.levels.front_right.from_right==0&&
        native.levels.front_left.from_left==1&&native.levels.front_left.from_right==2&&
        native.levels.center.from_left==5&&native.levels.center.from_right==6&&
        native.levels.lfe.from_left==7&&native.levels.lfe.from_right==8&&
        native.levels.back_left.from_left==9&&native.levels.back_left.from_right==10&&
        native.levels.back_right.from_left==1.1f&&native.levels.back_right.from_right==1.2f,
        "typed SDK transport received incorrect labeled source contributions");
    require(native.reads==1&&native.writes==2&&!native.invalid,"unexpected SDK-shaped call");
    std::cout<<"AUDIO_OUTPUT_MATRIX_SDK_FAKE_PASS:typed Win64 Get/Set;explicit destination records;pan0 roundtrip;system audio calls0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
