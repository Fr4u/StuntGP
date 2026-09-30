#include "sgn/retail_frontend_clock.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
template<class T> void read(std::ifstream& input, T& value) {
    if (!input.read(reinterpret_cast<char*>(&value),sizeof(value))) throw std::runtime_error("short frontend clock corpus");
}
using Record = std::array<std::uint32_t,6>;
struct Event {std::uint32_t kind; std::uint64_t value;};
struct Counter final : sgn::retail::FrontendPerformanceCounter {
    std::vector<Event> events; std::size_t cursor{};
    std::uint64_t next(std::uint32_t kind) {
        if (cursor == events.size() || events[cursor].kind != kind) throw std::runtime_error("frontend counter request order");
        return events[cursor++].value;
    }
    std::uint64_t counter() override {return next(0);}
    std::uint64_t frequency() override {return next(1);}
};
}
int main(int argc,char** argv) try {
    if (argc != 2) throw std::runtime_error("expected frontend clock corpus");
    std::ifstream input(argv[1],std::ios::binary); std::array<char,8> magic{}; read(input,magic);
    if (std::string_view(magic.data(),8) != "SGNFCLK1") throw std::runtime_error("frontend clock schema");
    unsigned groups{}; read(input,groups); if (!groups || groups > 10000) throw std::runtime_error("frontend clock groups");
    std::uint64_t calls=0,continuations=0,requests=0;
    for (unsigned group=0;group<groups;++group) {
        Record r{}; read(input,r); unsigned steps{}; read(input,steps);
        if (!steps || steps > 64) throw std::runtime_error("frontend clock steps");
        sgn::retail::RetailTickClock clock{r[0],r[1],r[2]};
        for (unsigned step=0;step<steps;++step) {
            unsigned op{},expected_return{},count{}; Record expected{};
            read(input,op); read(input,expected_return); read(input,expected); read(input,count);
            if (count>3 || op<1 || op>3) throw std::runtime_error("frontend clock operation/event count");
            Counter counter;
            for (unsigned i=0;i<count;++i) {Event event{};read(input,event.kind);read(input,event.value);counter.events.push_back(event);}
            unsigned returned=0;
            if (op==1) returned=sgn::retail::read_frontend_counter_434820(counter);
            else if (op==2) sgn::retail::initialize_frontend_clock_434840(clock,r[3],counter);
            else sgn::retail::sample_frontend_clock_442490(clock,r[4],r[5],counter);
            r[0]=clock.scheduled;r[1]=clock.observed;r[2]=clock.increment;
            if (r!=expected || returned!=expected_return || counter.cursor!=counter.events.size())
                throw std::runtime_error("frontend clock state/return/request mismatch");
            ++calls;continuations+=step!=0;requests+=count;
        }
    }
    if (input.peek()!=std::ifstream::traits_type::eof()) throw std::runtime_error("trailing frontend clock corpus");
    std::cout<<"{\"passed\":true,\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"continuations\":"<<continuations
             <<",\"compared_bytes\":"<<calls*28<<",\"os_requests\":"<<requests<<",\"mismatches\":0}\n";
    return 0;
} catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
