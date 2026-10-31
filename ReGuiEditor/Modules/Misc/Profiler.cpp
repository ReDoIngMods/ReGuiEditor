#include "Profiler.hpp" // VCIC-Excluded

#include "../Math/ValueAverager.hpp"

#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<std::string, ReGuiEditor::ValueAverager<double>>> ReGuiEditor::gProfilerResults;