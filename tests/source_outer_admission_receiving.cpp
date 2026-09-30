#include "sgn/session_outer_frame.hpp"
#include <iostream>
#include <stdexcept>

using sgn::retail::SessionOuterFrameCadence;
namespace {
template<class F>void rejects(F&& f){
    bool caught=false;
    try{f();}catch(const std::exception&){caught=true;}
    if(!caught)throw std::runtime_error("source outer admission accepted a forbidden call");
}
}
int main()try{
    unsigned groups=0;
    {SessionOuterFrameCadence c;c.begin(0);c.complete();c.begin(1);c.complete();++groups;}
    {SessionOuterFrameCadence c;c.begin(10);c.complete();rejects([&]{c.begin(10);});
        c.begin(11);c.complete();++groups;}
    {SessionOuterFrameCadence c;c.begin(10);c.complete();rejects([&]{c.begin(9);});
        c.begin(11);c.complete();++groups;}
    {SessionOuterFrameCadence c;c.begin(10);rejects([&]{c.begin(11);});
        c.complete();c.begin(11);c.complete();++groups;}
    {SessionOuterFrameCadence c;c.begin(10);c.poison();rejects([&]{c.begin(11);});++groups;}
    {SessionOuterFrameCadence c;c.begin(10);c.complete();c.poison();rejects([&]{c.begin(11);});++groups;}
    {SessionOuterFrameCadence c;c.begin(10);c.poison();c.complete();rejects([&]{c.begin(11);});++groups;}
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"scope\":\"actual existing cadence guard only\",\"GPU_executed\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
