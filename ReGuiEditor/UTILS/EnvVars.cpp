#include "EnvVars.hpp" // VCIC-Excluded: Includer cleaner is being a dum dum

#include <cstdlib>
#include <malloc.h>
#include <string>

bool UTILS::readEnvVar(const std::string& varName, std::string& outValue) {
	char* buffer = nullptr;

	size_t size = 0;
	errno_t err = _dupenv_s(&buffer, &size, varName.c_str());
	if (err != 0 || buffer == nullptr)
		return false;

	outValue.assign(buffer, size - 1);
	free(buffer);

	return true;
}

bool UTILS::writeEnvVar(const std::string& varName, const std::string& value) {
	return _putenv_s(varName.c_str(), value.c_str()) == 0;
}