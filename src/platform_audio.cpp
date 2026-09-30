#include <cstddef>
#include "sgn/platform_audio.hpp"
#include "sgn/source_audio_primary_format.hpp"
#include "sgn/source_audio_default_device.hpp"
#include "sgn/source_audio_default_device_native.hpp"
#include "sgn/source_audio_enumeration_native.hpp"
#include "sgn/source_audio_listener_configuration.hpp"
#include "sgn/source_audio_sample_creator.hpp"
#include "sgn/source_audio_sample_load.hpp"
#include "sgn/source_audio_catalog_load.hpp"
#include <list>
#include <cstring>
#include "sgn/retail_audio_catalog.hpp"
#include "audio_ring_queue.hpp"
#include "sgn/audio_output_matrix.hpp"
#include <windows.h>
#include <xaudio2.h>
#include <x3daudio.h>
#include <wrl/client.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <thread>
#include <utility>
#include <vector>

namespace sgn
{
using namespace retail;
namespace
{
AudioResult result(HRESULT value)
{
    return static_cast<AudioResult>(value);
}
void check(HRESULT value, const char *operation)
{
    if (FAILED(value))
        throw AudioError(result(value), operation);
}
float gain(std::int32_t value)
{
    return value == -10000 ? 0.0f : std::pow(10.0f, static_cast<float>(value) / 2000.0f);
}
bool finite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
X3DAUDIO_VECTOR vector(Vec3 v)
{
    return {v.x, v.y, v.z};
}
bool unit(Vec3 in, Vec3 &out)
{
    const double length = std::hypot(double(in.x), double(in.y), double(in.z));
    if (!std::isfinite(length) || length == 0)
        return false;
    out = {float(in.x / length), float(in.y / length), float(in.z / length)};
    return true;
}
struct ComApartment
{
    bool owned{};
    ComApartment()
    {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr != RPC_E_CHANGED_MODE)
            check(hr, "initialize audio COM apartment");
        owned = SUCCEEDED(hr);
    }
    ~ComApartment()
    {
        if (owned)
            CoUninitialize();
    }
};
struct VoiceDeleter
{
    void operator()(IXAudio2MasteringVoice *voice) const noexcept
    {
        if (voice)
            voice->DestroyVoice();
    }
};
struct Voice final : IXAudio2VoiceCallback
{
    std::shared_ptr<const PcmWave> wave;
    IXAudio2SourceVoice *output{};
    std::atomic<HRESULT> error{S_OK};
    AudioSpatialParameters spatial{};
    std::vector<float> base_matrix, matrix;
    std::uint32_t ordinary_refs{1}, spatial_refs{}, frequency{}, loop{};
    std::int32_t volume{}, pan{};
    bool spatial_capable{}, started{};
    bool sealed{true};
    std::unique_ptr<sgn::detail::AudioRingQueue> ring;
    ~Voice()
    {
        if (ring)
            ring->shutdown();
        if (output)
            output->DestroyVoice();
    }
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void *) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void *context) override
    {
        if (ring)
            ring->on_buffer_end(context);
    }
    void STDMETHODCALLTYPE OnLoopEnd(void *) override {}
    void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT failure) override
    {
        error.store(failure);
    }
};
} // namespace
struct WindowsAudioDevice::Impl
{
    struct Modern final : IXAudio2EngineCallback
    {
        ComApartment apartment;
        Microsoft::WRL::ComPtr<IXAudio2> engine;
        std::unique_ptr<IXAudio2MasteringVoice, VoiceDeleter> master;
        std::map<std::uint64_t, std::unique_ptr<Voice>> voices;
        struct PendingPcm
        {
            std::shared_ptr<PcmWave> wave;
            bool locked{};
            std::uint32_t offset{}, count{};
        };
        std::map<std::uint64_t, PendingPcm> pending;
        std::uint64_t next{1};
        std::uint64_t device_generation{};
        bool device_open{};
        AudioWaveFormat primary{};
        float doppler_factor{1}, rolloff_factor{1}, distance_factor{1};
        std::atomic<HRESULT> critical{S_OK};
        bool callback_registered{};
        AudioDeviceInfo info;
        X3DAUDIO_HANDLE spatial_engine{};
        X3DAUDIO_LISTENER listener{};
        explicit Modern(float master_gain)
        {
            if (!std::isfinite(master_gain) || master_gain < 0 || master_gain > 1)
                throw std::invalid_argument("audio master gain outside0..1");
            check(XAudio2Create(engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR), "create XAudio2 engine");
            IXAudio2MasteringVoice *raw{};
            check(engine->CreateMasteringVoice(&raw), "create default audio output");
            master.reset(raw);
            XAUDIO2_VOICE_DETAILS details{};
            master->GetVoiceDetails(&details);
            info.channels = details.InputChannels;
            info.sample_rate = details.InputSampleRate;
            DWORD mask{};
            check(master->GetChannelMask(&mask), "read audio channel layout");
            info.channel_mask = mask;
            check(X3DAudioInitialize(mask, X3DAUDIO_SPEED_OF_SOUND, spatial_engine), "initialize spatial audio");
            listener.OrientFront = {0, 0, 1};
            listener.OrientTop = {0, 1, 0};
            check(master->SetVolume(master_gain), "set audio master gain");
            check(engine->RegisterForCallbacks(this), "register audio device health callback");
            callback_registered = true;
        }
        ~Modern()
        {
            voices.clear();
            if (callback_registered)
                engine->UnregisterForCallbacks(this);
            master.reset();
        }
        void STDMETHODCALLTYPE OnProcessingPassStart() override {}
        void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
        void STDMETHODCALLTYPE OnCriticalError(HRESULT failure) override
        {
            critical.store(failure);
        }
        Voice *find(std::uint64_t id)
        {
            const auto it = voices.find(id);
            return it == voices.end() ? nullptr : it->second.get();
        }
        HRESULT health(Voice *voice) const
        {
            if (FAILED(critical.load()))
                return critical.load();
            if (!voice)
                return E_INVALIDARG;
            if (FAILED(voice->error.load()))
                return voice->error.load();
            return voice->ring ? voice->ring->health() : S_OK;
        }
        HRESULT mix(Voice &voice)
        {
            const auto device_health = health(&voice);
            if (FAILED(device_health))
                return device_health;
            voice.matrix = voice.base_matrix;
            float doppler = 1;
            if (voice.spatial_capable && voice.spatial.mode != 2)
            {
                X3DAUDIO_EMITTER emitter{};
                emitter.ChannelCount = 1;
                emitter.CurveDistanceScaler = voice.spatial.minimum_distance * distance_factor;
                emitter.DopplerScaler = doppler_factor;
                emitter.OrientFront = {0, 0, 1};
                emitter.OrientTop = {0, 1, 0};
                emitter.Position = vector(voice.spatial.position * distance_factor);
                emitter.Velocity = vector(voice.spatial.velocity * distance_factor);
                X3DAUDIO_LISTENER receiver = listener;
                if (voice.spatial.mode == 1)
                {
                    receiver.Position = {};
                    receiver.Velocity = {};
                    receiver.OrientFront = {0, 0, 1};
                    receiver.OrientTop = {0, 1, 0};
                }
                receiver.Position = {receiver.Position.x * distance_factor, receiver.Position.y * distance_factor,
                                     receiver.Position.z * distance_factor};
                receiver.Velocity = {receiver.Velocity.x * distance_factor, receiver.Velocity.y * distance_factor,
                                     receiver.Velocity.z * distance_factor};
                const auto valid = [](X3DAUDIO_VECTOR v) {
                    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
                };
                if (!std::isfinite(emitter.CurveDistanceScaler) || emitter.CurveDistanceScaler <= 0 ||
                    !valid(emitter.Position) || !valid(emitter.Velocity) || !valid(receiver.Position) ||
                    !valid(receiver.Velocity))
                    return E_INVALIDARG;
                const double distance = std::hypot(double(emitter.Position.x) - receiver.Position.x,
                                                   double(emitter.Position.y) - receiver.Position.y,
                                                   double(emitter.Position.z) - receiver.Position.z);
                if (!std::isfinite(distance) || distance > std::sqrt(double(std::numeric_limits<float>::max())))
                    return E_INVALIDARG;
                X3DAUDIO_CONE cone{};
                if (voice.spatial.inside_cone_angle != 360 || voice.spatial.outside_cone_angle != 360)
                {
                    Vec3 direction;
                    if (!unit(voice.spatial.cone_orientation, direction))
                        return E_INVALIDARG;
                    emitter.OrientFront = vector(direction);
                    cone.InnerAngle = float(voice.spatial.inside_cone_angle) * X3DAUDIO_PI / 180;
                    cone.OuterAngle = float(voice.spatial.outside_cone_angle) * X3DAUDIO_PI / 180;
                    cone.InnerVolume = 1;
                    cone.OuterVolume = gain(voice.spatial.outside_cone_volume);
                    emitter.pCone = &cone;
                }
                X3DAUDIO_DSP_SETTINGS dsp{};
                dsp.SrcChannelCount = 1;
                dsp.DstChannelCount = info.channels;
                dsp.pMatrixCoefficients = voice.matrix.data();
                X3DAUDIO_DISTANCE_CURVE_POINT flat_points[2]{{0, 1}, {1, 1}};
                X3DAUDIO_DISTANCE_CURVE flat{flat_points, 2};
                emitter.pVolumeCurve = &flat;
                X3DAudioCalculate(spatial_engine, &receiver, &emitter,
                                  X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER, &dsp);
                doppler = dsp.DopplerFactor;
                // New-platform attenuation in source distance units. The source
                // controls its rolloff; this is not a DirectSound DSP bit match.
                const double relative = distance / (double(voice.spatial.minimum_distance) * distance_factor);
                const float attenuation =
                    relative <= 1 ? 1.0f : float(1.0 / (1.0 + double(rolloff_factor) * (relative - 1.0)));
                for (auto &coefficient : voice.matrix)
                    coefficient *= attenuation;
                // The source requests DSBCAPS_MUTE3DATMAXDISTANCE (0x20000).
                if (distance >= double(voice.spatial.maximum_distance) * distance_factor)
                    std::fill(voice.matrix.begin(), voice.matrix.end(), 0.0f);
            }
            else
            {
                unsigned channel = 0;
                for (unsigned bit = 0; bit < 32 && channel < info.channels; ++bit)
                    if (info.channel_mask & (1u << bit))
                    {
                        const unsigned speaker = 1u << bit;
                        float balance = 1;
                        if ((speaker & (SPEAKER_FRONT_LEFT | SPEAKER_BACK_LEFT | SPEAKER_SIDE_LEFT)) && voice.pan > 0)
                            balance = gain(-voice.pan);
                        if ((speaker & (SPEAKER_FRONT_RIGHT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_RIGHT)) &&
                            voice.pan < 0)
                            balance = gain(voice.pan);
                        if (!audio_host::scale_output_matrix_destination(voice.matrix, voice.wave->channels,
                                                                         info.channels, channel, balance))
                            return E_INVALIDARG;
                        ++channel;
                    }
            }
            const float ratio = float(voice.frequency) / float(voice.wave->sample_rate) * doppler;
            if (!std::isfinite(ratio) || ratio < XAUDIO2_MIN_FREQ_RATIO || ratio > XAUDIO2_MAX_FREQ_RATIO)
                return E_INVALIDARG;
            if (std::any_of(voice.matrix.begin(), voice.matrix.end(), [](float v) { return !std::isfinite(v); }))
                return E_INVALIDARG;
            const auto matrix_result =
                voice.output->SetOutputMatrix(master.get(), voice.wave->channels, info.channels, voice.matrix.data());
            if (FAILED(matrix_result))
                return matrix_result;
            return voice.output->SetFrequencyRatio(ratio);
        }
        std::unique_ptr<Voice> make_voice(std::shared_ptr<const PcmWave> wave, bool spatial)
        {
            if (!wave || wave->samples.empty() || wave->samples.size() > XAUDIO2_MAX_BUFFER_BYTES ||
                wave->channels < 1 || wave->channels > 2 || (spatial && wave->channels != 1) ||
                (wave->bits_per_sample != 8 && wave->bits_per_sample != 16) || wave->sample_rate < 1000 ||
                wave->sample_rate > 200000 || wave->block_align != wave->channels * (wave->bits_per_sample / 8) ||
                wave->samples.size() % wave->block_align)
                throw AudioError(result(E_INVALIDARG), "unsupported audio sample");
            if (FAILED(critical.load()))
                throw AudioError(result(critical.load()), "audio output device failed");
            auto voice = std::make_unique<Voice>();
            voice->wave = std::move(wave);
            voice->frequency = voice->wave->sample_rate;
            voice->spatial_capable = spatial;
            voice->spatial = audio_default_spatial_parameters();
            WAVEFORMATEX format{};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = voice->wave->channels;
            format.nSamplesPerSec = voice->wave->sample_rate;
            format.wBitsPerSample = voice->wave->bits_per_sample;
            format.nBlockAlign = voice->wave->block_align;
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            check(engine->CreateSourceVoice(&voice->output, &format, 0, XAUDIO2_MAX_FREQ_RATIO, voice.get()),
                  "create audio source voice");
            voice->base_matrix.resize(std::size_t(format.nChannels) * info.channels);
            voice->output->GetOutputMatrix(master.get(), format.nChannels, info.channels, voice->base_matrix.data());
            check(mix(*voice), "configure new audio source");
            return voice;
        }
        AudioBufferId create(std::shared_ptr<const PcmWave> wave, bool spatial)
        {
            if (next == 0)
                throw AudioError(result(E_OUTOFMEMORY), "audio identity space exhausted");
            auto voice = make_voice(std::move(wave), spatial);
            const AudioBufferId id{next++};
            voices.emplace(id.value, std::move(voice));
            return id;
        }
        HRESULT rewind(std::uint64_t id)
        {
            auto &old = voices.at(id);
            auto replacement = make_voice(old->wave, old->spatial_capable);
            replacement->ordinary_refs = old->ordinary_refs;
            replacement->spatial_refs = old->spatial_refs;
            replacement->frequency = old->frequency;
            replacement->volume = old->volume;
            replacement->pan = old->pan;
            replacement->spatial = old->spatial;
            replacement->sealed = old->sealed;
            if (old->ring)
            {
                replacement->ring = std::make_unique<sgn::detail::AudioRingQueue>(
                    *replacement->output, static_cast<std::uint32_t>(old->wave->samples.size()), old->wave->block_align,
                    old->wave->sample_rate);
                replacement->ring->publish(old->wave->samples);
            }
            auto hr = mix(*replacement);
            if (FAILED(hr))
                return hr;
            hr = replacement->output->SetVolume(gain(replacement->volume));
            if (FAILED(hr))
                return hr;
            // Stop and FlushSourceBuffers are asynchronous. Replacing the stopped
            // backend voice joins its callbacks via DestroyVoice, so a successful
            // source cursor reset is complete before the next Play. The public ID,
            // both interface references and immutable PCM ownership stay unchanged.
            // This occurs on explicit rewind, never on ordinary Stop or each tick.
            old = std::move(replacement);
            return S_OK;
        }
        HRESULT refresh_spatial()
        {
            HRESULT first = critical.load();
            if (FAILED(first))
                return first;
            for (auto &[id, voice] : voices)
            {
                (void)id;
                if (voice->spatial_capable)
                {
                    const auto hr = mix(*voice);
                    if (FAILED(hr) && SUCCEEDED(first))
                        first = hr;
                }
            }
            return first;
        }
    };

