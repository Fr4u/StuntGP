#pragma once
// SCRIPTED COM ONLY, adapted from the NEW tests/source_force_feedback fixture.
// HWND/HINSTANCE are real; no fake window/thread/process APIs are used.
// Diff and source SHA are recorded in fixture-origin.json.
#include <Windows.h>
#include <dinput.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fixture {
inline void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
inline HWND receiving_window{};
inline HWND window() { return receiving_window; }
inline HINSTANCE instance() { return ::GetModuleHandleW(nullptr); }
inline constexpr GUID device2_iid{0x5944e682u,0xc92eu,0x11cfu,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};
inline bool guid_equal(const GUID& a, const GUID& b) { return std::memcmp(&a,&b,sizeof(GUID)) == 0; }

struct Recording {
    std::vector<std::string> calls;
    std::map<std::string,HRESULT> results;
    unsigned bad_calls{},bad_release{},effect_callbacks{};
    HRESULT call(const std::string& name) {
        calls.push_back(name);
        const auto it = results.find(name);
        return it == results.end() ? S_OK : it->second;
    }
    HRESULT unexpected(const std::string& name) { ++bad_calls; calls.push_back("UNEXPECTED:"+name); return E_UNEXPECTED; }
    void validate(bool value) { if (!value) ++bad_calls; }
};

// Verify against independently stated PE literals, not source_data.hpp itself.
inline bool valid_source_format(LPCDIDATAFORMAT f) {
    if (!f || f->dwSize != sizeof(DIDATAFORMAT) || f->dwObjSize != sizeof(DIOBJECTDATAFORMAT) ||
        f->dwFlags != 1 || f->dwDataSize != 80 || f->dwNumObjs != 44 || !f->rgodf) return false;
    constexpr std::array<DWORD,8> axes{0xa36d02e0u,0xa36d02e1u,0xa36d02e2u,0xa36d02f4u,
        0xa36d02f5u,0xa36d02e3u,0xa36d02e4u,0xa36d02e4u};
    for (DWORD i=0;i<44;++i) {
        const auto& o=f->rgodf[i];
        if (o.dwOfs != (i<12 ? i*4 : i+36) || o.dwType !=
            (i<8 ? 0x80ffff03u : i<12 ? 0x80ffff10u : 0x80ffff0cu) || o.dwFlags != (i<8 ? 0x100u : 0u)) return false;
        if (i>=12) { if (o.pguid) return false; }
        else {
            GUID expected{i<8 ? axes[i] : 0xa36d02f2u,0xc9f3u,0x11cfu,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};
            if (!o.pguid || !guid_equal(*o.pguid,expected)) return false;
        }
    }
    return true;
}

