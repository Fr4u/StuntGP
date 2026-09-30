#include "sgn/renderer.hpp"
#include <stdexcept>
unsigned overlay_renderer_tripwire_calls=0;
namespace sgn {
void Renderer::replace_texture(unsigned,const Image&){
    ++overlay_renderer_tripwire_calls;
    throw std::runtime_error("CPU overlay test reached native Renderer::replace_texture");
}
std::array<unsigned,2> Renderer::texture_dimensions(unsigned) const{
    ++overlay_renderer_tripwire_calls;
    throw std::runtime_error("CPU overlay test reached native Renderer::texture_dimensions");
}
}
