#pragma once
#include "sgn/source_audio_listener_reference_transfer.hpp"

namespace listener_receiving_fixture {
// Test-only transport of explicit metadata from the fake SDK provider.
// Ordinary native tests leave this unset and obey the ordinary QI contract.
inline thread_local sgn::SourceAudioListenerReferenceTransfer* active_transfer=nullptr;
struct QueryContext {
    IDirectSoundBuffer* primary;
    sgn::SourceAudioListenerReferenceTransfer* transfer;
    bool claimed{};
};
inline thread_local QueryContext* active_query=nullptr;
inline HRESULT query_with_transfer(IDirectSoundBuffer* primary,REFIID iid,void** output,
    sgn::SourceAudioListenerReferenceTransfer& transfer){
    QueryContext context{primary,&transfer};
    struct Scope {
        sgn::SourceAudioListenerReferenceTransfer* previous;
        QueryContext* previous_query;
        explicit Scope(QueryContext& value):previous(active_transfer),previous_query(active_query){
            active_transfer=value.transfer;active_query=&value;
        }
        ~Scope(){active_transfer=previous;active_query=previous_query;}
    } scope{context};
    return primary->QueryInterface(iid,output);
}
// Claim once at fake QI entry, not at output publication. Nested ordinary QI
// must not borrow an outer invocation's sink, even on the same primary object.
inline sgn::SourceAudioListenerReferenceTransfer* begin_query(IDirectSoundBuffer* primary){
    if(!active_query||active_query->primary!=primary||active_query->claimed)return nullptr;
    active_query->claimed=true;
    return active_query->transfer;
}
inline void report_listener_acquisition(sgn::SourceAudioListenerReferenceTransfer* transfer,IDirectSound3DListener* acquired){
    if(!transfer)return;
    Microsoft::WRL::ComPtr<IDirectSound3DListener> provider_owned;
    provider_owned.Attach(acquired);
    transfer->transfer_acquired(provider_owned);
}
}
