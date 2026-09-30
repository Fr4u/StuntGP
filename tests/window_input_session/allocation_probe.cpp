#include "allocation_probe.hpp"
#include <cstdlib>
#include <new>

// Replace the ordinary allocation/deallocation pair in THIS test EXE only.
// Aligned forms retain the CRT's paired implementations. A uint8_t vector
// uses the ordinary pair. Observation never allocates or dereferences p.
void* operator new(std::size_t size){
    for(;;){
        if(auto* p=std::malloc(size?size:1))return p;
        if(auto handler=std::get_new_handler())handler();else throw std::bad_alloc();
    }
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{allocation_probe::on_free(p);std::free(p);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
