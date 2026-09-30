#pragma once
#include "sgn/assets.hpp"
#include <windows.h>
#include <xaudio2.h>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace sgn::detail {
// New XAudio2 transport for a recovered DirectSound ring. The source thread
// publishes immutable ring snapshots. A separate queue worker consumes them;
// it never accesses Session, source file cursors or gameplay state. XAudio2
// callbacks only signal completion and never read files, allocate or wait.
class AudioRingQueue {
    struct Packet {Bytes bytes;std::atomic<bool> complete{};};
    IXAudio2SourceVoice& voice_;
    std::uint32_t size_{},chunk_{},next_{};
    HANDLE wake_{};
    std::atomic<std::shared_ptr<const Bytes>> published_;
    std::vector<std::unique_ptr<Packet>> packets_;
    std::thread worker_;
    std::atomic<bool> stopping_{};
    std::atomic<HRESULT> failure_{S_OK};
    std::atomic<std::uint64_t> submitted_{},completed_{},publications_{};
    HRESULT fill();
    void run() noexcept;
public:
    AudioRingQueue(IXAudio2SourceVoice&,std::uint32_t bytes,std::uint32_t align,std::uint32_t rate);
    ~AudioRingQueue(); // caller joins voice callbacks before destroying this object
    void publish(std::span<const std::uint8_t>);
    HRESULT prepare() noexcept;
    void shutdown() noexcept;
    void on_buffer_end(void*) noexcept;
    HRESULT health() const noexcept{return failure_.load();}
    std::uint64_t submitted() const noexcept{return submitted_.load();}
    std::uint64_t completed() const noexcept{return completed_.load();}
    std::uint64_t publications() const noexcept{return publications_.load();}
};
}
