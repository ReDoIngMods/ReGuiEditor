#pragma once

#include <string>
#include <vector>

struct LuaStackFrame {
	std::string source;   // Shortened chunk name, e.g. "...Application/Loader.lua" or "[C]".
	std::string function; // Empty when Lua does not know the name.
	std::string what;     // "Lua", "C", "main" or "tail".
	int line = -1;        // -1 when there is no line information (C functions).
};

struct LuaError {
	static constexpr const char* kUnknownMessage = "unknown error";

	std::string message = kUnknownMessage;
	std::vector<LuaStackFrame> frames;

	void reset() {
		this->message = kUnknownMessage;
		this->frames.clear();
	}

	void setMessage(const char* message, const char* fallback = nullptr) {
		this->message = message ? message : (fallback ? fallback : kUnknownMessage);
	}

	[[nodiscard]] std::string formatTraceback() const {
		std::string trace = "--- Lua Stack Traceback ---";
		for (const LuaStackFrame& frame : this->frames) {
			trace += "\n\t" + frame.source;
			if (frame.line != -1)
				trace += ":" + std::to_string(frame.line) + ":";

			if (!frame.function.empty())
				trace += " in " + frame.function;
		}

		return trace;
	}
};
