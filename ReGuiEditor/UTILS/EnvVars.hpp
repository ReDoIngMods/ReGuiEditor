#pragma once
#include <string>

namespace UTILS {
	bool readEnvVar(const std::string& varName, std::string& outValue);
	bool writeEnvVar(const std::string& varName, const std::string& value);
}