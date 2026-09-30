#pragma once
// Private output-only owner/alias protocol. Nothing is imported into Session.
#include "normal_session_owner_limits.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace normal_session_owner {
static_assert(std::endian::native==std::endian::little);
inline void require(bool okay,const std::string& why){if(!okay)throw std::runtime_error(why);}
inline std::uint32_t narrow(std::size_t n){require(n<=std::numeric_limits<std::uint32_t>::max(),"owner count overflow");return static_cast<std::uint32_t>(n);}
inline std::uint32_t add(std::uint32_t base,std::size_t offset){require(offset<=0xffffffffull-base,"guest address overflow");return base+static_cast<std::uint32_t>(offset);}
inline std::uint32_t word(std::span<const std::uint8_t> data,std::size_t offset){require(offset<=data.size()&&data.size()-offset>=4,"owner word outside extent");std::uint32_t value;std::memcpy(&value,data.data()+offset,4);return value;}
inline void raw(std::ostream& out,const void* data,std::size_t n){out.write(static_cast<const char*>(data),static_cast<std::streamsize>(n));require(bool(out),"owner write failed");}
inline void number(std::ostream& out,std::uint32_t value){raw(out,&value,4);}
namespace wire_detail {
inline bool valid_utf8(std::string_view value) noexcept {
    for(std::size_t i=0;i<value.size();){
        const auto first=static_cast<unsigned char>(value[i++]);
        if(first<0x80){if(first==0)return false;continue;}
        unsigned extra{};std::uint32_t scalar{},minimum{};
        if(first>=0xc2&&first<=0xdf){extra=1;scalar=first&0x1fu;minimum=0x80;}
        else if(first>=0xe0&&first<=0xef){extra=2;scalar=first&0x0fu;minimum=0x800;}
        else if(first>=0xf0&&first<=0xf4){extra=3;scalar=first&0x07u;minimum=0x10000;}
        else return false;
        if(extra>value.size()-i)return false;
        for(unsigned j=0;j<extra;++j){
            const auto next=static_cast<unsigned char>(value[i++]);
            if((next&0xc0u)!=0x80u)return false;
            scalar=(scalar<<6)|(next&0x3fu);
        }
        if(scalar<minimum||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff))return false;
    }
    return true;
}
inline void label(std::string_view value){
    require(!value.empty()&&value.size()<=wire_limits::label_bytes&&valid_utf8(value),"invalid UTF-8 owner label");
}
inline void charge(std::uint64_t& current,std::uint64_t increment,std::uint64_t maximum,const char* why){
    require(current<=maximum&&increment<=maximum-current,why);current+=increment;
}
struct FileBudget {
    wire_limits::Policy limits;
    std::uint64_t allocations{},aliases{},claims{},payload{},metadata{},wire{};
    explicit FileBudget(unsigned version):limits(wire_limits::policy(version)){framing(12);}
    void framing(std::uint64_t bytes){
        charge(metadata,bytes,limits.metadata_bytes,"owner metadata byte limit");
        charge(wire,bytes,limits.wire_bytes,"owner total wire byte limit");
    }
    void data(std::uint64_t bytes){
        charge(payload,bytes,wire_limits::payload_bytes_per_file,"owner aggregate payload byte limit");
        charge(wire,bytes,limits.wire_bytes,"owner total wire byte limit");
    }
};
using Interval=std::pair<std::uint64_t,std::uint64_t>;
inline void intervals(std::vector<Interval>& spans,bool partition,std::uint64_t extent){
    std::sort(spans.begin(),spans.end());std::uint64_t end=0;
    for(const auto& span:spans){
        require(partition?span.first==end:span.first>=end,partition?"unrepresented/overlapping live partition":"overlapping owner claims");
        end=span.second;
    }
    if(partition)require(end==extent,"unrepresented live partition tail");
}
}
inline void text(std::ostream& out,std::string_view value){wire_detail::label(value);number(out,narrow(value.size()));raw(out,value.data(),value.size());}

enum class Origin:std::uint32_t {authored=1,live=2,relocation=3,source_initialized=4};
struct Claim {std::uint32_t offset{},length{};Origin origin{};std::string name;};
struct Allocation {std::uint32_t id{},address{};std::string name;std::vector<std::uint8_t> bytes;std::vector<Claim> claims;};
struct Alias {std::string name;std::uint32_t address{},length{},owner_id{},owner_offset{};bool one_past{};};
class Image;
struct NamedImage {std::string_view label;const Image* image;};
inline void save_file(std::ostream& out,std::span<const NamedImage> snapshots);

