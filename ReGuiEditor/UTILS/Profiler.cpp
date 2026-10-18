#include "Profiler.hpp" // VCIC-Excluded

#include "ValueAverager.hpp"

#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<std::string, UTILS::ValueAverager<double>>> UTILS::gProfilerResults;