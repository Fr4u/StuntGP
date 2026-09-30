#include "audio_ring_queue.hpp"
#include <algorithm>
#include <cstring>
#include <system_error>

namespace sgn::detail
{
AudioRingQueue::AudioRingQueue(IXAudio2SourceVoice &voice, std::uint32_t bytes, std::uint32_t align, std::uint32_t rate)
    : voice_(voice), size_(bytes)
{
    if (!align || !bytes || bytes % align || !rate)
        throw std::invalid_argument("unaligned audio ring");
    chunk_ = std::min(bytes, std::max(1u, rate / 50u) * align);
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!wake_)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "audio queue event");
}
AudioRingQueue::~AudioRingQueue()
{
    shutdown();
    if (wake_)
        CloseHandle(wake_);
}
void AudioRingQueue::publish(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != size_)
        throw std::invalid_argument("audio ring publication size changed");
    published_.store(std::make_shared<const Bytes>(bytes.begin(), bytes.end()), std::memory_order_release);
    ++publications_;
}
HRESULT AudioRingQueue::fill()
{
    std::erase_if(packets_, [](const auto &packet) { return packet->complete.load(std::memory_order_acquire); });
    XAUDIO2_VOICE_STATE state{};
    voice_.GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    while (state.BuffersQueued < 3)
    {
        const auto ring = published_.load(std::memory_order_acquire);
        if (!ring)
            return E_PENDING;
        auto packet = std::make_unique<Packet>();
        packet->bytes.resize(chunk_);
        const auto first = std::min(chunk_, size_ - next_);
        std::memcpy(packet->bytes.data(), ring->data() + next_, first);
        if (first < chunk_)
            std::memcpy(packet->bytes.data() + first, ring->data(), chunk_ - first);
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = chunk_;
        buffer.pAudioData = packet->bytes.data();
        buffer.pContext = packet.get();
        // Keep the context alive even if a zero-duration voice consumes the
        // packet immediately after submission. Ownership precedes the API call.
        packets_.push_back(std::move(packet));
        const HRESULT hr = voice_.SubmitSourceBuffer(&buffer);
        if (FAILED(hr))
        {
            packets_.pop_back();
            return hr;
        }
        submitted_ += chunk_;
        next_ = (next_ + chunk_) % size_;
        ++state.BuffersQueued;
    }
    return S_OK;
}
HRESULT AudioRingQueue::prepare() noexcept
{
    if (worker_.joinable())
        return health();
    try
    {
        const auto hr = fill();
        if (FAILED(hr))
            return hr;
        stopping_ = false;
        worker_ = std::thread([this] { run(); });
        return S_OK;
    }
    catch (const std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_FAIL;
    }
}
void AudioRingQueue::run() noexcept
{
    while (!stopping_.load(std::memory_order_acquire))
    {
        if (WaitForSingleObject(wake_, INFINITE) != WAIT_OBJECT_0)
        {
            failure_ = E_FAIL;
            return;
        }
        if (stopping_.load(std::memory_order_acquire))
            break;
        try
        {
            const auto hr = fill();
            if (FAILED(hr))
            {
                failure_ = hr;
                return;
            }
        }
        catch (const std::bad_alloc &)
        {
            failure_ = E_OUTOFMEMORY;
            return;
        }
        catch (...)
        {
            failure_ = E_FAIL;
            return;
        }
    }
}
void AudioRingQueue::shutdown() noexcept
{
    stopping_.store(true, std::memory_order_release);
    if (wake_)
        SetEvent(wake_);
    if (worker_.joinable())
        worker_.join();
}
void AudioRingQueue::on_buffer_end(void *context) noexcept
{
    if (context)
    {
        static_cast<Packet *>(context)->complete.store(true, std::memory_order_release);
        ++completed_;
    }
    SetEvent(wake_);
}
} // namespace sgn::detail