// Each physical guest extent occurs once. Claims identify explicitly authored,
// current-live or relocated fields. Full allocation bytes are always present;
// claims never become comparison masks. Later writes may replace authored
// claims, but may not replace another live/relocated claim in one snapshot.
class Image {
    unsigned version_;
    wire_detail::FileBudget append_budget_;
    bool mutable_records_exposed_{};
    std::vector<Allocation> allocations_;
    std::vector<Alias> aliases_;
    std::map<std::uint32_t,std::vector<std::uint8_t>> claimed_;
    friend void save_file(std::ostream&,std::span<const NamedImage>);

    static void account_owner(wire_detail::FileBudget& budget,std::string_view name,
        std::size_t bytes,std::span<const Claim> claims){
        wire_detail::label(name);
        require(bytes>0&&bytes<=wire_limits::allocation_bytes,"owner allocation extent");
        require(!claims.empty()&&claims.size()<=wire_limits::claims_per_owner,"owner claim count");
        wire_detail::charge(budget.allocations,1,budget.limits.allocations_per_file,"owner file allocation count");
        wire_detail::charge(budget.claims,claims.size(),budget.limits.claims_per_file,"owner file claim count");
        budget.framing(20+name.size());budget.data(bytes);
        for(const auto& claim:claims){wire_detail::label(claim.name);budget.framing(16+claim.name.size());}
    }
    static void account_alias(wire_detail::FileBudget& budget,std::string_view name){
        wire_detail::label(name);
        wire_detail::charge(budget.aliases,1,budget.limits.aliases_per_file,"owner file alias count");
        budget.framing(24+name.size());
    }
    wire_detail::FileBudget current_append_budget() const {
        if(version_!=3||!mutable_records_exposed_)return append_budget_;
        // Escaped references may be used again even after a previous check, so
        // this flag is never reset. Recount budgets before each later mutation.
        // Forward relocation targets may still be awaiting their owner; only
        // complete save preflight validates the finished graph.
        wire_detail::FileBudget actual(version_);actual.framing(13);
        for(const auto& owner:allocations_)account_owner(actual,owner.name,owner.bytes.size(),owner.claims);
        for(const auto& alias:aliases_)account_alias(actual,alias.name);
        return actual;
    }
    std::uint32_t append_owned(std::string name,std::uint32_t address,
        std::span<const std::uint8_t> initial,std::vector<Claim> claims,bool fully_claimed,wire_detail::FileBudget next){
        require_allocation_capacity(1);
        const auto end=std::uint64_t(address)+initial.size();
        require(address&&end<=0x100000000ull,"guest allocation overflow");
        for(const auto& a:allocations_){require(a.name!=name,"duplicate allocation name");require(end<=a.address||std::uint64_t(a.address)+a.bytes.size()<=address,"overlapping guest allocation");}
        const auto id=narrow(allocations_.size()+1);
        // Payload and bitmap allocation happen only after the checked budget.
        Allocation owner{id,address,std::move(name),{initial.begin(),initial.end()},std::move(claims)};
        std::vector<std::uint8_t> flags(initial.size(),fully_claimed?1u:0u);
        const auto inserted=claimed_.emplace(id,std::move(flags));
        require(inserted.second,"duplicate private owner bitmap");
        try{allocations_.push_back(std::move(owner));}
        catch(...){claimed_.erase(inserted.first);throw;}
        append_budget_=next;return id;
    }