#define FIXTURE_UNUSED_METHOD(name, signature) \
    HRESULT STDMETHODCALLTYPE name signature override { return log.unexpected(label+"." #name); }

struct Device final : IDirectInputDevice2A {
    Recording& log;
    std::string label;
    Device* qi_target{this};
    ULONG refs{};
    DWORD subtype{6};
    unsigned effects{2};
    bool null_qi_success{};
    std::array<std::byte,80> packet{};
    std::function<void()> on_unacquire;
    std::function<void()> on_info;
    std::function<HRESULT(REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT*,LPUNKNOWN)> on_create_effect;
    Device(Recording& r,std::string name) : log(r),label(std::move(name)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,LPVOID* output) override {
        log.validate(output && guid_equal(iid,device2_iid));
        const auto hr=log.call(label+".qi");
        *output=nullptr;
        if (SUCCEEDED(hr) && !null_qi_success) { ++qi_target->refs; *output=static_cast<IDirectInputDevice2A*>(qi_target); }
        return hr;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { (void)log.unexpected(label+".AddRef"); return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        (void)log.call(label+".release");
        if (refs==0) { ++log.bad_release; return 0; }
        return --refs; //fixture lifetime intentionally stays available for assertions
    }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT f) override {
        log.validate(valid_source_format(f)); return log.call(label+".format");
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND w,DWORD flags) override {
        log.validate(w==window() && (flags==5 || flags==6));
        return log.call(label+".coop"+std::to_string(flags));
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID property,LPCDIPROPHEADER p) override {
        const auto id=reinterpret_cast<std::uintptr_t>(&property);
        log.validate(p && p->dwHeaderSize==16);
        if (id==4) {
            const auto* value=reinterpret_cast<const DIPROPRANGE*>(p);
            log.validate(p->dwSize==24 && p->dwHow==1 && (p->dwObj==0 || p->dwObj==4) && value->lMin==-5000 && value->lMax==5000);
            return log.call(label+".range"+std::to_string(p->dwObj));
        }
        if (id==5 || id==9) {
            const auto* value=reinterpret_cast<const DIPROPDWORD*>(p);
            log.validate(p->dwSize==20 && (id==5 ? p->dwHow==1 && (p->dwObj==0 || p->dwObj==4) && value->dwData==1250 :
                p->dwHow==0 && p->dwObj==0 && value->dwData==0));
            return log.call(label+(id==9 ? ".autocenter" : ".deadzone"+std::to_string(p->dwObj)));
        }
        return log.unexpected(label+".property");
    }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA callback,LPVOID context,DWORD flags) override {
        log.validate(flags==0);
        const auto hr=log.call(label+".effects");
        for (unsigned i=0;i<effects;++i) {
            DIEFFECTINFOA info{}; info.dwSize=static_cast<DWORD>(sizeof(info)); info.guid.Data1=i+101;
            log.validate(callback(&info,context)==TRUE); ++log.effect_callbacks;
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA info) override {
        log.validate(info && info->dwSize==0x244);
        const auto* bytes=reinterpret_cast<const unsigned char*>(info);
        for (std::size_t i=4;i<sizeof(*info);++i) log.validate(bytes[i]==0);
        if (on_info) on_info();
        info->dwDevType=(subtype<<8u)|4u;
        return log.call(label+".info");
    }
    HRESULT STDMETHODCALLTYPE Poll() override { return log.call(label+".poll"); }
    HRESULT STDMETHODCALLTYPE Acquire() override { return log.call(label+".acquire"); }
    HRESULT STDMETHODCALLTYPE Unacquire() override {
        const auto hr=log.call(label+".unacquire"); if (on_unacquire) on_unacquire(); return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID output) override {
        log.validate(size==80 && output); std::memcpy(output,packet.data(),80);
        return log.call(label+".state");
    }
    FIXTURE_UNUSED_METHOD(GetCapabilities,(LPDIDEVCAPS))
    FIXTURE_UNUSED_METHOD(EnumObjects,(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD))
    FIXTURE_UNUSED_METHOD(GetProperty,(REFGUID,LPDIPROPHEADER))
    FIXTURE_UNUSED_METHOD(GetDeviceData,(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD))
    FIXTURE_UNUSED_METHOD(SetEventNotification,(HANDLE))
    FIXTURE_UNUSED_METHOD(GetObjectInfo,(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD))
    FIXTURE_UNUSED_METHOD(RunControlPanel,(HWND,DWORD))
    FIXTURE_UNUSED_METHOD(Initialize,(HINSTANCE,DWORD,REFGUID))
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID guid,LPCDIEFFECT parameters,
        LPDIRECTINPUTEFFECT* output,LPUNKNOWN outer) override {
        if(on_create_effect)return on_create_effect(guid,parameters,output,outer);
        return log.unexpected(label+".create_effect");
    }
    FIXTURE_UNUSED_METHOD(GetEffectInfo,(LPDIEFFECTINFOA,REFGUID))
    FIXTURE_UNUSED_METHOD(GetForceFeedbackState,(LPDWORD))
    FIXTURE_UNUSED_METHOD(SendForceFeedbackCommand,(DWORD))
    FIXTURE_UNUSED_METHOD(EnumCreatedEffectObjects,(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD))
    FIXTURE_UNUSED_METHOD(Escape,(LPDIEFFESCAPE))
    FIXTURE_UNUSED_METHOD(SendDeviceData,(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD))
};

struct Root final : IDirectInputA {
    Recording& log;
    std::string label{"root"};
    ULONG refs{};
    std::map<DWORD,Device*> devices;
    std::array<std::vector<DWORD>,2> enumeration;
    std::vector<BOOL> callback_results;
    explicit Root(Recording& r) : log(r) {}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid,LPDIRECTINPUTDEVICEA* output,LPUNKNOWN outer) override {
        log.validate(output && !outer && devices.contains(guid.Data1));
        const auto hr=log.call("root.create"+std::to_string(guid.Data1));
        *output=nullptr;
        if (SUCCEEDED(hr)) { auto* device=devices.at(guid.Data1); ++device->refs; *output=device; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD type,LPDIENUMDEVICESCALLBACKA callback,LPVOID context,DWORD flags) override {
        log.validate(type==4 && (flags==0x101 || flags==1));
        const auto hr=log.call(flags==0x101 ? "root.enum101" : "root.enum1");
        for (const auto id : enumeration[flags==0x101 ? 0 : 1]) {
            DIDEVICEINSTANCEA info{}; info.dwSize=static_cast<DWORD>(sizeof(info)); info.guidInstance.Data1=id;
            const auto keep=callback(&info,context); callback_results.push_back(keep);
            if (!keep) break;
        }
        return hr;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { (void)log.unexpected("root.AddRef"); return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        (void)log.call("root.release"); if (!refs) { ++log.bad_release; return 0; } return --refs;
    }
    FIXTURE_UNUSED_METHOD(QueryInterface,(REFIID,LPVOID*))
    FIXTURE_UNUSED_METHOD(GetDeviceStatus,(REFGUID))
    FIXTURE_UNUSED_METHOD(RunControlPanel,(HWND,DWORD))
    FIXTURE_UNUSED_METHOD(Initialize,(HINSTANCE,DWORD))
};
#undef FIXTURE_UNUSED_METHOD

// Owned copies only; no invocation pointer escapes into a later assertion.
struct EffectParameters {
    std::array<DWORD,9> header{}; //size/flags/duration/sample/gain/button/repeat/axes/typeBytes
    std::vector<DWORD> axes,type;
    std::vector<LONG> directions;
    explicit EffectParameters(LPCDIEFFECT p){
        require(p&&p->dwSize==sizeof(DIEFFECT_DX5),"native DX5 descriptor size");
        require(p->cAxes<=2&&p->rgdwAxes&&p->rglDirection&&!p->lpEnvelope,"native pointer relocation");
        require(p->lpvTypeSpecificParams&&(p->cbTypeSpecificParams==16||p->cbTypeSpecificParams==24),"type-specific layout");
        header={p->dwSize,p->dwFlags,p->dwDuration,p->dwSamplePeriod,p->dwGain,
            p->dwTriggerButton,p->dwTriggerRepeatInterval,p->cAxes,p->cbTypeSpecificParams};
        axes.assign(p->rgdwAxes,p->rgdwAxes+p->cAxes);
        directions.assign(p->rglDirection,p->rglDirection+p->cAxes);
        type.resize(p->cbTypeSpecificParams/4);std::memcpy(type.data(),p->lpvTypeSpecificParams,p->cbTypeSpecificParams);
    }
};
struct Effect final : IDirectInputEffect {
    Recording& log; std::string label; ULONG refs{1};
    std::function<void()> on_release,on_start;
    std::function<void(LPCDIEFFECT,DWORD)> on_set;
    std::vector<EffectParameters> parameters;
    std::vector<DWORD> set_flags;
    Effect(Recording& r,std::string name) : log(r),label(std::move(name)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,LPVOID*) override { return log.unexpected(label+".qi"); }
    ULONG STDMETHODCALLTYPE AddRef() override { (void)log.unexpected(label+".AddRef"); return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (on_release) on_release();
        (void)log.call(label+".release"); if (!refs) { ++log.bad_release; return 0; } return --refs;
    }
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT p,DWORD flags) override {
        parameters.emplace_back(p);set_flags.push_back(flags);
        const auto result=log.call(label+".set");if(on_set)on_set(p,flags);return result;
    }
    HRESULT STDMETHODCALLTYPE Start(DWORD iterations,DWORD flags) override {
        log.validate(iterations==1&&flags==0);
        const auto result=log.call(label+".start");if(on_start)on_start();return result;
    }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override{return log.unexpected(label+".initialize");}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override{return log.unexpected(label+".guid");}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT,DWORD) override{return log.unexpected(label+".get");}
    HRESULT STDMETHODCALLTYPE Stop() override{return log.unexpected(label+".stop");}
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD) override{return log.unexpected(label+".status");}
    HRESULT STDMETHODCALLTYPE Download() override{return log.unexpected(label+".download");}
    HRESULT STDMETHODCALLTYPE Unload() override{return log.unexpected(label+".unload");}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override{return log.unexpected(label+".escape");}
};
struct Scenario {
    Recording log;
    Root root{log};
    std::vector<std::unique_ptr<Device>> devices;
    std::vector<std::unique_ptr<Effect>> effects;
    Effect& add_effect(std::string label){
        auto p=std::make_unique<Effect>(log,std::move(label));p->refs=0;
        auto* result=p.get();effects.push_back(std::move(p));return *result;
    }
    Device& add(unsigned id,bool second=false,bool same_interface=false) {
        auto basic=std::make_unique<Device>(log,"b"+std::to_string(id));
        auto extended=std::make_unique<Device>(log,"e"+std::to_string(id));
        basic->qi_target=same_interface ? basic.get() : extended.get();
        auto* result=basic.get(); root.devices[id]=result;
        root.enumeration[second ? 1 : 0].push_back(id);
        devices.push_back(std::move(basic)); devices.push_back(std::move(extended));
        return *result;
    }
    bool all_released() const noexcept {
        if (root.refs!=0) return false;
        for(const auto& effect:effects)if(effect->refs!=0)return false;
        for (const auto& device : devices) if (device->refs!=0) return false;
        return true;
    }
    void clean() const {
        require(log.bad_calls==0 && log.bad_release==0,"unexpected COM call, wrong request or Release imbalance");
        require(all_released(),"COM reference leaked");
    }
};
inline Scenario* active{}; //one serial fixture, never an OS singleton
inline HRESULT WINAPI create(HINSTANCE instance_arg,DWORD version,LPDIRECTINPUTA* output,LPUNKNOWN outer) {
    active->log.validate(instance_arg==instance() && version==0x500 && !outer && output);
    const auto hr=active->log.call("factory500"); *output=nullptr;
    if (SUCCEEDED(hr)) { ++active->root.refs; *output=&active->root; }
    return hr;
}
}

