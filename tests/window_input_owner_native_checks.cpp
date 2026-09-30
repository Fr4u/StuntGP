// Proposed real-window ownership receiving. NOT run by this task. No source
// startup/shutdown, DirectInput creation/enumeration, polling or FF invocation.
#include "sgn/window_input_owner.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include <Windows.h>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void expect_error(F&& call,const char* reason){
    try{call();}catch(const std::exception& error){
        require(std::string(error.what()).find(reason)!=std::string::npos,"unexpected guard failure reason");return;
    }
    throw std::runtime_error("expected owner/lifetime rejection was absent");
}
struct HiddenWindows {
    HINSTANCE instance{::GetModuleHandleW(nullptr)};
    HWND first{},second{};
    ATOM atom{};
    static constexpr wchar_t name[]=L"StuntGPNewWindowInputOwnershipReceiving";
    HiddenWindows(){
        require(instance!=nullptr,"module instance unavailable");
        WNDCLASSEXW type{};type.cbSize=static_cast<UINT>(sizeof(type));type.hInstance=instance;
        type.lpfnWndProc=&::DefWindowProcW;type.lpszClassName=name;
        atom=::RegisterClassExW(&type);require(atom!=0,"register hidden window class failed");
        first=create();second=create();
        if(!first || !second){cleanup();throw std::runtime_error("create hidden top-level window failed");}
    }
    HWND create(){return ::CreateWindowExW(0,name,L"Input owner receiving",WS_OVERLAPPEDWINDOW,
        0,0,160,100,nullptr,nullptr,instance,nullptr);}
    InputWindowHandles handles() const{return {reinterpret_cast<std::uintptr_t>(instance),reinterpret_cast<std::uintptr_t>(first)};}
    void cleanup() noexcept{
        if(second){(void)::DestroyWindow(second);second=nullptr;}
        if(first){(void)::DestroyWindow(first);first=nullptr;}
        if(atom){(void)::UnregisterClassW(name,instance);atom=0;}
    }
    ~HiddenWindows(){cleanup();}
    HiddenWindows(const HiddenWindows&)=delete;
    HiddenWindows& operator=(const HiddenWindows&)=delete;
};
std::vector<std::uint32_t> snapshot(const MenuKeyboardRawState& raw){
    std::vector<std::uint32_t> words;
    for(const auto& record:raw.upstream)for(auto word:record.words)words.push_back(word);
    for(const auto& record:raw.physical){
        words.push_back(record.word_8);words.push_back(record.word_0c);words.push_back(record.word_24);words.push_back(record.word_28);
        for(auto word:record.words_from_2c)words.push_back(word);
        const auto& e=record.extra_edges;
        for(auto byte:{e.current_1a8,e.current_1a9,e.current_1ad,e.previous_1af,e.previous_1b0,e.previous_1b4})words.push_back(byte);
    }
    words.push_back(raw.source_device_pointer_576b6c);return words;
}
void poison(MenuKeyboardRawState& raw){
    for(auto& record:raw.upstream)record.words.fill(0x56789abcu);
    for(auto& record:raw.physical){
        record.word_8=7;record.word_0c=6;record.word_24=5;record.word_28=4;
        record.words_from_2c.fill(0xabcdefffu);record.extra_edges={1,2,3,4,5,6};
    }
    raw.source_device_pointer_576b6c=0x576810u;
}
}
int main(){
    try{
        HiddenWindows windows;const auto handles=windows.handles();
        MenuKeyboardRawState raw,foreign;poison(raw);poison(foreign);
        const auto before=snapshot(raw),foreign_before=snapshot(foreign);
        unsigned groups=0;
        {
            WindowInputOwner owner(raw,handles);
            require(snapshot(raw)==before,"host construction changed source raw input");
            owner.require_binding(raw,handles);++groups;
            for(unsigned i=0;i<16;++i)owner.require_binding(raw,handles);
            require(snapshot(raw)==before,"repeat binding changed source raw input");++groups;
            expect_error([&]{(void)owner.borrow_initialized(raw,handles);},"outside the initialized source lifetime");
            require(snapshot(raw)==before,"uninitialized borrow changed source raw input");++groups;
            expect_error([&]{owner.require_binding(foreign,handles);},"another raw owner");
            expect_error([&]{owner.require_binding(raw,{handles.instance+1u,handles.window});},"another raw owner");
            expect_error([&]{owner.require_binding(raw,{handles.instance,reinterpret_cast<std::uintptr_t>(windows.second)});},"another raw owner");
            require(snapshot(foreign)==foreign_before,"foreign binding mutated foreign raw owner");++groups;
            std::exception_ptr worker_failure;
            std::thread worker([&]{try{expect_error([&]{owner.require_binding(raw,handles);},"creating message thread");}
                                  catch(...){worker_failure=std::current_exception();}});
            worker.join();if(worker_failure)std::rethrow_exception(worker_failure);++groups;
            expect_error([&]{WindowInputOwner invalid(raw,{handles.instance+1u,handles.window});},"actual local top-level HWND");
            require(snapshot(raw)==before,"failed construction reset raw input");++groups;
            require(::DestroyWindow(windows.first)!=FALSE,"destroy first window failed");windows.first=nullptr;
            expect_error([&]{owner.require_binding(raw,handles);},"no longer valid");++groups;
            // Owner destructor now runs on the original thread with a dead
            // HWND. Only residual cleanup is permitted; no source event ran.
        }
        require(snapshot(raw)==before,"owner destruction changed raw source fields");
        std::cout<<groups<<" native window ownership groups passed; source startup/shutdown not invoked\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
