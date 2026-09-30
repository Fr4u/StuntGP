#include "contact_solver_state.hpp"
#include "contact_ai_stunt_state.hpp"
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::contact;
constexpr unsigned size=0x8000,globals_at=0x7800,knots_at=0x7900;
using Record=std::vector<std::uint8_t>;
unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
void word(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
float number(const Record& b,unsigned at){return std::bit_cast<float>(word(b,at));}
void apply(Record& b,unsigned entry,const SplineSet& splines){
    VehicleParameters parameters{};VehicleState v{};VehicleRaceState race{};solver::State c{};AiStuntHelperState helper{};
    contact::oracle::transfer(b,parameters,v,race,c,true);ai_stunt_oracle::transfer(b,helper,true);
    AiAutomaticMemory memory{};memory.counter2674=word(b,0x2674);memory.lateral_fraction=number(b,0x266c);
    AiStuntContext context{word(b,globals_at),{number(b,globals_at+8),number(b,globals_at+12),number(b,globals_at+16)}};
    VehicleRandom random{word(b,globals_at+4)};AiAutomaticGlobals globals{};
    globals.routes=ai_spline_routes(splines);globals.route_distances[2]=number(b,globals_at+20);
    AiStuntKnots knots{};for(unsigned j=0;j<33;++j)knots[j]={number(b,knots_at+j*12),number(b,knots_at+j*12+4),word(b,knots_at+j*12+8)};
    if(entry==0x406ea0)advance_ai_stunt(parameters,v,helper,context);
    else if(entry==0x407010)choose_ai_stunt({parameters,v,race,memory},c,helper,globals,knots,random,context);
    else throw std::runtime_error("unknown AI stunt entry");
    contact::oracle::transfer(b,parameters,v,race,c,false);ai_stunt_oracle::transfer(b,helper,false);
    word(b,0x2674,memory.counter2674);word(b,globals_at+4,random.state);
}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected AI stunt corpus and PMD");
    const SplineSet splines(read_file(argv[2]));std::ifstream file(argv[1],std::ios::binary);if(!file)throw std::runtime_error("AI stunt corpus open");
    const auto read=[&](void* output,std::size_t count){file.read(static_cast<char*>(output),static_cast<std::streamsize>(count));if(!file)throw std::runtime_error("truncated AI stunt corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::memcmp(magic,"SGNAS001",8))throw std::runtime_error("AI stunt corpus magic");
    const auto cases=next();if(cases==0||cases>20000)throw std::runtime_error("AI stunt case count");
    std::uint64_t words=0;unsigned parser_cases=0,tick_cases=0;
    for(unsigned trial=0;trial<cases;++trial){
        const auto entry=next();
        if(entry==0x4467b0){const auto count=next();if(count>1000000)throw std::runtime_error("AI stunt config size");Bytes text(count);read(text.data(),count);
            std::array<unsigned,99> expected{};read(expected.data(),sizeof(expected));const auto knots=decode_ai_stunt_knots(text);
            for(unsigned j=0;j<33;++j){const std::array<unsigned,3> actual{std::bit_cast<unsigned>(knots[j].progress),std::bit_cast<unsigned>(knots[j].width),knots[j].grade};
                for(unsigned k=0;k<3;++k){++words;if(actual[k]!=expected[j*3+k])throw std::runtime_error("AI stunt config mismatch trial "+std::to_string(trial)+" row "+std::to_string(j));}}
            ++parser_cases;
        }else{
            Record actual(size),expected(size);read(actual.data(),size);read(expected.data(),size);apply(actual,entry,splines);
            for(unsigned at=0;at<size;at+=4){++words;if(word(actual,at)!=word(expected,at))throw std::runtime_error("AI stunt mismatch trial "+std::to_string(trial)+" entry "+std::to_string(entry)+" offset "+std::to_string(at)+" actual "+std::to_string(word(actual,at))+" expected "+std::to_string(word(expected,at)));}
            ++tick_cases;
        }
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing AI stunt corpus");
    std::cout<<"{\"cases\":"<<cases<<",\"parser_cases\":"<<parser_cases<<",\"tick_cases\":"<<tick_cases<<",\"exact_words\":"<<words<<",\"errors\":0,\"allowed_ulp\":0,\"excluded_words\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
