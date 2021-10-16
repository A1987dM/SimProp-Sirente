#ifndef SIMPROP_UTILS_H
#define SIMPROP_UTILS_H

#include <string>

namespace simprop {
namespace utils {

// pow implementation as template for integer exponents
template <unsigned int exponent>
inline double pow(double base) {
  return pow<(exponent >> 1)>(base * base) * (((exponent & 1) > 0) ? base : 1);
}

template <>
inline double pow<0>(double base) {
  return 1;
}

// String Utilities
std::string removeExtensionIniFilename(std::string inputFilename);

}  // namespace utils
}  // namespace simprop

#endif  // SIMPROP_UTILS_H