    // Rebuild all validation indexes from actual records. In particular, a
    // caller may have retained allocation(id)& and changed it after append.
    void validate(std::string_view name,wire_detail::FileBudget& budget,unsigned grammar) const {
        wire_detail::label(name);
        require(!allocations_.empty()&&allocations_.size()<=max_allocations(),"owner snapshot allocation count");
        require(aliases_.size()<=wire_limits::aliases_per_snapshot,"owner snapshot alias count");
        wire_detail::charge(budget.allocations,allocations_.size(),budget.limits.allocations_per_file,"owner file allocation count");
        wire_detail::charge(budget.aliases,aliases_.size(),budget.limits.aliases_per_file,"owner file alias count");
        budget.framing(12+name.size()); // name length, name, allocation count, alias count
        std::set<std::string_view> names;
        std::vector<const Allocation*> ordered;ordered.reserve(allocations_.size());
        for(std::size_t index=0;index<allocations_.size();++index){
            const auto& a=allocations_[index];wire_detail::label(a.name);
            require(a.id==index+1&&names.insert(a.name).second,"owner allocation ID/name collision");
            require(a.address&&a.bytes.size()>0&&a.bytes.size()<=wire_limits::allocation_bytes,"owner allocation extent");
            require(std::uint64_t(a.address)+a.bytes.size()<=wire_limits::address_space_end,"owner allocation address overflow");
            require(!a.claims.empty()&&a.claims.size()<=wire_limits::claims_per_owner,"owner claim count");
            wire_detail::charge(budget.claims,a.claims.size(),budget.limits.claims_per_file,"owner file claim count");
            budget.framing(20+a.name.size());budget.data(a.bytes.size());
            const auto first=a.claims.front().origin;
            const bool partition=grammar>=2&&(first==Origin::live||first==Origin::relocation);
            std::vector<wire_detail::Interval> spans;spans.reserve(a.claims.size());
            for(std::size_t i=0;i<a.claims.size();++i){
                const auto& c=a.claims[i];wire_detail::label(c.name);budget.framing(16+c.name.size());
                require(c.length&&c.offset<=a.bytes.size()&&c.length<=a.bytes.size()-c.offset,"owner claim outside allocation");
                require(c.origin==Origin::authored||c.origin==Origin::live||c.origin==Origin::relocation||c.origin==Origin::source_initialized,"invalid owner claim origin");
                require(c.origin!=Origin::relocation||c.length==4,"relocation must be one source pointer");
                if(partition){
                    require(c.origin==Origin::live||c.origin==Origin::relocation,"partition needs live/relocation provenance");
                    spans.emplace_back(c.offset,std::uint64_t(c.offset)+c.length);
                }else if(i==0){
                    require(c.offset==0&&c.length==a.bytes.size()&&(first==Origin::authored||first==Origin::live||first==Origin::source_initialized),"initial claim must cover whole allocation");
                    if(first==Origin::live)spans.emplace_back(0,a.bytes.size());
                }else{
                    require(c.origin==Origin::live||c.origin==Origin::relocation||c.origin==Origin::source_initialized,"invalid overlay origin");
                    spans.emplace_back(c.offset,std::uint64_t(c.offset)+c.length);
                }
            }
            wire_detail::intervals(spans,partition,a.bytes.size());ordered.push_back(&a);
        }
        std::sort(ordered.begin(),ordered.end(),[](const Allocation* a,const Allocation* b){return a->address<b->address;});
        for(std::size_t i=1;i<ordered.size();++i)
            require(std::uint64_t(ordered[i-1]->address)+ordered[i-1]->bytes.size()<=ordered[i]->address,"overlapping physical allocations");
        std::set<std::string_view> alias_names;std::set<std::uint32_t> one_past;
        for(const auto& a:aliases_){
            wire_detail::label(a.name);budget.framing(24+a.name.size());
            require(alias_names.insert(a.name).second,"duplicate alias name");
            require(a.owner_id&&a.owner_id<=allocations_.size(),"alias allocation ID");
            const auto& owner=allocations_[a.owner_id-1];
            require(a.owner_offset<=owner.bytes.size()&&a.length<=owner.bytes.size()-a.owner_offset,"alias outside explicit owner");
            const auto target=std::uint64_t(owner.address)+a.owner_offset;
            require(target<=0xffffffffull&&a.address==target,"alias address not representable or differs from owner");
            require(a.one_past==(a.owner_offset==owner.bytes.size())&&(!a.one_past||a.length==0),"alias one-past owner/extent mismatch");
            if(a.one_past)one_past.insert(a.address);
        }
        for(const auto& a:allocations_)for(const auto& c:a.claims)if(c.origin==Origin::relocation){
            const auto target=word(a.bytes,c.offset);if(!target)continue;
            const auto upper=std::upper_bound(ordered.begin(),ordered.end(),target,
                [](std::uint32_t value,const Allocation* owner){return value<owner->address;});
            const bool inside=upper!=ordered.begin()&&std::uint64_t(target)<std::uint64_t((*(upper-1))->address)+(*(upper-1))->bytes.size();
            require(inside||one_past.contains(target),"relocation target lacks an owner or explicit one-past alias");
        }
    }
    void write_snapshot(std::ostream& out,std::string_view label) const {
        text(out,label);number(out,narrow(allocations_.size()));
        for(const auto& a:allocations_){number(out,a.id);number(out,a.address);text(out,a.name);number(out,narrow(a.bytes.size()));raw(out,a.bytes.data(),a.bytes.size());number(out,narrow(a.claims.size()));
            for(const auto& c:a.claims){number(out,c.offset);number(out,c.length);number(out,static_cast<std::uint32_t>(c.origin));text(out,c.name);}}
        number(out,narrow(aliases_.size()));for(const auto& a:aliases_){text(out,a.name);for(auto v:{a.address,a.length,a.owner_id,a.owner_offset,std::uint32_t(a.one_past)})number(out,v);}
    }
public:
    explicit Image(unsigned version=1):version_(version),append_budget_(version){
        require(version>=1&&version<=3,"owner format version");
        // The eventual snapshot name is not known yet. Count all fixed file
        // and snapshot headers and its minimum nonempty one-byte name here.
        // save_file charges the actual name length and all other snapshots.
        append_budget_.framing(13);
    }
    unsigned version() const noexcept{return version_;}
    std::size_t max_allocations() const noexcept{return wire_limits::policy(version_).allocations_per_snapshot;}
    void require_allocation_capacity(std::size_t additional) const {
        const auto maximum=max_allocations();
        require(allocations_.size()<=maximum&&additional<=maximum-allocations_.size(),"owner allocation capacity exceeded");
    }
    std::span<const Allocation> allocations() const noexcept{return allocations_;}
    const Allocation& allocation(std::uint32_t id) const {
        require(id&&id<=allocations_.size(),"unknown allocation ID");return allocations_[id-1];
    }
    Allocation& allocation(std::uint32_t id){require(id&&id<=allocations_.size(),"unknown allocation ID");mutable_records_exposed_=true;return allocations_[id-1];}
    std::uint32_t create(std::string name,std::uint32_t address,std::span<const std::uint8_t> initial,Origin origin){
        require_allocation_capacity(1);wire_detail::label(name);
        require(initial.size()>0&&initial.size()<=wire_limits::allocation_bytes,"invalid allocation extent");
        require(origin==Origin::authored||origin==Origin::live||origin==Origin::source_initialized,"initial allocation origin");
        std::vector<Claim> claims{{0,narrow(initial.size()),origin,name}};
        auto next=current_append_budget();if(version_==3)account_owner(next,name,initial.size(),claims);
        return append_owned(std::move(name),address,initial,std::move(claims),origin==Origin::live,next);
    }
    template<class T,std::size_t N>std::uint32_t create_live(std::string name,std::uint32_t address,std::span<T,N> values){
        static_assert(std::is_trivially_copyable_v<T>);return create(std::move(name),address,{reinterpret_cast<const std::uint8_t*>(values.data()),values.size_bytes()},Origin::live);
    }
    // A translated native pointer record has no invented initial source bytes.
    // Each byte must belong to exactly one live field or normalized pointer.
    std::uint32_t create_encoded(std::string name,std::uint32_t address,std::span<const std::uint8_t> bytes,std::vector<Claim> claims){
        require_allocation_capacity(1);wire_detail::label(name);
        require(!bytes.empty()&&bytes.size()<=wire_limits::allocation_bytes&&!claims.empty()&&claims.size()<=wire_limits::claims_per_owner,"encoded owner size/claim count");
        auto next=current_append_budget();if(version_==3)account_owner(next,name,bytes.size(),claims);
        std::vector<wire_detail::Interval> spans;spans.reserve(claims.size());
        for(const auto& claim:claims){
            wire_detail::label(claim.name);
            require(claim.length&&claim.offset<=bytes.size()&&claim.length<=bytes.size()-claim.offset,"encoded claim outside owner");
            require(claim.origin==Origin::live||claim.origin==Origin::relocation,"encoded record needs live/pointer provenance");
            require(claim.origin!=Origin::relocation||claim.length==4,"encoded pointer must be four bytes");
            spans.emplace_back(claim.offset,std::uint64_t(claim.offset)+claim.length);
        }
        wire_detail::intervals(spans,true,bytes.size());
        return append_owned(std::move(name),address,bytes,std::move(claims),true,next);
    }
    void write(std::uint32_t id,std::uint32_t offset,const void* data,std::size_t length,Origin origin,std::string name){
        wire_detail::label(name);require(data!=nullptr,"null owner overlay data");
        require(id&&id<=allocations_.size(),"unknown allocation ID");
        auto& a=allocations_[id-1];require(length>0&&offset<=a.bytes.size()&&length<=a.bytes.size()-offset,"write outside guest allocation");
        require(origin==Origin::live||origin==Origin::relocation||origin==Origin::source_initialized,"overlay must have explicit live/relocation/source-initialized origin");
        require(origin!=Origin::relocation||length==4,"relocation must be one source pointer");
        require(a.claims.size()<wire_limits::claims_per_owner,"owner claim count");
        auto& flags=claimed_.at(id);require(flags.size()==a.bytes.size(),"owner extent changed after append");
        for(std::size_t i=offset;i<offset+length;++i)require(flags[i]==0,"duplicate live field ownership: "+name);
        auto next=current_append_budget();
        if(version_==3){
            wire_detail::charge(next.claims,1,next.limits.claims_per_file,"owner file claim count");
            next.framing(16+name.size());
        }
        // Commit the fallible claim append before the nonallocating byte write.
        a.claims.push_back({offset,narrow(length),origin,std::move(name)});
        std::fill(flags.begin()+offset,flags.begin()+offset+length,std::uint8_t{1});std::memcpy(a.bytes.data()+offset,data,length);
        append_budget_=next;
    }
    template<class T>void live(std::uint32_t id,std::uint32_t offset,const T& value,std::string name){static_assert(std::is_trivially_copyable_v<T>);write(id,offset,&value,sizeof(value),Origin::live,std::move(name));}
    template<class T>void initialized(std::uint32_t id,std::uint32_t offset,const T& value,std::string name){static_assert(std::is_trivially_copyable_v<T>);write(id,offset,&value,sizeof(value),Origin::source_initialized,std::move(name));}
    void pointer(std::uint32_t id,std::uint32_t offset,std::uint32_t value,std::string name){write(id,offset,&value,4,Origin::relocation,std::move(name));}
    Alias reference(std::string name,std::uint32_t address,std::uint32_t length,bool one_past=false) const {
        wire_detail::label(name);
        require(address!=0,"null address is not a registered alias");
        const Allocation* match=nullptr;
        for(const auto& a:allocations_){const auto end=std::uint64_t(a.address)+a.bytes.size();
            const bool inside=address>=a.address&&std::uint64_t(address)+length<=end&&address<end;
            const bool cursor=one_past&&length==0&&address==end;
            if(inside||cursor){require(match==nullptr,"ambiguous guest boundary alias");match=&a;}}
        require(match,"alias outside registered guest owner: "+name);return {std::move(name),address,length,match->id,address-match->address,address==std::uint64_t(match->address)+match->bytes.size()};
    }
    void alias(std::string name,std::uint32_t address,std::uint32_t length,bool one_past=false){
        require(aliases_.size()<wire_limits::aliases_per_snapshot,"owner snapshot alias count");
        auto next=current_append_budget();if(version_==3)account_alias(next,name);
        for(const auto& a:aliases_)require(a.name!=name,"duplicate alias label");
        aliases_.push_back(reference(std::move(name),address,length,one_past));append_budget_=next;
    }
    void alias_in(std::string name,std::uint32_t id,std::uint32_t offset,std::uint32_t length){
        wire_detail::label(name);require(aliases_.size()<wire_limits::aliases_per_snapshot,"owner snapshot alias count");
        auto next=current_append_budget();if(version_==3)account_alias(next,name);
        for(const auto& a:aliases_)require(a.name!=name,"duplicate alias label");const auto& a=static_cast<const Image&>(*this).allocation(id);
        require(offset<=a.bytes.size()&&length<=a.bytes.size()-offset,"explicit alias outside owner");
        aliases_.push_back({std::move(name),add(a.address,offset),length,id,offset,offset==a.bytes.size()});append_budget_=next;
    }
    void save(std::ostream& out,std::string_view label) const {
        // Historical callers write file_header(2) around Image(default1).
        // This compatibility entry accepts the v2 grammar, but never v3.
        // Only save_file can validate budgets across multiple snapshots.
        require(version_<=2,"v3 requires save_file full-file validation");
        wire_detail::FileBudget budget(version_);validate(label,budget,2);
        require(bool(out),"owner output stream is not writable");write_snapshot(out,label);
    }
};

