#pragma once
#include <cmath>
#include <cstddef>
#include <locale>
#include <sstream>
#include <string>
namespace bce::native::level_policy {
inline bool parse_numeric(const std::string& actualText,double& result) {
    if(actualText.empty())return false;
    std::istringstream input(actualText);input.imbue(std::locale::classic());
    input >> std::noskipws >> result;
    return !input.fail() && input.eof() && std::isfinite(result);
}
inline bool eligible(std::size_t completed,std::size_t expected,bool actualNumericRead,bool sourceSHAUnchanged,bool hardFailure) {
    return expected>=3 && completed==expected && actualNumericRead && sourceSHAUnchanged && !hardFailure;
}
} // namespace bce::native::level_policy
