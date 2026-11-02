#pragma once
#include "LuaScriptError.hpp"

#include <Modules/Misc/Strings.hpp>

#include <cstdarg>
#include <string>
#include <utility>

namespace ReGuiEditor {
	struct LuaResult {
		bool success = true;
		LuaScriptError error;

		explicit operator bool() const { return this->success; }

		static LuaResult generateSuccess() { return {}; }

		static LuaResult generateFailure(LuaScriptError error) {
			LuaResult result;
			result.success = false;
			result.error = std::move(error);

			return result;
		}

		static LuaResult generateFailure(std::string message) {
			LuaScriptError error;
			error.message = std::move(message);

			return LuaResult::generateFailure(std::move(error));
		}

		static LuaResult generateFailure(const char* format, ...) {
			va_list arguments;
			va_start(arguments, format);

			std::string message = ReGuiEditor::formatStringV(format, arguments);

			va_end(arguments);

			return LuaResult::generateFailure(std::move(message));
		}

		void report() const {
			if (this->success)
				return;
		
			CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s", this->error.message.c_str());
			if (!this->error.frames.empty())
				CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "%s", this->error.formatTraceback().c_str());
		}
	};
}