// Register complete actual native array owners once. Resolution only computes
// offsets after an observed pointer fits the registered native extent. It does
// not dereference arbitrary addresses or make a foreign pointer native.
class NativeAddresses {
    struct Owner {std::string name;std::uintptr_t begin{};std::size_t count{},native_stride{},source_stride{};std::uint32_t guest{};};
    std::vector<Owner> owners_;
public:
    template<class T,std::size_t N>void array(std::string name,std::span<T,N> values,std::uint32_t guest,std::size_t source_stride=sizeof(T)){
        if(values.empty())return;
        const auto begin=reinterpret_cast<std::uintptr_t>(values.data());const auto bytes=values.size_bytes();
        require(begin&&bytes<=std::numeric_limits<std::uintptr_t>::max()-begin&&source_stride&&values.size()<=0xffffffffull/source_stride,"native owner extent overflow");
        add(guest,values.size()*source_stride);
        for(const auto& o:owners_){const auto end=o.begin+o.count*o.native_stride;
            require(o.name!=name,"duplicate native owner label");require(begin+bytes<=o.begin||end<=begin,"duplicate/overlapping native array owner: "+name);
            require(std::uint64_t(guest)+values.size()*source_stride<=o.guest||std::uint64_t(o.guest)+o.count*o.source_stride<=guest,"different native arrays map to the same guest owner bytes");}
        owners_.push_back({std::move(name),begin,values.size(),sizeof(T),source_stride,guest});
    }
    std::uint32_t address(const void* value,std::size_t byte_extent=0,bool allow_one_past=false) const {
        if(!value){require(byte_extent==0,"null native view has nonzero extent");return 0;}
        const auto p=reinterpret_cast<std::uintptr_t>(value);std::optional<std::uint32_t> result;
        for(const auto& o:owners_){const auto end=o.begin+o.count*o.native_stride;
            if(p<o.begin||p>end||(p==end&&!allow_one_past)||byte_extent>end-p)continue;
            const auto offset=p-o.begin;const auto item=offset/o.native_stride,inner=offset%o.native_stride;
            require(o.native_stride==o.source_stride||inner==0,"interior pointer into differently laid out native record");
            require(!result.has_value(),"ambiguous native owner boundary");result=add(o.guest,item*o.source_stride+inner);
        }
        require(result.has_value(),"native pointer lacks a registered owner");return *result;
    }
    template<class T,std::size_t N>void check_view(std::span<T,N> values,std::uint32_t expected,bool one_past=false) const {
        require(address(values.data(),values.size_bytes(),one_past)==expected,"native span alias disagrees with authored section offset");
    }
};