    const std::thread::id owner_thread{std::this_thread::get_id()};
    const float master_gain;
    const DirectSoundCreateEntry create_entry;
    const SourceAudioListenerQueryReceivingEntry listener_query_receiving;
    // Exactly one owned reference. GetAddressOf permits the real API to write
    // this retained slot directly; no temporary probe or ReleaseAndGetAddressOf.
    Microsoft::WRL::ComPtr<IDirectSound> source_device;
    // One native output reference; no added device/window/mixer owner.
    Microsoft::WRL::ComPtr<IDirectSoundBuffer> source_primary;
    Microsoft::WRL::ComPtr<IDirectSound3DListener> source_listener; // One QI reference.
    // 462090 clears the source primary/listener cells without Release. Keep
    // each displaced reference until NEW host teardown, including repeated
    // native pointer identities. These are ownership obligations, not new
    // source globals or cached replacements for the current source output.
    std::list<Microsoft::WRL::ComPtr<IDirectSoundBuffer>> source_displaced_primaries;
    std::list<Microsoft::WRL::ComPtr<IDirectSound3DListener>> source_displaced_listeners;
    bool source_primary_format_succeeded{}; // NEW admission after actual A5 zero.
    bool source_listener_query_succeeded{}; // NEW admission for actual A6 zero.
    bool source_samples_reached{};          // NEW history admission, not a source ready flag.
    struct SourceSampleReferences
    {
        Microsoft::WRL::ComPtr<IDirectSoundBuffer> buffer;
        Microsoft::WRL::ComPtr<IDirectSound3DBuffer> spatial;
    };
    // Allocate an empty node before native calls. Each COM reference is retained
    // even if its HRESULT/exception prevents publication into the source sample.
    std::list<SourceSampleReferences> source_samples;
    // 460A80 never releases its acquired IKsPropertySet reference. Preserve one
    // owned reference per successful query until NEW host owner destruction,
    // even when successive queries return the same pointer identity.
    std::list<Microsoft::WRL::ComPtr<IKsPropertySet>> source_properties;
    // NEW HOST (R35b): COM identities of every child reference (primary, listener, displaced primaries and
    // listeners, sample buffers and 3D buffers, property sets) that was retained when 4621EA's Release
    // destroyed the IDirectSound device. DirectSound frees a device's buffers together with the device, so
    // these references are dangling; host teardown forgets them instead of calling Release on freed memory
    // (the 0xC0000005 in ~Impl at every process exit). Identities only; never dereferenced. A later live
    // child that reuses such an address is merely not Released before its own device is (the device frees it).
    std::vector<const void *> source_destroyed_children;
    const AudioListenerId *source_listener_slot{}; // Borrowed same-world address, never dereferenced after consumption.
    std::unique_ptr<Modern> modern_backend;
    bool source_path{}, preview_path{}, creating{};
    bool source_primary_creation_succeeded{}; // NEW host admission for actual A4 zero result.
    bool source_cooperative_completed{};      // NEW host admission, independent of HRESULT.
    bool source_creation_succeeded{};         // NEW admission: a nonnull failed output is not a reached success branch.
    Impl(float gain, SourceAudioDefaultDeviceNativeApi api)
        : master_gain(gain), create_entry(api.create), listener_query_receiving(api.listener_query_receiving)
    {
        if (!std::isfinite(gain) || gain < 0 || gain > 1)
            throw std::invalid_argument("audio master gain outside0..1");
        if (!create_entry)
            throw SourceAudioDefaultDeviceError(0x462146u, "462146 missing DirectSoundCreate entry");
    }
    void owner() const
    {
        if (std::this_thread::get_id() != owner_thread)
            throw SourceAudioDefaultDeviceError(0x462146u, "audio owner called from another thread");
    }
    ~Impl()
    {
        // Destructors cannot report a recoverable ownership violation. Reject
        // before either native owner is destroyed on a foreign thread.
        if (std::this_thread::get_id() != owner_thread)
            std::terminate();
        creating = true; // Reject mutations reentered from final native Release.
        modern_backend.reset();
        // NEW host lifetime cleanup, not a recovered source failure branch.
        // Release the child reference while its parent is still retained.
        // NEW HOST (R35b): a child whose device was already destroyed is forgotten, not Released.
        const auto release_child = [this](auto &child) {
            if (child.Get() != nullptr && destroyed_child(child.Get()))
                (void)child.Detach();
            else
                child.Reset();
        };
        for (auto &property : source_properties)
            release_child(property);
        source_properties.clear();
        for (auto &sample : source_samples)
            release_child(sample.spatial);
        for (auto &sample : source_samples)
            release_child(sample.buffer);
        source_samples.clear();
        release_child(source_listener);
        for (auto &listener : source_displaced_listeners)
            release_child(listener);
        source_displaced_listeners.clear();
        release_child(source_primary);
        for (auto &primary : source_displaced_primaries)
            release_child(primary);
        source_displaced_primaries.clear();
        source_device.Reset();
    }
    // NEW HOST (R35b): host teardown bookkeeping for children of a destroyed device (see source_destroyed_children).
    bool destroyed_child(const void *identity) const noexcept
    {
        return std::find(source_destroyed_children.begin(), source_destroyed_children.end(), identity) !=
               source_destroyed_children.end();
    }
    void remember_destroyed_children()
    {
        const auto remember = [this](const auto &child) {
            if (child.Get() != nullptr && !destroyed_child(child.Get()))
                source_destroyed_children.push_back(child.Get());
        };
        for (const auto &property : source_properties)
            remember(property);
        for (const auto &sample : source_samples)
        {
            remember(sample.spatial);
            remember(sample.buffer);
        }
        remember(source_listener);
        for (const auto &listener : source_displaced_listeners)
            remember(listener);
        remember(source_primary);
        for (const auto &primary : source_displaced_primaries)
            remember(primary);
    }
    Modern &modern() const
    {
        owner();
        if (!modern_backend)
            throw SourceAudioDefaultDeviceError(
                0x462165u, "modern audio backend is not prepared; source continuation remains bounded");
        return *modern_backend;
    }
    void prepare()
    {
        owner();
        if (source_path)
            throw SourceAudioDefaultDeviceError(
                0x462165u, "source DirectSound owner is not qualified for modern playback attachment");
        preview_path = true; // Retain admission even if a native constructor fails.
        if (!modern_backend)
            modern_backend = std::make_unique<Modern>(master_gain);
    }
    std::uint64_t identity() const noexcept
    {
        static_assert(sizeof(IDirectSound *) == sizeof(std::uintptr_t));
        static_assert(sizeof(std::uintptr_t) <= sizeof(AudioDeviceId{}.value));
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source_device.Get()));
    }
    AudioResult create_source(AudioDeviceId &out)
    {
        owner();
        if (preview_path)
            throw SourceAudioDefaultDeviceError(
                0x462146u, "462146 source creation cannot follow an attempted preview backend preparation");
        if (creating || source_device.Get() != nullptr)
            throw SourceAudioDefaultDeviceError(0x462146u,
                                                "462146 would overwrite an in-flight or retained source interface");
        if (out.value != 0)
            throw SourceAudioDefaultDeviceError(
                0x462146u, "462146 requires the actual zero output from the cold prefix or source release");
        source_path = true;
        creating = true;
        source_creation_succeeded = false;
        struct Publish
        {
            Impl &owner;
            AudioDeviceId &out;
            ~Publish() noexcept
            {
                out.value = owner.identity();
                owner.creating = false;
            }
        } publish{*this, out};
        // HRESULT sign does not decide pointer publication or ownership. A
        // nonnull output remains owned even on failure; no rollback to zero.
        const auto hr = result(create_entry(nullptr, source_device.GetAddressOf(), nullptr));
        source_creation_succeeded = hr == 0;
        return hr;
    }
    AudioResult source_cooperative(AudioDeviceId id, std::uintptr_t window, std::uint32_t level)
    {
        owner();
        if (creating || !source_path || !source_creation_succeeded || source_device.Get() == nullptr ||
            id.value != identity())
            throw SourceAudioDefaultDeviceError(0x462165u, "462165 requires the same retained source interface token");
        static_assert(DSSCL_EXCLUSIVE == 3);
        if (level != 3)
            throw SourceAudioDefaultDeviceError(0x462165u, "462165 source cooperative level must be3");
        creating = true;
        source_cooperative_completed = false;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        // No IsWindow/health fallback or HRESULT-dependent teardown. Native HWND
        // validity errors must come from the actual interface. The platform
        // composition separately enforces the known same-window lifetime.
        const auto hr = result(source_device->SetCooperativeLevel(reinterpret_cast<HWND>(window), level));
        source_cooperative_completed = true; // 462168 is unconditional after return.
        return hr;
    }
    std::uint64_t primary_identity() const noexcept
    {
        static_assert(sizeof(std::uintptr_t) <= sizeof(AudioPrimaryId{}.value));
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source_primary.Get()));
    }
    AudioResult create_primary(AudioDeviceId id, const AudioBufferRequest &request, AudioPrimaryId &out)
    {
        owner();
        if (creating || !source_path || !source_creation_succeeded || !source_cooperative_completed ||
            source_device.Get() == nullptr || id.value != identity())
            throw SourceAudioDefaultDeviceError(
                0x461d25u, "461D25 requires the same retained device after a completed cooperative call");
        if (out.value != 0)
            throw SourceAudioDefaultDeviceError(0x461d25u, "461D25 requires the zero source primary output from4620E7");
        if (request.size != 20 || request.flags != 0x11 || request.bytes != 0 || request.reserved != 0 ||
            request.format)
            throw SourceAudioDefaultDeviceError(0x461d25u, "461D25 requires the exact source primary descriptor");
        // The source DSBUFFERDESC1 has a 32-bit format pointer and size20.
        // Native DSBUFFERDESC1 is the same five-field version with a native
        // pointer (size24 on Win64). Backing storage has the actual SDK type;
        // dwSize selects its legacy prefix. No alias cast or algorithm upgrade.
        static_assert(sizeof(DWORD) == 4 && sizeof(DSBUFFERDESC1) == 16 + sizeof(void *));
        static_assert(offsetof(DSBUFFERDESC, dwSize) == offsetof(DSBUFFERDESC1, dwSize));
        static_assert(offsetof(DSBUFFERDESC, dwFlags) == offsetof(DSBUFFERDESC1, dwFlags));
        static_assert(offsetof(DSBUFFERDESC, dwBufferBytes) == offsetof(DSBUFFERDESC1, dwBufferBytes));
        static_assert(offsetof(DSBUFFERDESC, dwReserved) == offsetof(DSBUFFERDESC1, dwReserved));
        static_assert(offsetof(DSBUFFERDESC, lpwfxFormat) == 16 && offsetof(DSBUFFERDESC1, lpwfxFormat) == 16);
        static_assert(sizeof(DSBUFFERDESC) >= sizeof(DSBUFFERDESC1));
        static_assert((DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D) == 0x11);
        DSBUFFERDESC native{};
        native.dwSize = static_cast<DWORD>(sizeof(DSBUFFERDESC1));
        native.dwFlags = request.flags;
        native.dwBufferBytes = request.bytes;
        native.dwReserved = request.reserved;
        native.lpwfxFormat = nullptr;
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        if (source_primary.Get() != nullptr)
        {
            // Allocate before transfer and before the output publisher exists.
            // Allocation failure leaves the source's already-cleared output
            // untouched. Attach/Detach adds no native AddRef or Release.
            auto &displaced = source_displaced_primaries.emplace_back();
            displaced.Attach(source_primary.Detach());
        }
        source_cooperative_completed = false;
        source_primary_creation_succeeded = false;
        source_primary_format_succeeded = false;
        struct Publish
        {
            Impl &owner;
            AudioPrimaryId &out;
            ~Publish() noexcept
            {
                out.value = owner.primary_identity();
            }
        } publish{*this, out};
        // The API writes the retained output slot. Preserve a valid nonnull
        // output even on nonzero HRESULT or a receiving exception. No rollback.
        const auto hr = result(source_device->CreateSoundBuffer(&native, source_primary.GetAddressOf(), nullptr));
        source_primary_creation_succeeded = hr == 0;
        return hr;
    }
    SourceAudioPrimaryFormatResult complete_primary_format(AudioDeviceId id, AudioPrimaryId &primary,
                                                           const std::uint32_t &requested_rate,
                                                           std::uint32_t &capability, std::uint32_t &enabled)
    {
        owner();
        if (creating || !source_primary_creation_succeeded || source_device.Get() == nullptr || id.value != identity())
            throw SourceAudioDefaultDeviceError(
                0x461d4cu, "461D4C requires actual successful primary creation on the retained device");
        creating = true;
        source_primary_creation_succeeded = false;
        source_primary_format_succeeded = false;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        static_assert(sizeof(DSCAPS) == 96 && offsetof(DSCAPS, dwFlags) == 4 &&
                      offsetof(DSCAPS, dwMaxHw3DAllBuffers) == 44);
        static_assert(sizeof(WAVEFORMATEX) == 18 && offsetof(WAVEFORMATEX, wFormatTag) == 0 &&
                      offsetof(WAVEFORMATEX, nChannels) == 2);
        static_assert(offsetof(WAVEFORMATEX, nSamplesPerSec) == 4 && offsetof(WAVEFORMATEX, nAvgBytesPerSec) == 8);
        static_assert(offsetof(WAVEFORMATEX, nBlockAlign) == 12 && offsetof(WAVEFORMATEX, wBitsPerSample) == 14 &&
                      offsetof(WAVEFORMATEX, cbSize) == 16);
        static_assert(DSCAPS_PRIMARY16BIT == 8);
        DSCAPS caps;
        caps.dwSize = 96; // Only this source local field is initialized.
        const auto caps_hr = result(source_device->GetCaps(&caps));
        if (caps_hr != 0)
            return {0x461d4cu, caps_hr};
        if (caps.dwMaxHw3DAllBuffers < 8u)
        {
            capability = 0;
            enabled = 0;
        } // 461D60 before461D66.
        // Source loads62E114 after GetCaps and dereferences at461D78. Keep
        // already reached global clears on a missing or changed primary token.
        if (source_primary.Get() == nullptr || primary.value != primary_identity())
            throw SourceAudioDefaultDeviceError(0x461d78u, "461D78 requires the actual retained primary interface");
        WAVEFORMATEX format;
        DWORD written; // No invented source default format.
        const auto format_hr = result(source_primary->GetFormat(&format, 18, &written));
        if (format_hr != 0)
            return {0x461d7eu, format_hr};
        // The source never reads written. Keep this exact native output object,
        // including returned channels/cbSize, through the subsequent SetFormat.
        const std::uint32_t flags = caps.dwFlags;  // 461D8C.
        const std::uint32_t rate = requested_rate; // 461D92, after GetFormat.
        format.nSamplesPerSec = rate;
        const std::uint32_t channels = format.nChannels;
        format.wFormatTag = 1;
        format.wBitsPerSample = static_cast<WORD>((flags & 8u) ? 16u : 8u);
        // The source signed product/divide is nonnegative for ushort channels
        // and8/16 bits. Truncate to the source WORD before the DWORD multiply.
        format.nBlockAlign = static_cast<WORD>((channels * format.wBitsPerSample) >> 3u);
        format.nAvgBytesPerSec = static_cast<std::uint32_t>(format.nBlockAlign) * rate;
        // Source reloads62E114 at461DE2 before its461DE8 vtable read.
        if (primary.value != primary_identity())
            throw SourceAudioDefaultDeviceError(0x461de8u,
                                                "461DE8 requires the unchanged retained primary interface token");
        const auto set_hr = result(source_primary->SetFormat(&format));
        source_primary_format_succeeded = set_hr == 0;
        return {0x461deau, set_hr};
    }
    std::uint64_t listener_identity() const noexcept
    {
        static_assert(sizeof(std::uintptr_t) <= sizeof(AudioListenerId{}.value));
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source_listener.Get()));
    }
    IDirectSound3DListener *retained_listener(AudioListenerId id) const noexcept
    {
        if (id.value == 0)
            return nullptr;
        if (source_listener.Get() != nullptr && id.value == listener_identity())
            return source_listener.Get();
        for (const auto &retained : source_displaced_listeners)
            if (retained.Get() != nullptr &&
                id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(retained.Get())))
                return retained.Get();
        return nullptr;
    }
    SourceAudioListenerAcquisitionOutcome acquire_listener(AudioPrimaryId &primary, AudioListenerId &listener,
                                                           const std::uint32_t &mode, std::uint32_t &enabled)
    {
        owner();
        if (creating || !source_path || !source_primary_format_succeeded || source_device.Get() == nullptr)
            throw SourceAudioDefaultDeviceError(0x462174u, "462174 requires this owner's actual successful461CF0");
        creating = true;
        source_primary_format_succeeded = false;
        source_listener_query_succeeded = false;
        source_listener_slot = nullptr;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        if (mode == 0)
        {
            enabled = 0;
            source_samples_reached = true;
            return SourceAudioSamplesBoundary{};
        } // 462174..462182.
        if (enabled != 1)
        {
            source_samples_reached = true;
            return SourceAudioSamplesBoundary{};
        } // 46218E..462194.
        if (primary.value == 0)
            return SourceAudioListenerAcquisitionFailure{0x461a89u, std::nullopt};
        if (source_primary.Get() == nullptr || primary.value != primary_identity())
            throw SourceAudioDefaultDeviceError(0x461a8au, "461A8A requires the retained source primary interface");
        if (listener.value > std::numeric_limits<std::uintptr_t>::max())
            throw SourceAudioDefaultDeviceError(0x461a97u, "461A97 cannot truncate an input listener token");
        if (source_listener.Get() != nullptr)
        {
            // 4620ED cleared the source cell; 461A97 never releases its old QI
            // reference. Retain that obligation before invoking QI. Do not
            // clear, copy or otherwise rewrite the actual source output cell.
            auto &displaced = source_displaced_listeners.emplace_back();
            displaced.Attach(source_listener.Detach());
        }
        // PE46FE80, byte-identical to the SDK IID_IDirectSound3DListener.
        static constexpr GUID iid{
            0x279afa84u, 0x4981u, 0x11ceu, {0xa5u, 0x21u, 0x00u, 0x20u, 0xafu, 0x0bu, 0xe5u, 0x60u}};
        static_assert(sizeof(GUID) == 16);
        // The actual source cell is a live void* object, shared with callbacks.
        // No shadow output, final copy-back or inference from changed bits.
        auto **const output = listener.value.native_address();
        AudioResult hr{};
        if (listener_query_receiving)
        {
            struct Transfer final : SourceAudioListenerReferenceTransfer
            {
                Impl &parent;
                bool transferred{};
                explicit Transfer(Impl &value) : parent(value) {}
                void transfer_acquired(Microsoft::WRL::ComPtr<IDirectSound3DListener> &incoming) override
                {
                    parent.owner(); // Before reading non-atomic state or incoming.
                    if (!parent.creating || transferred || parent.source_listener.Get() != nullptr ||
                        incoming.Get() == nullptr)
                        throw SourceAudioDefaultDeviceError(0x461a97u,
                                                            "461A97 invalid or repeated receiving reference transfer");
                    parent.source_listener.Attach(incoming.Detach());
                    transferred = true;
                }
            } transfer{*this};
            hr = result(listener_query_receiving(source_primary.Get(), iid, output, transfer));
            // Consistency check of explicit metadata, never a second adoption.
            if (hr == 0 && *output != nullptr && (!transfer.transferred || *output != source_listener.Get()))
                throw SourceAudioDefaultDeviceError(
                    0x461a97u, "461A97 successful receiving output lacks its matching transferred reference");
        }
        else
        {
            hr = result(source_primary->QueryInterface(iid, output)); // 461A97.
            // Ordinary successful QI returns one owned interface reference,
            // including when the output bits equal the initial source token.
            // Hidden provider references are not guessed or adopted here.
            if (hr == 0 && *output != nullptr)
                source_listener.Attach(static_cast<IDirectSound3DListener *>(*output));
        }
        const bool returned_zero = hr == 0;
        source_listener_query_succeeded = returned_zero;
        if (returned_zero)
            source_listener_slot = &listener;
        if (hr != 0)
            return SourceAudioListenerAcquisitionFailure{0x461a97u, hr};
        // Zero with a null output still reaches461AA0. Its first dereference is
        // 461AAD, owned by the next stage; no early setter or readiness write.
        return SourceAudioListenerConfigurationBoundary{listener};
    }
    SourceAudioSamplesBoundary configure_listener(const AudioListenerId &listener)
    {
        owner();
        if (creating || !source_path || !source_listener_query_succeeded || source_listener_slot != &listener)
            throw SourceAudioDefaultDeviceError(
                0x461aa0u, "461AA0 requires the actual successful QI and the same retained output slot");
        creating = true;
        source_listener_query_succeeded = false;
        source_listener_slot = nullptr;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        // Only a transient borrowing adapter. The sealed leaf owns source call
        // order, factor bits, apply words and late listener-slot reloads.
        struct Setup final : AudioListenerSetupDevice
        {
            Impl &owner;
            explicit Setup(Impl &value) : owner(value) {}
            IDirectSound3DListener &checked(AudioListenerId id, std::uint32_t site)
            {
                if (auto *retained = owner.retained_listener(id))
                    return *retained;
                throw SourceAudioListenerConfigurationError(
                    site, "source listener reload is not a retained acquired interface");
            }
            AudioResult query_listener(AudioPrimaryId, AudioListenerId &) override
            {
                throw SourceAudioListenerConfigurationError(0x461a97u,
                                                            "configuration adapter cannot repeat QueryInterface");
            }
            AudioResult listener_doppler(AudioListenerId id, float factor, std::uint32_t apply) override
            {
                return result(checked(id, 0x461abeu).SetDopplerFactor(factor, apply));
            }
            AudioResult listener_rolloff(AudioListenerId id, float factor, std::uint32_t apply) override
            {
                return result(checked(id, 0x461aadu).SetRolloffFactor(factor, apply));
            }
            AudioResult listener_distance(AudioListenerId id, float factor, std::uint32_t apply) override
            {
                return result(checked(id, 0x461ad0u).SetDistanceFactor(factor, apply));
            }
        } setup{*this};
        (void)configure_source_audio_listener_461aa0(listener, setup);
        source_samples_reached = true;
        // 461ADB returns1, the caller's461A9B test branches to462182. No sample
        // load, listener release, initialized flag or later setting write here.
        return SourceAudioSamplesBoundary{};
    }
    IDirectSoundBuffer *retained_sample(AudioBufferId id) const noexcept
    {
        if (id.value)
            for (const auto &refs : source_samples)
                if (refs.buffer.Get() &&
                    id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(refs.buffer.Get())))
                    return refs.buffer.Get();
        return nullptr;
    }
    bool sample_stop_admitted(AudioBufferId id) const
    {
        owner();
        if (creating)
            throw SourceAudioDefaultDeviceError(0x460d79u,
                                                "source sample Stop cannot reenter an active native operation");
        return source_path && source_samples_reached && source_device.Get() != nullptr &&
               source_primary.Get() != nullptr && retained_sample(id) != nullptr;
    }
    AudioResult stop_source_sample(AudioBufferId id)
    {
        if (!sample_stop_admitted(id))
            throw SourceAudioDefaultDeviceError(0x460d79u, "source sample Stop requires this owner's retained buffer");
        auto *buffer = retained_sample(id);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        // 460D79: every returned HRESULT reaches the caller's 460D7C DEC.
        // The existing source loop owns that write; native exceptions propagate.
        return result(buffer->Stop());
    }
    void require_source_property_owner(std::uint32_t site) const
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(site,
                                                "source property call requires the retained idle DirectSound owner");
    }
    static GUID property_guid(const AudioPropertyGuid &bytes) noexcept
    {
        GUID guid;
        static_assert(sizeof(guid) == 16);
        std::memcpy(&guid, bytes.data(), sizeof(guid));
        return guid;
    }
    AudioResult query_source_property(AudioSpatialId spatial, const AudioPropertyGuid &iid, AudioPropertyId &output)
    {
        require_source_property_owner(0x460a9c);
        if (iid != audio_property_interface || output.value != 0)
            throw SourceAudioDefaultDeviceError(0x460a9c,
                                                "source property query requires the literal IID and zero local output");
        IDirectSound3DBuffer *actual = nullptr;
        for (const auto &sample : source_samples)
            if (sample.spatial.Get() &&
                spatial.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(sample.spatial.Get())))
            {
                actual = sample.spatial.Get();
                break;
            }
        if (!actual)
            throw SourceAudioDefaultDeviceError(0x460a94,
                                                "source property query spatial identity is null, foreign or released");
        // Reserve retention storage BEFORE the native call; no post-query allocation.
        auto &retained = source_properties.emplace_back();
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        void *native = nullptr; // 460A8C: actual call-local output, not a global slot.
        const auto hr = result(actual->QueryInterface(property_guid(iid), &native));
        // Only successful SDK QI transfers a reference. A failed nonnull output
        // is neither fabricated ownership nor a reason to issue an extra Release.
        if (hr == 0 && native)
            retained.Attach(static_cast<IKsPropertySet *>(native));
        else
            source_properties.pop_back(); // Empty metadata only; no native Release.
        output.value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(native));
        return hr; // Any nonzero HRESULT is handled by the unchanged source caller.
    }
    IKsPropertySet &checked_source_property(AudioPropertyId id, std::uint32_t site)
    {
        require_source_property_owner(site);
        for (const auto &property : source_properties)
            if (property.Get() &&
                id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(property.Get())))
                return *property.Get();
        throw SourceAudioDefaultDeviceError(site, "source property identity is null or not acquired by this owner");
    }
    AudioResult support_source_property(AudioPropertyId id, const AudioPropertyGuid &set, std::uint32_t identifier,
                                        std::uint32_t &support)
    {
        auto &property = checked_source_property(id, 0x460704);
        if (set != audio_environment_property_set || identifier != 1)
            throw SourceAudioDefaultDeviceError(0x46071b,
                                                "source environment support requires its literal set and property1");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        ULONG native = support;
        const auto hr = result(property.QuerySupport(property_guid(set), identifier, &native));
        support = static_cast<std::uint32_t>(native);
        return hr;
    }
    AudioResult set_source_property(AudioPropertyId id, const AudioPropertyGuid &set, std::uint32_t identifier,
                                    std::span<const std::uint8_t> instance, std::span<const std::uint8_t> data)
    {
        auto &property = checked_source_property(id, 0x460ac9);
        if (set != audio_environment_property_set || identifier != 1 || !instance.empty() || data.size() != 56)
            throw SourceAudioDefaultDeviceError(
                0x460ad5, "source environment Set requires NULL instance and actual56-byte properties");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        // Same source properties storage, no synthesized EAX support or data copy.
        return result(
            property.Set(property_guid(set), identifier, nullptr, 0, const_cast<std::uint8_t *>(data.data()), 56));
    }
    IDirectSoundBuffer *checked_playback_sample(AudioBufferId id, std::uint32_t site)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(site, "source sample playback requires the retained native owner");
        auto *buffer = retained_sample(id);
        if (!buffer)
            throw SourceAudioDefaultDeviceError(site, "source sample identity is not retained by this owner");
        return buffer;
    }
    AudioResult sample_status(AudioBufferId id, std::uint32_t &output, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        DWORD native = output;
        const auto hr = result(b->GetStatus(&native));
        output = native;
        return hr;
    }
    AudioResult sample_duplicate(AudioDeviceId device, AudioBufferId id, AudioBufferId &output, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        if (device.value != identity() || output.value != 0)
            throw SourceAudioDefaultDeviceError(site,
                                                "source duplicate requires actual device and empty source output");
        // Use the SAME COM retention ledger as source catalog creation. Allocate
        // before the native output; retain even a nonnull failed output. The
        // source receiving slot is known null and is published after this
        // synchronous SDK call, before inspecting HRESULT in the caller.
        auto &node = source_samples.emplace_back();
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        const auto hr = result(source_device->DuplicateSoundBuffer(b, node.buffer.GetAddressOf()));
        output.value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node.buffer.Get()));
        return hr;
    }
    AudioResult sample_query_spatial(AudioBufferId id, AudioSpatialId &output)
    {
        auto *b = checked_playback_sample(id, 0x460efa);
        if (output.value != 0)
            throw SourceAudioDefaultDeviceError(0x460efd, "source spatial query requires an empty local OUT transport");
        // The duplicate already has a node in the one existing native ledger.
        // Select an unqueried reference obligation, even if COM returned an
        // identity already represented by another owned buffer reference.
        const auto found = std::find_if(source_samples.begin(), source_samples.end(), [&](const auto &node) {
            return node.buffer.Get() == b && !node.spatial.Get();
        });
        if (found == source_samples.end())
            throw SourceAudioDefaultDeviceError(0x460efd,
                                                "source spatial query has no retained unqueried duplicate reference");
        // Pinned PE46FE90, SDK IID_IDirectSound3DBuffer. No probe/AddRef or
        // preview query is performed. SDK QI transfers a ref on S_OK only.
        static constexpr GUID iid{
            0x279afa86u, 0x4981u, 0x11ceu, {0xa5u, 0x21u, 0x00u, 0x20u, 0xafu, 0x0bu, 0xe5u, 0x60u}};
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        void *queried = nullptr; // NEW local OUT transport; no source-field store
        const auto hr = result(b->QueryInterface(iid, &queried));
        if (hr == 0 && queried)
            found->spatial.Attach(static_cast<IDirectSound3DBuffer *>(queried));
        // Preserve the actual OUT bits on every returned HRESULT. A failed
        // nonnull OUT does not prove a transferred COM reference; no invented
        // ownership/Release is attached to it. Source460F01 takes its failure arm.
        output.value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(queried));
        return hr;
    }
    AudioResult sample_spatial_mode(AudioSpatialId id, std::uint32_t mode, std::uint32_t apply)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(0x46109e, "source spatial mode requires the retained native owner");
        IDirectSound3DBuffer *spatial = nullptr;
        for (const auto &node : source_samples)
            if (node.spatial.Get() &&
                id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node.spatial.Get())))
            {
                spatial = node.spatial.Get();
                break;
            }
        if (!spatial)
            throw SourceAudioDefaultDeviceError(0x46109e, "source spatial mode identity is null, foreign or released");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(spatial->SetMode(mode, apply)); // 4610A5, actual SDK operation
    }
    // NEW R17 (replay_audio_reset): 461300/461730 positioned spatial children on this
    // owner's SAME retained IDirectSound3DBuffer ledger. SDK operation only; the
    // unchanged source callers ignore the HRESULT. Unknown identities stay errors.
    IDirectSound3DBuffer *checked_sample_spatial(AudioSpatialId id, std::uint32_t site)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(site, "source spatial operation requires the retained native owner");
        for (const auto &node : source_samples)
            if (node.spatial.Get() &&
                id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node.spatial.Get())))
                return node.spatial.Get();
        throw SourceAudioDefaultDeviceError(site, "source spatial identity is null, foreign or released");
    }
    AudioResult sample_spatial_parameters(AudioSpatialId id, const AudioSpatialParameters &value, std::uint32_t apply,
                                          std::uint32_t site)
    {
        auto *spatial = checked_sample_spatial(id, site);
        static_assert(sizeof(DS3DBUFFER) == 64 && sizeof(AudioSpatialParameters) == 64);
        DS3DBUFFER native{};
        std::memcpy(&native, &value, sizeof(native)); // literal 4897B8 DS3DBUFFER image
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(spatial->SetAllParameters(&native, apply));
    }
    AudioResult sample_spatial_position(AudioSpatialId id, Vec3 v, std::uint32_t apply, std::uint32_t site)
    {
        auto *spatial = checked_sample_spatial(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(spatial->SetPosition(v.x, v.y, v.z, apply));
    }
    AudioResult sample_spatial_velocity(AudioSpatialId id, Vec3 v, std::uint32_t apply, std::uint32_t site)
    {
        auto *spatial = checked_sample_spatial(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(spatial->SetVelocity(v.x, v.y, v.z, apply));
    }
    AudioResult sample_frequency(AudioBufferId id, std::uint32_t value, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(b->SetFrequency(value));
    }
    AudioResult sample_volume(AudioBufferId id, std::int32_t value, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(b->SetVolume(value));
    }
    AudioResult sample_pan(AudioBufferId id, std::int32_t value, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(b->SetPan(value));
    }
    AudioResult sample_play(AudioBufferId id, std::uint32_t flags, std::uint32_t site)
    {
        auto *b = checked_playback_sample(id, site);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(b->Play(0, 0, flags));
    }
    std::uint32_t release_sample_sample(AudioBufferId id, std::uint32_t site)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(site, "source release requires retained native owner");
        const auto found = std::find_if(source_samples.begin(), source_samples.end(), [&](const auto &node) {
            return node.buffer.Get() &&
                   id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node.buffer.Get()));
        });
        if (found == source_samples.end())
            throw SourceAudioDefaultDeviceError(site, "source release identity is not retained");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        auto *reference = found->buffer.Detach(); // one ledger obligation consumed, no extra COM Release
        const auto result = reference->Release();
        if (!found->buffer.Get() && !found->spatial.Get())
            source_samples.erase(found);
        return result;
    }
    std::uint32_t release_sample_spatial(AudioSpatialId id)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get() || !source_primary.Get())
            throw SourceAudioDefaultDeviceError(0x460e33, "source release requires retained native owner");
        const auto found = std::find_if(source_samples.begin(), source_samples.end(), [&](const auto &node) {
            return node.spatial.Get() &&
                   id.value == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node.spatial.Get()));
        });
        if (found == source_samples.end())
            throw SourceAudioDefaultDeviceError(0x460e33, "source release identity is not retained");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        auto *reference = found->spatial.Detach(); // one ledger obligation consumed, no extra COM Release
        const auto result = reference->Release();
        if (!found->buffer.Get() && !found->spatial.Get())
            source_samples.erase(found);
        return result;
    }
    IDirectSound3DListener *checked_sample_listener(AudioListenerId id, std::uint32_t site)
    {
        owner();
        if (creating || !source_path || !source_samples_reached || !source_device.Get())
            throw SourceAudioDefaultDeviceError(site, "source listener identity is not retained by this owner");
        if (auto *retained = retained_listener(id))
            return retained;
        throw SourceAudioDefaultDeviceError(site, "source listener identity is not retained by this owner");
    }
    AudioResult sample_listener_position(AudioListenerId id, Vec3 v)
    {
        auto *listener = checked_sample_listener(id, 0x461b00);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(listener->SetPosition(v.x, v.y, v.z, 0));
    }
    AudioResult sample_listener_velocity(AudioListenerId id, Vec3 v)
    {
        auto *listener = checked_sample_listener(id, 0x461b1d);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(listener->SetVelocity(v.x, v.y, v.z, 0));
    }
    AudioResult sample_listener_orientation(AudioListenerId id, Vec3 forward, Vec3 up)
    {
        auto *listener = checked_sample_listener(id, 0x461b5e);
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish()
            {
                active = false;
            }
        } finish{creating};
        return result(listener->SetOrientation(forward.x, forward.y, forward.z, up.x, up.y, up.z, 0));
    }
    struct Creator final : AudioCreationDevice
    {
        Impl &owner;
        SourceSampleReferences *retained{};
        std::optional<bool> direct_spatial;
        bool loading{}, catalog{};
        struct Loading
        {
        };
        struct Catalog
        {
        };
        bool queried{}, copy_phase_reached{}, locked{};
        AudioBufferId locked_buffer;
        void *locked_first{};
        std::uint32_t locked_count{};
        std::uint32_t last_call{};
        std::optional<AudioResult> last_result;
        Creator(Impl &value, bool spatial) : owner(value), direct_spatial(spatial) {}
        Creator(Impl &value, Loading) : owner(value), loading(true) {}
        Creator(Impl &value, Catalog) : owner(value), loading(true), catalog(true) {}
        IDirectSoundBuffer &checked_buffer(AudioBufferId id, std::uint32_t site)
        {
            if (auto *buffer = owner.retained_sample(id))
                return *buffer;
            throw SourceAudioDefaultDeviceError(site, "source sample identity is not a retained buffer in this owner");
        }
        bool buffer_retained() const noexcept
        {
            return retained && retained->buffer.Get() != nullptr;
        }
        bool spatial_retained() const noexcept
        {
            return retained && retained->spatial.Get() != nullptr;
        }
        AudioResult create_buffer(AudioDeviceId device, const AudioBufferRequest &request, AudioBufferId &out) override
        {
            // Label the already selected creator; this does not select a game branch.
            // 461E00 reaches3D only with flag0x10; direct A8 keeps its explicit kind.
            const bool spatial = direct_spatial.value_or((request.flags & 0x10u) != 0);
            if (!owner.source_device.Get() || device.value != owner.identity())
                throw SourceAudioDefaultDeviceError(spatial ? 0x461befu : 0x461ca6u,
                                                    "creator device is not the retained native interface");
            if (request.size != 20 || request.reserved || !request.format)
                throw SourceAudioDefaultDeviceError(spatial ? 0x461bfau : 0x461cc8u,
                                                    "creator descriptor is outside its source contract");
            static_assert(sizeof(DSBUFFERDESC1) == 16 + sizeof(void *));
            static_assert(offsetof(DSBUFFERDESC, lpwfxFormat) == offsetof(DSBUFFERDESC1, lpwfxFormat));
            static_assert(sizeof(DSBUFFERDESC) >= sizeof(DSBUFFERDESC1));
            static_assert(sizeof(WAVEFORMATEX) == 18 && offsetof(WAVEFORMATEX, cbSize) == 16);
            const auto &f = *request.format;
            WAVEFORMATEX format{
                f.tag, f.channels, f.samples_per_second, f.bytes_per_second, f.block_align, f.bits_per_sample, 0};
            DSBUFFERDESC desc{};
            desc.dwSize = static_cast<DWORD>(sizeof(DSBUFFERDESC1));
            desc.dwFlags = request.flags;
            desc.dwBufferBytes = request.bytes;
            desc.dwReserved = request.reserved;
            desc.lpwfxFormat = &format;
            // Native output starts null as NEW transport hygiene. The source
            // temporary's prior stack bits are outside the valid-output ABI.
            if (retained && !catalog)
                throw SourceAudioDefaultDeviceError(0x461e00u, "single sample adapter cannot create twice");
            if (catalog)
            {
                // NEW transport state for the next461E00 call, including after
                // a false return. Prior native references remain in source_samples.
                // No source-field reset, release or compensating Unlock here.
                retained = nullptr;
                queried = false;
                copy_phase_reached = false;
                locked = false;
                locked_buffer = {};
                locked_first = nullptr;
                locked_count = 0;
            }
            retained = &owner.source_samples.emplace_back(); // Before COM; no post-output allocation.
            last_call = spatial ? 0x461bfau : 0x461cc8u;
            last_result.reset();
            const auto hr =
                result(owner.source_device->CreateSoundBuffer(&desc, retained->buffer.GetAddressOf(), nullptr));
            last_result = hr;
            out.value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(retained->buffer.Get()));
            return hr;
        }
        AudioResult query_spatial_buffer(AudioBufferId buffer, AudioSpatialId &out) override
        {
            if (!retained || !retained->buffer.Get() ||
                buffer.value != static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(retained->buffer.Get())))
                throw SourceAudioDefaultDeviceError(0x461c16u, "461C16 dereference requires the created native buffer");
            // Pinned PE46FE90: IID_IDirectSound3DBuffer. QI supplies its own
            // reference; no bridge AddRef, identity query or smart-pointer copy.
            static constexpr GUID iid{
                0x279afa86u, 0x4981u, 0x11ceu, {0xa5u, 0x21u, 0x00u, 0x20u, 0xafu, 0x0bu, 0xe5u, 0x60u}};
            void *output = nullptr;
            struct Retain
            {
                SourceSampleReferences &refs;
                void *&output;
                ~Retain() noexcept
                {
                    if (output)
                        refs.spatial.Attach(static_cast<IDirectSound3DBuffer *>(output));
                }
            } retention{*retained, output};
            queried = true;
            last_call = 0x461c19u;
            last_result.reset();
            const auto hr = result(retained->buffer->QueryInterface(iid, &output));
            last_result = hr;
            out.value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output));
            return hr;
        }
        [[noreturn]] static AudioResult beyond()
        {
            throw SourceAudioDefaultDeviceError(0x461e00u, "native creator cannot enter loading or primary setup");
        }
        AudioResult lock_buffer(AudioBufferId id, std::uint32_t offset, std::uint32_t count, AudioBufferMemory &memory,
                                std::uint32_t flags) override
        {
            if (!loading)
                return beyond();
            auto &buffer = checked_buffer(id, 0x461ed7u);
            if (locked || offset != 0 || flags != 2)
                throw SourceAudioDefaultDeviceError(0x461eecu, "Lock is outside the single source sample call");
            void *first = nullptr;
            void *second = nullptr;
            DWORD first_count = 0, second_count = 0;
            last_call = 0x461eecu;
            last_result.reset();
            const auto hr = result(buffer.Lock(offset, count, &first, &first_count, &second, &second_count, flags));
            last_result = hr;
            if (hr != 0)
                return hr; // Source ignores all returned memory on nonzero HRESULT.
            // NEW bounded memory transport. Never form span(nullptr, nonzero).
            if (!first && first_count)
                throw SourceAudioDefaultDeviceError(0x461f0bu,
                                                    "native Lock returned a null first region with nonzero extent");
            memory.first = {static_cast<std::uint8_t *>(first), first_count};
            memory.second = {}; // Source never uses the second pointer/count; do not validate or join it.
            locked = true;
            locked_buffer = id;
            locked_first = first;
            locked_count = count;
            return hr;
        }
        AudioResult unlock_buffer(AudioBufferId id, std::span<std::uint8_t> first,
                                  std::span<std::uint8_t> second) override
        {
            if (!loading)
                return beyond();
            auto &buffer = checked_buffer(id, 0x461f1eu);
            if (!locked || id != locked_buffer || first.data() != locked_first || first.size() != locked_count ||
                !second.empty())
                throw SourceAudioDefaultDeviceError(
                    0x461f26u, "Unlock requires the pre-Lock identity/count and returned first pointer");
            copy_phase_reached = true;
            last_call = 0x461f26u;
            last_result.reset();
            const auto hr = result(buffer.Unlock(first.data(), locked_count, nullptr, 0));
            last_result = hr;
            return hr;
        }
        AudioResult capabilities(AudioDeviceId, std::span<std::uint32_t, 24>) override
        {
            return beyond();
        }
        AudioResult primary_format(AudioPrimaryId, AudioWaveFormat &, std::uint32_t, std::uint32_t &) override
        {
            return beyond();
        }
        AudioResult set_primary_format(AudioPrimaryId, const AudioWaveFormat &) override
        {
            return beyond();
        }
    };
    SourceAudioSampleCreationResult create_sample_source(AudioDeviceId id, AudioSample &sample,
                                                         const AudioSampleLoadFields &fields, bool spatial,
                                                         std::uint32_t flags)
    {
        owner();
        const std::uint32_t entry = spatial ? 0x461b70u : 0x461c40u;
        if (creating || !source_path || !source_samples_reached)
            throw SourceAudioDefaultDeviceError(entry,
                                                "sample creator requires this owner's actual A6/A7 sample boundary");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        Creator creator{*this, spatial};
        const bool returned = create_audio_sample_buffer(sample, fields, id, creator, spatial, flags);
        return {returned, returned && creator.buffer_retained() && (!spatial || creator.spatial_retained()), entry};
    }
    SourceAudioSampleLoadResult load_sample_source(const AudioDeviceId &id, AudioSample &sample,
                                                   AudioSampleLoadFields &fields, const AssetStore &assets,
                                                   std::string_view resource, const std::uint32_t &enabled,
                                                   std::uint32_t flags)
    {
        owner();
        if (creating || !source_path || !source_samples_reached)
            throw SourceAudioDefaultDeviceError(0x461e00u,
                                                "sample load requires this owner's actual A6/A7 sample boundary");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        Creator adapter{*this, Creator::Loading{}};
        // The sealed Audio dependency owns metadata, late owner reads, creator,
        // pre-Lock locals, payload copy, HRESULT branches and source return.
        const bool returned = load_audio_sample(sample, fields, id, adapter, assets, resource, enabled, flags);
        return {returned,
                adapter.buffer_retained(),
                adapter.queried,
                adapter.spatial_retained(),
                adapter.copy_phase_reached,
                adapter.last_call,
                adapter.last_result};
    }
    SourceAudioCatalogLoadResult load_catalog_source(AudioDriverState &driver, AudioSampleLoadStorage &storage,
                                                     std::array<std::uint32_t, 72> &types, const AudioDeviceId &id,
                                                     const AssetStore &assets, const std::uint32_t &enabled)
    {
        owner();
        if (creating || !source_path || !source_samples_reached)
            throw SourceAudioDefaultDeviceError(0x461f50u,
                                                "catalog load requires this owner's actual A6/A7 sample boundary");
        creating = true;
        struct Finish
        {
            bool &active;
            ~Finish() noexcept
            {
                active = false;
            }
        } finish{creating};
        Creator adapter{*this, Creator::Catalog{}};
        // Pinned461F50: the existing algorithm owns reset, both descriptor loops,
        // late owner reads and ignored single-load returns. No ready publication.
        const bool returned = load_audio_samples(driver, storage, types, id, adapter, assets, enabled);
        return {returned, adapter.last_call, adapter.last_result};
    }
    std::uint32_t release_source(AudioDeviceId id)
    {
        owner();
        if (creating || source_device.Get() == nullptr || id.value != identity())
            throw SourceAudioDefaultDeviceError(0x4621eau,
                                                "4621EA release requires the actual retained source interface token");
        // 4621EA drops precisely the device reference after45FAB0 returns.
        // Keep primary/listener/sample ComPtrs and their source globals intact;
        // the source body does not release those references at this call.
        // Detach before Release prevents host teardown from releasing it twice.
        // NEW HOST (R35b): reserve the destroyed-children record first; a bad_alloc leaves the device retained.
        source_destroyed_children.reserve(source_destroyed_children.size() + source_properties.size() +
                                          2 * source_samples.size() + source_displaced_listeners.size() +
                                          source_displaced_primaries.size() + 2);
        auto *native = source_device.Detach();
        source_creation_succeeded = false;
        source_cooperative_completed = false;
        source_primary_creation_succeeded = false;
        source_primary_format_succeeded = false;
        source_listener_query_succeeded = false;
        source_listener_slot = nullptr;
        // Drop only this owner's reference. The source caller separately writes
        // 62E0B0=0 at4621ED; this API must not invent that source write.
        // NEW HOST (R35b): the returned count and every retained slot are unchanged. When this was the last device
        // reference, DirectSound has destroyed all of its buffers, so the host only records their identities for
        // teardown (capacity was reserved before the Detach above, so recording cannot throw after the Release).
        const auto remaining = native->Release();
        if (remaining == 0)
            remember_destroyed_children();
        return remaining;
    }
};
AudioResult WindowsAudioDevice::query_audio_property(AudioSpatialId, const AudioPropertyGuid &, AudioPropertyId &output)
{
    (void)impl_->modern();
    // XAudio2 source voices expose no IKsPropertySet. This is the actual native
    // capability, not a successful empty EAX effect. Source460AF0 handles failure.
    output = {};
    return static_cast<AudioResult>(E_NOINTERFACE);
}
AudioResult WindowsAudioDevice::audio_property_support(AudioPropertyId, const AudioPropertyGuid &, std::uint32_t,
                                                       std::uint32_t &)
{
    (void)impl_->modern();
    // No property identity can be acquired from this backend.
    return static_cast<AudioResult>(E_NOINTERFACE);
}
AudioResult WindowsAudioDevice::set_audio_property(AudioPropertyId, const AudioPropertyGuid &, std::uint32_t,
                                                   std::span<const std::uint8_t>, std::span<const std::uint8_t>)
{
    (void)impl_->modern();
    return static_cast<AudioResult>(E_NOINTERFACE);
}
WindowsAudioDevice::WindowsAudioDevice(DeferredAudioConstructionOnly, float master_gain)
    : WindowsAudioDevice(DeferredAudioConstructionOnly{}, master_gain, SourceAudioDefaultDeviceNativeApi{})
{
}
WindowsAudioDevice::WindowsAudioDevice(DeferredAudioConstructionOnly, float master_gain,
                                       const SourceAudioDefaultDeviceNativeApi &api)
    : impl_(std::make_unique<Impl>(master_gain, api))
{
}
WindowsAudioDevice::WindowsAudioDevice(float master_gain)
    : WindowsAudioDevice(DeferredAudioConstructionOnly{}, master_gain)
{
    prepare_host_backend();
}
void WindowsAudioDevice::prepare_host_backend()
{
    impl_->prepare();
}
bool WindowsAudioDevice::host_backend_ready() const
{
    impl_->owner();
    return bool(impl_->modern_backend);
}
bool WindowsAudioDevice::source_sample_stop_admitted(AudioBufferId id) const
{
    return impl_->sample_stop_admitted(id);
}
AudioResult WindowsAudioDevice::source_query_audio_property(AudioSpatialId spatial, const AudioPropertyGuid &iid,
                                                            AudioPropertyId &output)
{
    return impl_->query_source_property(spatial, iid, output);
}
AudioResult WindowsAudioDevice::source_audio_property_support(AudioPropertyId property, const AudioPropertyGuid &set,
                                                              std::uint32_t identifier, std::uint32_t &support)
{
    return impl_->support_source_property(property, set, identifier, support);
}
AudioResult WindowsAudioDevice::source_set_audio_property(AudioPropertyId property, const AudioPropertyGuid &set,
                                                          std::uint32_t identifier,
                                                          std::span<const std::uint8_t> instance,
                                                          std::span<const std::uint8_t> data)
{
    return impl_->set_source_property(property, set, identifier, instance, data);
}
std::uint32_t WindowsAudioDevice::source_sample_release(AudioBufferId id, std::uint32_t site)
{
    return impl_->release_sample_sample(id, site);
}
std::uint32_t WindowsAudioDevice::source_sample_release_spatial(AudioSpatialId id)
{
    return impl_->release_sample_spatial(id);
}
AudioResult WindowsAudioDevice::source_sample_listener_position(AudioListenerId id, Vec3 v)
{
    return impl_->sample_listener_position(id, v);
}
AudioResult WindowsAudioDevice::source_sample_listener_velocity(AudioListenerId id, Vec3 v)
{
    return impl_->sample_listener_velocity(id, v);
}
AudioResult WindowsAudioDevice::source_sample_listener_orientation(AudioListenerId id, Vec3 f, Vec3 u)
{
    return impl_->sample_listener_orientation(id, f, u);
}

