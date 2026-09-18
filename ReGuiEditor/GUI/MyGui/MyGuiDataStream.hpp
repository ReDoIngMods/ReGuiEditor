#pragma once
#include <MyGui/MyGUI_IDataStream.h>
#include <MyGui/MyGUI_Types.h>

#include <Windows.h>

#include <filesystem>
#include <string>

class MyGuiDataStream : public MyGUI::IDataStream {
	HANDLE hFile_ = INVALID_HANDLE_VALUE;
	HANDLE hMapping_ = nullptr;
	LPVOID pMapView_ = nullptr;

	size_t fileSize_ = 0;
	size_t currentPosition_ = 0;
public:
	MyGuiDataStream(const std::filesystem::path& filePath);
	~MyGuiDataStream();

	bool eof() override;
	size_t size() override;
	void readline(std::string& _source, MyGUI::Char _delim = '\n') override;
	size_t read(void* _buf, size_t _count) override;
};