inline void file_header(std::ostream& out,std::uint32_t snapshots,unsigned version=1){require(snapshots>0&&snapshots<=1024&&version>=1&&version<=2,"owner snapshot count/version");raw(out,version==1?"SGNOWN01":"SGNOWN02",8);number(out,snapshots);}

// Images and labels must remain stable for this synchronous call. Validation
// failures append no bytes; an I/O failure may leave a partial stream, which
// the caller must not publish as a completed capture. Check flush separately.
inline void save_file(std::ostream& out,std::span<const NamedImage> snapshots){
    require(!snapshots.empty()&&snapshots.size()<=wire_limits::snapshots,"owner file snapshot count");
    require(snapshots.front().image!=nullptr,"null owner image");
    const auto version=snapshots.front().image->version();
    require(version>=1&&version<=3,"owner file format version");
    wire_detail::FileBudget budget(version);std::set<std::string_view> names;
    for(const auto& named:snapshots){
        require(named.image!=nullptr,"null owner image");
        require(named.image->version()==version,"mixed owner image versions");
        wire_detail::label(named.label);require(names.insert(named.label).second,"duplicate snapshot name");
        named.image->validate(named.label,budget,version);
    }
    require(bool(out),"owner output stream is not writable");
    raw(out,version==3?"SGNOWN03":version==2?"SGNOWN02":"SGNOWN01",8);number(out,narrow(snapshots.size()));
    for(const auto& named:snapshots)named.image->write_snapshot(out,named.label);
}
}
