#include "sgn/persistence_file_service.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
using namespace sgn;using namespace sgn::retail;
unsigned checks{};
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);++checks;}
std::vector<std::uint8_t> bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write_file(const std::filesystem::path& path,std::span<const std::uint8_t> data){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));require(bool(out),"fixture write failed");}
}
int main()try{
    const auto temporary=std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
    const auto parent=temporary.filename().empty()?temporary.parent_path():temporary;
    const auto root=(parent/(L"SGN-persistence-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()))).lexically_normal();
    require(root.is_absolute()&&root.parent_path()==parent,"unsafe fixture root");require(std::filesystem::create_directory(root),"fixture directory already exists");
    PersistenceFileService service(root);PersistenceBank live;PersistenceReadScratch scratch;std::array<std::uint8_t,persistence_body_size> saved;
    saved.fill(0xa5);scratch.bytes.fill(0xcd);for(std::size_t n=0;n<live.bytes.size();++n)live.bytes[n]=static_cast<std::uint8_t>(n*37+11);
    PersistenceView view{live.bytes,saved,scratch.bytes};
    require(save_persistence(view,service,0)==PersistenceStatus::no_card,"missing card status");
    require(bytes(root/L"detail.cfg").size()>2&&bytes(root/L"language.cfg").size()>2,"auxiliary files must be attempted after failed setup write");
    require(saved.front()==0xa5&&saved.back()==0xa5,"failed save acknowledged live state");
    service.format_card(0);service.format_card(8);require(service.card_directory_exists(0)&&service.card_directory_exists(8),"formatted card missing");
    persistence_word(live.bytes,0x84,97);persistence_word(live.bytes,0x90,8);const auto before=live.bytes;
    require(save_persistence(view,service,0)==PersistenceStatus::success,"host save failed");const auto setup=root/L"save"/L"card00"/L"setup.bin";auto disk=bytes(setup);
    require(disk.size()==persistence_size&&persistence_word(disk,0)==persistence_size,"saved file header/size");
    const auto checksum=persistence_word(disk,4);persistence_word(disk,4,0);require(persistence_checksum(disk)==checksum,"saved file checksum");
    require(std::equal(disk.begin()+8,disk.end(),before.begin()+8),"disk image must precede acknowledgment changes");
    require(!persistence_changed(view),"successful save did not refresh comparison");
    require(bytes(root/L"detail.cfg")==std::vector<std::uint8_t>{'9','7','\r','\n'},"Windows detail text bytes");
    require(bytes(root/L"language.cfg")==std::vector<std::uint8_t>{'8','\r','\n'},"Windows language text bytes");
    live.bytes.fill(0x77);require(load_persistence(view,service,0)==PersistenceStatus::success,"host load failed");
    require(persistence_word(live.bytes,4)==0&&persistence_word(scratch.bytes,4)==0,"loaded checksum word must remain zero");
    require(persistence_word(live.bytes,0x84)==90&&persistence_word(live.bytes,0x90)==8,"source auxiliary normalization");
    require(!persistence_changed(view),"load comparison dirty");
    auto bad=bytes(setup);bad[700]^=1;write_file(setup,bad);const auto retained=live.bytes;
    require(load_persistence(view,service,0)==PersistenceStatus::invalid_data&&live.bytes==retained,"corrupt checksum changed live bank");
    require(persistence_word(scratch.bytes,4)==0,"checksum failure must retain cleared scratch word");
    bad.resize(32);write_file(setup,bad);const auto old_scratch=scratch.bytes;
    require(load_persistence(view,service,0)==PersistenceStatus::invalid_data&&scratch.bytes==old_scratch,"wrong size must fail before reading");
    require(save_persistence(view,service,0)==PersistenceStatus::invalid_data&&bytes(setup)==bad,"wrong-size file overwritten");
    require(save_persistence(view,service,8)==PersistenceStatus::success,"second card save failed");
    const auto second=root/L"save"/L"card08"/L"setup.bin";const auto old_file=bytes(second);const auto old_saved=saved;
    require(SetFileAttributesW(second.c_str(),FILE_ATTRIBUTE_READONLY)!=0,"readonly fixture setup");bool commit_failed{};
    try{(void)save_persistence(view,service,8);}catch(const std::system_error&){commit_failed=true;}
    require(SetFileAttributesW(second.c_str(),FILE_ATTRIBUTE_NORMAL)!=0,"readonly fixture restore");
    require(commit_failed&&bytes(second)==old_file&&saved==old_saved,"failed atomic replacement lost prior save or acknowledged state");
    unsigned temporaries{};for(const auto& entry:std::filesystem::directory_iterator(second.parent_path()))if(entry.path().extension()==L".tmp")++temporaries;
    require(temporaries==0,"failed commit leaked temporary file");
    bool refused{};try{service.format_card(9);}catch(const std::out_of_range&){refused=true;}require(refused,"out-of-range host card accepted");
    auto slot=persistence_campaign_slot(view,7);slot[0]=0x4a;require(live.bytes[0xf1c+7*0x160]==0x4a,"campaign slot is not borrowed");
    auto records=persistence_campaign_records(view,24);records[31]=0xb3;require(live.bytes[0x1a1c+24*32+31]==0xb3,"campaign records are not borrowed");
    auto unlocks=persistence_unlocks(view);unlocks[25]=2;require(live.bytes[33]==2,"unlock owner duplicated");
    bool reparse_tested{};const auto redirect=root/L"redirect";
    if(CreateSymbolicLinkW(redirect.c_str(),(root/L"save").c_str(),SYMBOLIC_LINK_FLAG_DIRECTORY|2)){
        reparse_tested=true;bool rejected{};try{PersistenceFileService redirected(redirect);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"reparse root accepted");rejected=false;
        try{PersistenceFileService redirected_parent(redirect/L"card08");}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"reparse ancestor accepted");require(RemoveDirectoryW(redirect.c_str())!=0,"fixture symlink cleanup");
    }
    // Delete only the exact newly created fixture directory, after checking its
    // resolved identity and parent. No user save/settings path is enumerated.
    require(std::filesystem::weakly_canonical(root)==root&&root.parent_path()==parent,"fixture root changed before cleanup");
    std::filesystem::remove_all(root);require(!std::filesystem::exists(root),"fixture cleanup failed");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"new_temp_fixture_only\":true,\"atomic_failure_preserved_prior_file\":true,\"reparse_tested\":"<<(reparse_tested?"true":"false")<<",\"fixture_removed\":true,\"gpu_used\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
