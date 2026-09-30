#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace sgn::retail::detail {
inline bool horizontal(std::uint8_t c) noexcept{return c==' '||c=='\t';}
inline bool newline(std::uint8_t c) noexcept{return c=='\r'||c=='\n';}
struct CatalogToken {
    std::array<std::uint8_t,256> bytes{};
    std::array<bool,256> initialized{};
    explicit CatalogToken(bool legacy){initialized.fill(legacy);}
    template<class Input>void store(std::uint32_t at,std::uint8_t value,std::uint32_t site,Input& input){
        if(at>=bytes.size())input.fail(site,"frontend language token exceeds source buffer");
        bytes[at]=value;initialized[at]=true;
    }
    template<class Writer>std::span<const std::uint8_t> terminated(Writer& writer) const{
        for(std::size_t i=0;i<bytes.size();++i){
            if(!initialized[i])writer.fail(0x440bff,"frontend language flag reaches an unwritten local token byte");
            if(bytes[i]==0)return std::span(bytes).first(i+1);
        }
        writer.fail(0x440bff,"frontend language flag lacks a terminator");
    }
};
// One token algorithm. Each external read carries the instruction that actually
// reads that byte; the legacy adapter alone supplies its virtual end sentinel.
template<class Input>struct CatalogReader {
    Input& input;std::uint32_t cursor{};
    std::uint8_t read(std::uint32_t site){return input.read(cursor,site);}
    std::uint32_t token(CatalogToken& output){
        output.store(0,0,0x43c691,input);std::uint32_t size=0;
        auto c=read(0x43c696);
        while(horizontal(c)){++cursor;c=read(0x43c6a9);}
        c=read(0x43c6b7);if(c==0)return 0;
        if(c=='"'){
            c=input.read(cursor+1,0x43c6ce);++cursor;
            while(c!='"'&&c!=0){output.store(size++,c,0x43c6e3,input);++cursor;c=read(0x43c6e9);}
            if(read(0x43c6f3)!='"')return 0;++cursor;
        }else if(c!=','){
            for(;;){
                if(newline(read(0x43c710)))break;
                c=read(0x43c71e);if(c==0)break;
                output.store(size++,c,0x43c72a,input);++cursor;
                if(read(0x43c730)==',')break;
            }
        }
        output.store(size,0,0x43c73b,input);
        while(size&&output.bytes[size-1]==' ')output.store(--size,0,0x43c74c,input);
        if(read(0x43c754)==',')++cursor;return size;
    }
    void next_line(){
        if(read(0x43c5c9)!=0){
            for(;;){
                if(newline(read(0x43c5d1)))break;
                const auto c=input.read(cursor+1,0x43c5dd);++cursor;if(c==0)break;
            }
        }
        for(;;){const auto c=read(0x43c53a);if(!horizontal(c)&&!newline(c))break;++cursor;}
    }
};
inline std::optional<std::uint32_t> catalog_decimal(std::span<const std::uint8_t> raw){
    const auto end=std::find(raw.begin(),raw.end(),std::uint8_t{});
    std::size_t at=0;const auto length=static_cast<std::size_t>(end-raw.begin());
    while(at<length&&(horizontal(raw[at])||newline(raw[at])||raw[at]=='\v'||raw[at]=='\f'))++at;
    bool negative=false;if(at<length&&(raw[at]=='+'||raw[at]=='-'))negative=raw[at++]=='-';
    const auto first=at;std::uint32_t value=0;
    while(at<length&&raw[at]>='0'&&raw[at]<='9')value=value*10u+(raw[at++]-'0');
    if(at==first)return std::nullopt;return negative?0u-value:value;
}
template<class Input,class Writer>std::uint32_t parse_catalog_contents(Input& input,Writer& writer,bool legacy){
    CatalogReader<Input> reader{input};CatalogToken token(legacy);
    (void)reader.token(token);unsigned count=0;
    for(;count<8;++count){
        const auto size=reader.token(token);if(size==0)break;
        if(size>=32)writer.fail(0x440b6a,"frontend language name crosses its catalog flag");
        writer.name(count,std::span(token.bytes).first(size+1));
    }
    reader.next_line();writer.count(count);(void)reader.token(token);
    // Same EBP-10 at440BFF; failed CRT conversion skips468D96, then440C04 reads.
    std::optional<std::uint32_t> retained_flag;
    for(unsigned i=0;i<count;++i){
        (void)reader.token(token);if(const auto value=catalog_decimal(token.terminated(writer)))retained_flag=value;
        if(!retained_flag)writer.fail(0x440c04,"source frontend language flag conversion leaves an undefined local");
        writer.flag(i,*retained_flag);
    }
    return reader.cursor;
}
}
