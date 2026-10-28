#include "ImGuiCustomInputText.hpp" // VCIC-Excluded

#include <ImGui/imgui.h>

#include <string>

struct CustomInputTextCallbackData {
	std::string* str = nullptr;
	ImGuiInputTextCallback chainCallback = nullptr;
	void* chainUserData = nullptr;
};

static int CustomInputTextCallback(ImGuiInputTextCallbackData* data) {
	CustomInputTextCallbackData* customData = reinterpret_cast<CustomInputTextCallbackData*>(data->UserData);
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		IM_ASSERT(data->Buf == customData->str->c_str());

		customData->str->resize(data->BufSize);
		data->Buf = const_cast<char*>(customData->str->c_str());
	} else if (customData->chainCallback) {
		data->UserData = customData->chainUserData;
		return customData->chainCallback(data);
	}

	return 0;
}

bool ImGui::InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	CustomInputTextCallbackData customData;
	customData.str = str;
	customData.chainCallback = callback;
	customData.chainUserData = user_data;

	return ImGui::InputText(label, const_cast<char*>(str->c_str()), str->capacity() + 1, flags, CustomInputTextCallback, &customData);
}

bool ImGui::InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	CustomInputTextCallbackData customData;
	customData.str = str;
	customData.chainCallback = callback;
	customData.chainUserData = user_data;

	return ImGui::InputTextMultiline(label, const_cast<char*>(str->c_str()), str->capacity() + 1, size, flags, CustomInputTextCallback, &customData);
}

bool ImGui::InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	CustomInputTextCallbackData customData;
	customData.str = str;
	customData.chainCallback = callback;
	customData.chainUserData = user_data;

	return ImGui::InputTextWithHint(label, hint, const_cast<char*>(str->c_str()), str->capacity() + 1, flags, CustomInputTextCallback, &customData);
}
