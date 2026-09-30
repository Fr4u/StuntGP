#include "vehicle_integration_fixture.hpp"
int main(int argc,char** argv){
    return sgn::retail::integration_test::run_corpus(argc,argv,"SGNVT002",sgn::retail::tick_session_vehicle);
}
