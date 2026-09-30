#pragma once
#include "sgn/source_cold_language_input.hpp"
#include <utility>

namespace sgn::retail::cold_language_input_detail {
// The same four-line ordering kernel is used by the concrete owner binding
// and CPU tests. CPU child callables prove sequencing, never OS admission.
template<class LanguageCall,class InputCall>
SourceColdLanguageInputObservation entered_44215c(std::uint32_t& fixed_mode_b,
    LanguageCall&& language,InputCall&& input){
    std::forward<LanguageCall>(language)(); //44215C CALL445F80
    const std::uint32_t observed=std::forward<InputCall>(input)(); //442161 CALL4249B0
    fixed_mode_b=0; //442166, also after source return0; no BOOL/ready test
    return {observed}; //new diagnostic stop BEFORE442170 ->442030
}
}