AudioResult WindowsAudioDevice::source_sample_status(AudioBufferId id, std::uint32_t &output, std::uint32_t site)
{
    return impl_->sample_status(id, output, site);
}
AudioResult WindowsAudioDevice::source_sample_duplicate(AudioDeviceId device, AudioBufferId id, AudioBufferId &output,
                                                        std::uint32_t site)
{
    return impl_->sample_duplicate(device, id, output, site);
}
AudioResult WindowsAudioDevice::source_sample_query_spatial(AudioBufferId id, AudioSpatialId &output)
{
    return impl_->sample_query_spatial(id, output);
}
AudioResult WindowsAudioDevice::source_sample_spatial_mode(AudioSpatialId id, std::uint32_t mode, std::uint32_t apply)
{
    return impl_->sample_spatial_mode(id, mode, apply);
}
AudioResult WindowsAudioDevice::source_sample_frequency(AudioBufferId id, std::uint32_t value, std::uint32_t site)
{
    return impl_->sample_frequency(id, value, site);
}
AudioResult WindowsAudioDevice::source_sample_volume(AudioBufferId id, std::int32_t value, std::uint32_t site)
{
    return impl_->sample_volume(id, value, site);
}
AudioResult WindowsAudioDevice::source_sample_pan(AudioBufferId id, std::int32_t value, std::uint32_t site)
{
    return impl_->sample_pan(id, value, site);
}
AudioResult WindowsAudioDevice::source_sample_play(AudioBufferId id, std::uint32_t value, std::uint32_t site)
{
    return impl_->sample_play(id, value, site);
}
AudioResult WindowsAudioDevice::source_sample_spatial_parameters(AudioSpatialId id, const AudioSpatialParameters &value,
                                                                 std::uint32_t apply, std::uint32_t site)
{
    return impl_->sample_spatial_parameters(id, value, apply, site);
}
AudioResult WindowsAudioDevice::source_sample_spatial_position(AudioSpatialId id, Vec3 v, std::uint32_t apply,
                                                               std::uint32_t site)
{
    return impl_->sample_spatial_position(id, v, apply, site);
}
AudioResult WindowsAudioDevice::source_sample_spatial_velocity(AudioSpatialId id, Vec3 v, std::uint32_t apply,
                                                               std::uint32_t site)
{
    return impl_->sample_spatial_velocity(id, v, apply, site);
}

