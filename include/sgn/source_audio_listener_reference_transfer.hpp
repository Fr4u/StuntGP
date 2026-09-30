#pragma once
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>
#include <wrl/client.h>

namespace sgn
{
// Explicit extension for synchronous receiving providers, not a COM QI rule.
// Report at most one acquired reference, before callbacks can erase the output.
// Acceptance consumes provider_owned without AddRef. Rejection leaves it owned
// by the provider. The borrowed sink must not escape the query's call scope.
class SourceAudioListenerReferenceTransfer
{
  protected:
    SourceAudioListenerReferenceTransfer() = default;

  public:
    virtual ~SourceAudioListenerReferenceTransfer() = default;
    SourceAudioListenerReferenceTransfer(const SourceAudioListenerReferenceTransfer &) = delete;
    SourceAudioListenerReferenceTransfer &operator=(const SourceAudioListenerReferenceTransfer &) = delete;
    SourceAudioListenerReferenceTransfer(SourceAudioListenerReferenceTransfer &&) = delete;
    SourceAudioListenerReferenceTransfer &operator=(SourceAudioListenerReferenceTransfer &&) = delete;
    virtual void transfer_acquired(Microsoft::WRL::ComPtr<IDirectSound3DListener> &provider_owned) = 0;
};
using SourceAudioListenerQueryReceivingEntry = HRESULT (*)(IDirectSoundBuffer *, REFIID, void **,
                                                           SourceAudioListenerReferenceTransfer &);
} // namespace sgn
