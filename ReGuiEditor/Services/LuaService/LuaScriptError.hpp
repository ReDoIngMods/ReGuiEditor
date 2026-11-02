#pragma once
#include <sstream>
#include <string>
#include <vector>

namespace ReGuiEditor {
	struct LuaStackFrame {
		std::string source;
		std::string functionName;
		std::string what;
		int line = -1;
	};

	struct LuaScriptError {
		std::string message;
		std::vector<LuaStackFrame> frames;

		[[nodiscard]] std::string formatTraceback() const {
			std::ostringstream stream;
			stream << "--- Lua Stack Traceback ---";
			for (const LuaStackFrame& frame : this->frames) {
				stream << "\n\t" << frame.source;
				if (frame.line != -1)
					stream << ":" << frame.line << ":";
				
				if (!frame.what.empty())
					stream << " (" << frame.what << ")";

				if (!frame.functionName.empty())
					stream << " in " << frame.functionName;
			}

			return stream.str();
		}
	};
}