std::uint64_t WindowsAudioDevice::source_device_identity() const
{
    impl_->owner();
    return impl_->identity();
}
AudioResult WindowsAudioDevice::create_source_default_device(AudioDeviceId &out)
{
    return impl_->create_source(out);
}
AudioResult WindowsAudioDevice::set_source_cooperative_level(AudioDeviceId device, std::uintptr_t window,
                                                             std::uint32_t level)
{
    return impl_->source_cooperative(device, window, level);
}
AudioResult WindowsAudioDevice::create_source_primary_buffer(AudioDeviceId id, const AudioBufferRequest &request,
                                                             AudioPrimaryId &out)
{
    return impl_->create_primary(id, request, out);
}
std::uint64_t WindowsAudioDevice::source_primary_identity() const
{
    impl_->owner();
    return impl_->primary_identity();
}
SourceAudioPrimaryFormatResult WindowsAudioDevice::complete_source_primary_format(AudioDeviceId device,
                                                                                  AudioPrimaryId &primary,
                                                                                  const std::uint32_t &rate,
                                                                                  std::uint32_t &capability,
                                                                                  std::uint32_t &enabled)
{
    return impl_->complete_primary_format(device, primary, rate, capability, enabled);
}
WindowsAudioDevice::~WindowsAudioDevice() = default;
SourceAudioListenerAcquisitionOutcome WindowsAudioDevice::acquire_source_listener_after_primary_format(
    AudioPrimaryId &primary, AudioListenerId &listener, const std::uint32_t &mode, std::uint32_t &enabled)
{
    return impl_->acquire_listener(primary, listener, mode, enabled);
}
std::uint64_t WindowsAudioDevice::source_listener_identity() const
{
    impl_->owner();
    return impl_->listener_identity();
}
SourceAudioSamplesBoundary WindowsAudioDevice::configure_source_listener_after_query(const AudioListenerId &listener)
{
    return impl_->configure_listener(listener);
}
SourceAudioSampleCreationResult WindowsAudioDevice::create_source_sample_buffer(AudioDeviceId device,
                                                                                AudioSample &sample,
                                                                                const AudioSampleLoadFields &fields,
                                                                                bool spatial, std::uint32_t flags)
{
    return impl_->create_sample_source(device, sample, fields, spatial, flags);
}
SourceAudioSampleLoadResult WindowsAudioDevice::load_source_sample(const AudioDeviceId &device, AudioSample &sample,
                                                                   AudioSampleLoadFields &fields,
                                                                   const AssetStore &assets, std::string_view resource,
                                                                   const std::uint32_t &enabled, std::uint32_t flags)
{
    return impl_->load_sample_source(device, sample, fields, assets, resource, enabled, flags);
}
SourceAudioCatalogLoadResult WindowsAudioDevice::load_source_catalog(
    AudioDriverState &driver, AudioSampleLoadStorage &storage, std::array<std::uint32_t, 72> &types,
    const AudioDeviceId &device, const AssetStore &assets, const std::uint32_t &enabled)
{
    return impl_->load_catalog_source(driver, storage, types, device, assets, enabled);
}
AudioListenerId WindowsAudioDevice::listener() const
{
    (void)impl_->modern();
    return {1};
}
AudioDeviceInfo WindowsAudioDevice::info() const
{
    auto &backend = impl_->modern();
    return backend.info;
}
std::size_t WindowsAudioDevice::live_buffers() const
{
    auto &backend = impl_->modern();
    return backend.voices.size();
}
AudioResult WindowsAudioDevice::health() const
{
    auto &backend = impl_->modern();
    return result(backend.critical.load());
}
AudioVoiceInfo WindowsAudioDevice::inspect(AudioBufferId id) const
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v)
        throw AudioError(result(E_INVALIDARG), "unknown audio buffer");
    XAUDIO2_VOICE_STATE state{};
    v->output->GetState(&state);
    float ratio{};
    v->output->GetFrequencyRatio(&ratio);
    return {state.SamplesPlayed,
            state.BuffersQueued,
            v->ordinary_refs + v->spatial_refs,
            v->started,
            ratio,
            v->matrix,
            bool(v->ring),
            v->ring ? v->ring->submitted() : 0,
            v->ring ? v->ring->completed() : 0,
            v->ring ? v->ring->publications() : 0};
}
AudioSample WindowsAudioDevice::create_sample(PcmWave wave, bool spatial)
{
    auto &backend = impl_->modern();
    const auto rate = wave.sample_rate;
    const auto id = backend.create(std::make_shared<const PcmWave>(std::move(wave)), spatial);
    AudioSpatialId spatial_id;
    if (spatial)
    {
        const auto hr = query_spatial(id, spatial_id);
        if (hr != 0)
        {
            release(id);
            throw AudioError(hr, "query new spatial source");
        }
    }
    return {id, spatial_id, spatial ? 2u : 1u, rate};
}
void WindowsAudioDevice::load_catalog(const AssetStore &assets, AudioDriverState &state, bool spatial)
{
    (void)impl_->modern();
    if (std::any_of(state.samples.begin(), state.samples.end(),
                    [](const auto &s) { return s.buffer.value || s.spatial.value; }))
        throw std::logic_error("audio catalog requires empty sample slots");
    auto pending = state.samples;
    try
    {
        for (const unsigned type : {2u, 1u})
            for (const auto &definition : audio_sample_definitions)
                if (definition.type == type)
                    pending[definition.slot] =
                        create_sample(decode_pcm_wave(assets.resource(definition.resource)), spatial && type == 2);
    }
    catch (...)
    {
        for (auto &sample : pending)
        {
            if (sample.spatial.value)
                release_spatial(sample.spatial);
            if (sample.buffer.value)
                release(sample.buffer);
        }
        throw;
    }
    state.samples = pending;
    state.spatial_mode = spatial ? 1u : 0u;
    state.initialized = 1;
    state.spatial_template = audio_default_spatial_parameters();
}
AudioResult WindowsAudioDevice::status(AudioBufferId id, std::uint32_t &flags)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    XAUDIO2_VOICE_STATE state{};
    v->output->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    flags = v->started && state.BuffersQueued ? (1u | (v->loop ? 4u : 0u)) : 0u;
    return 0;
}
AudioResult WindowsAudioDevice::duplicate(AudioBufferId id, AudioBufferId &output)
{
    auto &backend = impl_->modern();
    auto *original = backend.find(id.value);
    const auto hr = backend.health(original);
    if (FAILED(hr))
        return result(hr);
    if (!original->sealed)
        return result(E_PENDING);
    if (original->ring)
        return result(E_NOTIMPL);
    try
    {
        const auto copy = backend.create(original->wave, original->spatial_capable);
        auto *v = backend.find(copy.value);
        v->frequency = original->frequency;
        v->volume = original->volume;
        v->pan = original->pan;
        v->spatial = original->spatial;
        const auto mixed = backend.mix(*v);
        const auto level = SUCCEEDED(mixed) ? v->output->SetVolume(gain(v->volume)) : mixed;
        if (FAILED(level))
        {
            release(copy);
            return result(level);
        }
        output = copy;
        return 0;
    }
    catch (const AudioError &e)
    {
        return e.result;
    }
    catch (const std::bad_alloc &)
    {
        return result(E_OUTOFMEMORY);
    }
}
AudioResult WindowsAudioDevice::query_spatial(AudioBufferId id, AudioSpatialId &output)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (!v->spatial_capable)
        return result(E_NOINTERFACE);
    if (v->spatial_refs == std::numeric_limits<std::uint32_t>::max())
        return result(E_OUTOFMEMORY);
    ++v->spatial_refs;
    output = {id.value};
    return 0;
}
std::uint32_t WindowsAudioDevice::release(AudioBufferId id)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v || !v->ordinary_refs)
        return 0;
    --v->ordinary_refs;
    const auto remaining = v->ordinary_refs + v->spatial_refs;
    if (!remaining)
    {
        backend.voices.erase(id.value);
        backend.pending.erase(id.value);
    }
    return remaining;
}
std::uint32_t WindowsAudioDevice::release_spatial(AudioSpatialId id)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v || !v->spatial_refs)
        return 0;
    --v->spatial_refs;
    const auto remaining = v->ordinary_refs + v->spatial_refs;
    if (!remaining)
    {
        backend.voices.erase(id.value);
        backend.pending.erase(id.value);
    }
    return remaining;
}
AudioResult WindowsAudioDevice::frequency(AudioBufferId id, std::uint32_t hz)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (hz != 0 && (hz < 100 || hz > 200000))
        return result(E_INVALIDARG);
    const auto previous = v->frequency;
    v->frequency = hz ? hz : v->wave->sample_rate;
    const auto applied = backend.mix(*v);
    if (FAILED(applied))
        v->frequency = previous;
    return result(applied);
}
AudioResult WindowsAudioDevice::volume(AudioBufferId id, std::int32_t db)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (db < -10000 || db > 0)
        return result(E_INVALIDARG);
    const auto applied = v->output->SetVolume(gain(db));
    if (SUCCEEDED(applied))
        v->volume = db;
    return result(applied);
}
AudioResult WindowsAudioDevice::pan(AudioBufferId id, std::int32_t db)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (db < -10000 || db > 10000 || v->spatial_capable)
        return result(E_INVALIDARG);
    const auto previous = v->pan;
    v->pan = db;
    const auto applied = backend.mix(*v);
    if (FAILED(applied))
        v->pan = previous;
    return result(applied);
}
AudioResult WindowsAudioDevice::play(AudioBufferId id, std::uint32_t reserved1, std::uint32_t reserved2,
                                     std::uint32_t flags)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (reserved1 || reserved2 || flags > 1)
        return result(E_INVALIDARG);
    if (!v->sealed)
        return result(E_PENDING);
    if (v->ring)
    {
        if (flags != 1)
            return result(E_NOTIMPL);
        const auto prepared = v->ring->prepare();
        if (FAILED(prepared))
            return result(prepared);
        const auto started = v->output->Start();
        if (SUCCEEDED(started))
        {
            v->started = true;
            v->loop = 1;
        }
        return result(started);
    }
    XAUDIO2_VOICE_STATE state{};
    v->output->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (state.BuffersQueued && v->loop != flags)
        return result(E_NOTIMPL);
    if (!state.BuffersQueued)
    {
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(v->wave->samples.size());
        buffer.pAudioData = v->wave->samples.data();
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopCount = flags ? XAUDIO2_LOOP_INFINITE : 0;
        const auto submitted = v->output->SubmitSourceBuffer(&buffer);
        if (FAILED(submitted))
            return result(submitted);
        v->loop = flags;
    }
    const auto started = v->output->Start();
    if (SUCCEEDED(started))
        v->started = true;
    return result(started);
}
AudioResult WindowsAudioDevice::stop(AudioBufferId id)
{
    impl_->owner();
    if (impl_->source_path)
        return impl_->stop_source_sample(id);
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    const auto stopped = v->output->Stop();
    if (SUCCEEDED(stopped))
        v->started = false;
    return result(stopped);
}
AudioResult WindowsAudioDevice::spatial_parameters(AudioSpatialId id, const AudioSpatialParameters &value,
                                                   std::uint32_t apply)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    const auto hr = backend.health(v);
    if (FAILED(hr))
        return result(hr);
    if (!v->spatial_refs || apply || value.size != 64 || value.mode > 2 || !finite(value.position) ||
        !finite(value.velocity) || !finite(value.cone_orientation) || !std::isfinite(value.minimum_distance) ||
        !std::isfinite(value.maximum_distance) || value.minimum_distance <= 0 ||
        value.maximum_distance < value.minimum_distance || value.inside_cone_angle > value.outside_cone_angle ||
        value.outside_cone_angle > 360 || value.outside_cone_volume < -10000 || value.outside_cone_volume > 0)
        return result(E_INVALIDARG);
    const auto previous = v->spatial;
    v->spatial = value;
    const auto applied = backend.mix(*v);
    if (FAILED(applied))
        v->spatial = previous;
    return result(applied);
}
AudioResult WindowsAudioDevice::spatial_mode(AudioSpatialId id, std::uint32_t mode, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v)
        return result(E_INVALIDARG);
    auto value = v->spatial;
    value.mode = mode;
    return spatial_parameters(id, value, apply);
}
AudioResult WindowsAudioDevice::spatial_position(AudioSpatialId id, Vec3 position, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v)
        return result(E_INVALIDARG);
    auto value = v->spatial;
    value.position = position;
    return spatial_parameters(id, value, apply);
}
AudioResult WindowsAudioDevice::spatial_velocity(AudioSpatialId id, Vec3 velocity, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    auto *v = backend.find(id.value);
    if (!v)
        return result(E_INVALIDARG);
    auto value = v->spatial;
    value.velocity = velocity;
    return spatial_parameters(id, value, apply);
}
AudioResult WindowsAudioDevice::listener_position(AudioListenerId id, Vec3 position, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    if (id.value != 1 || apply || !finite(position))
        return result(E_INVALIDARG);
    backend.listener.Position = vector(position);
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::listener_velocity(AudioListenerId id, Vec3 velocity, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    if (id.value != 1 || apply || !finite(velocity))
        return result(E_INVALIDARG);
    backend.listener.Velocity = vector(velocity);
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::listener_orientation(AudioListenerId id, Vec3 forward, Vec3 up, std::uint32_t apply)
{
    auto &backend = impl_->modern();
    Vec3 front, top;
    if (id.value != 1 || apply || !unit(forward, front) || !unit(up, top))
        return result(E_INVALIDARG);
    const double alignment = double(front.x) * top.x + double(front.y) * top.y + double(front.z) * top.z;
    const Vec3 orthogonal{float(top.x - alignment * front.x), float(top.y - alignment * front.y),
                          float(top.z - alignment * front.z)};
    if (!unit(orthogonal, top))
        return result(E_INVALIDARG);
    backend.listener.OrientFront = vector(front);
    backend.listener.OrientTop = vector(top);
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::enumerate(AudioEnumerationReceiver &receiver)
{
    impl_->owner();
    if (impl_->creating)
        throw SourceAudioDefaultDeviceError(0x46210eu, "enumeration cannot reenter an active source operation");
    Win32SourceAudioEnumeration native;
    return native.enumerate(receiver); // Actual ANSI inventory, callback stop, and HRESULT.
}
AudioResult WindowsAudioDevice::create_default_device(AudioDeviceId &out)
{
    auto &backend = impl_->modern();
    if (health() != 0)
        return health();
    if (backend.device_generation == 0x7fffffffffffffffull)
        return result(E_OUTOFMEMORY);
    backend.device_open = true;
    out.value = ++backend.device_generation;
    const auto channels = static_cast<std::uint16_t>(backend.info.channels);
    backend.primary = {1,
                       channels,
                       backend.info.sample_rate,
                       backend.info.sample_rate * channels * 2,
                       static_cast<std::uint16_t>(channels * 2),
                       16,
                       0};
    return 0;
}
AudioResult WindowsAudioDevice::cooperative_level(AudioDeviceId device, std::uint64_t window, unsigned level)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || device.value != backend.device_generation || level != 3 ||
        !IsWindow(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(window))))
        return result(E_INVALIDARG);
    // XAudio2 owns a shared output and performs resampling without changing the
    // desktop endpoint format. Its existing mastering voice supplies priority.
    return health();
}
unsigned WindowsAudioDevice::release_device(AudioDeviceId device)
{
    impl_->owner();
    if (impl_->source_path)
        return impl_->release_source(device);
    auto &backend = impl_->modern();
    if (!backend.device_open || device.value != backend.device_generation)
        return 0;
    // A platform generation owns its voices, including failed source loads.
    // Destroying a generation joins voice callbacks before PCM is released.
    backend.voices.clear();
    backend.pending.clear();
    backend.device_open = false;
    return 0;
}
template <class Output>
AudioResult WindowsAudioDevice::create_buffer_value(AudioDeviceId device, const AudioBufferRequest &request,
                                                    Output &out)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || device.value != backend.device_generation || request.size != 20 || request.reserved)
        return result(E_INVALIDARG);
    if (!request.format)
    {
        if (request.flags != 0x11 || request.bytes)
            return result(E_INVALIDARG);
        out.value = 0x8000000000000000ull | device.value;
        return health();
    }
    const auto limit = request.flags == 0xe8 ? 64u * 1024u * 1024u : 4u * 1024u * 1024u;
    const auto &format = *request.format;
    if (format.tag != 1 || !request.bytes || request.bytes > limit ||
        format.bytes_per_second != format.samples_per_second * format.block_align)
        return result(E_INVALIDARG);
    try
    {
        auto wave =
            std::make_shared<PcmWave>(PcmWave{format.samples_per_second, format.channels, format.bits_per_sample,
                                              format.block_align, Bytes(request.bytes)});
        const auto id = backend.create(wave, (request.flags & 0x10) != 0);
        backend.find(id.value)->sealed = false;
        try
        {
            if (request.flags == 0xe8)
                backend.find(id.value)->ring = std::make_unique<sgn::detail::AudioRingQueue>(
                    *backend.find(id.value)->output, request.bytes, format.block_align, format.samples_per_second);
            backend.pending.emplace(id.value, Impl::Modern::PendingPcm{std::move(wave), false, 0, 0});
        }
        catch (...)
        {
            backend.voices.erase(id.value);
            throw;
        }
        out.value = id.value;
        return 0;
    }
    catch (const AudioError &e)
    {
        return e.result;
    }
    catch (const std::bad_alloc &)
    {
        return result(E_OUTOFMEMORY);
    }
    catch (const std::system_error &e)
    {
        return result(HRESULT_FROM_WIN32(static_cast<DWORD>(e.code().value())));
    }
}
AudioResult WindowsAudioDevice::create_buffer(AudioDeviceId device, const AudioBufferRequest &request,
                                              AudioBufferId &out)
{
    return create_buffer_value(device, request, out);
}
AudioResult WindowsAudioDevice::create_music_buffer(AudioDeviceId device, const AudioBufferRequest &request,
                                                    AudioMusicBuffer &out)
{
    return create_buffer_value(device, request, out);
}
AudioResult WindowsAudioDevice::query_spatial_buffer(AudioBufferId buffer, AudioSpatialId &out)
{
    return query_spatial(buffer, out);
}
AudioResult WindowsAudioDevice::lock_buffer(AudioBufferId buffer, unsigned offset, unsigned count,
                                            AudioBufferMemory &memory, unsigned flags)
{
    auto &backend = impl_->modern();
    const auto found = backend.pending.find(buffer.value);
    auto *voice = backend.find(buffer.value);
    if (found == backend.pending.end() || !voice || found->second.locked)
        return result(E_INVALIDARG);
    auto &pending = found->second;
    const auto length = static_cast<unsigned>(pending.wave->samples.size());
    if (voice->ring)
    {
        if (flags || offset >= length || !count || count > length)
            return result(E_INVALIDARG);
    }
    else if (offset || flags != 2 || count != length)
        return result(E_INVALIDARG);
    const auto checked = backend.health(voice);
    if (FAILED(checked))
        return result(checked);
    const auto first = std::min(count, length - offset);
    pending.locked = true;
    pending.offset = offset;
    pending.count = count;
    memory.first = {pending.wave->samples.data() + offset, first};
    memory.second = count > first ? std::span<std::uint8_t>{pending.wave->samples.data(), count - first}
                                  : std::span<std::uint8_t>{};
    return 0;
}
AudioResult WindowsAudioDevice::unlock_buffer(AudioBufferId buffer, std::span<std::uint8_t> first,
                                              std::span<std::uint8_t> second)
{
    auto &backend = impl_->modern();
    const auto found = backend.pending.find(buffer.value);
    auto *voice = backend.find(buffer.value);
    if (found == backend.pending.end() || !voice || !found->second.locked)
        return result(E_INVALIDARG);
    auto &pending = found->second;
    const auto first_size = std::min<std::size_t>(pending.count, pending.wave->samples.size() - pending.offset);
    const auto second_size = pending.count - first_size;
    if (first.data() != pending.wave->samples.data() + pending.offset || first.size() != first_size ||
        second.size() != second_size || (second_size && second.data() != pending.wave->samples.data()))
        return result(E_INVALIDARG);
    const auto hr = backend.health(voice);
    if (FAILED(hr))
        return result(hr);
    if (voice->ring)
    {
        try
        {
            voice->ring->publish(pending.wave->samples);
        }
        catch (const std::bad_alloc &)
        {
            return result(E_OUTOFMEMORY);
        }
        pending.locked = false;
        voice->sealed = true;
    }
    else
    {
        voice->sealed = true;
        backend.pending.erase(found);
    }
    return 0;
}
AudioResult WindowsAudioDevice::capabilities(AudioDeviceId device, std::span<unsigned, 24> caps)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || device.value != backend.device_generation || caps[0] != 96)
        return result(E_INVALIDARG);
    std::fill(caps.begin(), caps.end(), 0);
    caps[0] = 96;
    caps[1] = 8;
    caps[2] = 1000;
    caps[3] = 200000;
    caps[11] = 128;
    return health();
}
AudioResult WindowsAudioDevice::primary_format(AudioPrimaryId id, AudioWaveFormat &format, unsigned count,
                                               unsigned &written)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || id.value != (0x8000000000000000ull | backend.device_generation) || count != 18)
        return result(E_INVALIDARG);
    format = backend.primary;
    written = 18;
    return health();
}
AudioResult WindowsAudioDevice::set_primary_format(AudioPrimaryId id, const AudioWaveFormat &format)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || id.value != (0x8000000000000000ull | backend.device_generation) || format.tag != 1 ||
        format.channels < 1 || format.channels > 8 || (format.bits_per_sample != 8 && format.bits_per_sample != 16) ||
        format.samples_per_second < 1000 || format.samples_per_second > 200000 ||
        format.block_align != format.channels * (format.bits_per_sample / 8) ||
        format.bytes_per_second != format.samples_per_second * format.block_align)
        return result(E_INVALIDARG);
    backend.primary = format;
    return health();
}
AudioResult WindowsAudioDevice::query_listener(AudioPrimaryId id, AudioListenerId &out)
{
    auto &backend = impl_->modern();
    if (!backend.device_open || id.value != (0x8000000000000000ull | backend.device_generation))
        return result(E_INVALIDARG);
    out = listener();
    return health();
}
AudioResult WindowsAudioDevice::listener_doppler(AudioListenerId id, float factor, unsigned apply)
{
    auto &backend = impl_->modern();
    if (id.value != 1 || apply || !std::isfinite(factor) || factor < 0 || factor > 10)
        return result(E_INVALIDARG);
    backend.doppler_factor = factor;
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::listener_rolloff(AudioListenerId id, float factor, unsigned apply)
{
    auto &backend = impl_->modern();
    if (id.value != 1 || apply || !std::isfinite(factor) || factor < 0 || factor > 10)
        return result(E_INVALIDARG);
    backend.rolloff_factor = factor;
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::listener_distance(AudioListenerId id, float factor, unsigned apply)
{
    auto &backend = impl_->modern();
    if (id.value != 1 || apply || !std::isfinite(factor) || factor <= 0 || factor > 1000000)
        return result(E_INVALIDARG);
    backend.distance_factor = factor;
    return result(backend.refresh_spatial());
}
AudioResult WindowsAudioDevice::set_play_cursor(AudioBufferId id, unsigned offset)
{
    auto &backend = impl_->modern();
    auto *voice = backend.find(id.value);
    const auto hr = backend.health(voice);
    if (FAILED(hr))
        return result(hr);
    if (offset || voice->started)
        return result(E_NOTIMPL);
    try
    {
        return result(backend.rewind(id.value));
    }
    catch (const AudioError &e)
    {
        return e.result;
    }
    catch (const std::bad_alloc &)
    {
        return result(E_OUTOFMEMORY);
    }
}
AudioResult WindowsAudioDevice::play_cursors(AudioBufferId id, unsigned &play, unsigned &write)
{
    auto &backend = impl_->modern();
    auto *voice = backend.find(id.value);
    const auto hr = backend.health(voice);
    if (FAILED(hr))
        return result(hr);
    if (!voice->ring)
        return result(E_NOTIMPL);
    XAUDIO2_VOICE_STATE state{};
    voice->output->GetState(&state);
    play = static_cast<unsigned>((state.SamplesPlayed * voice->wave->block_align) % voice->wave->samples.size());
    // The recovered callback fetches but never consumes the write cursor.
    write = play;
    return 0;
}
} // namespace sgn
