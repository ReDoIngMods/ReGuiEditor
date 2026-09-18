#include "MyGuiDataStream.hpp"

#include <MyGui/MyGUI_Types.h>

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

MyGuiDataStream::MyGuiDataStream(const std::filesystem::path& filePath) {
	this->hFile_ = CreateFileW(filePath.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (this->hFile_ == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Failed to open file: " + filePath.string());

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(this->hFile_, &fileSize))
		throw std::runtime_error("Failed to get file size: " + filePath.string());

	this->fileSize_ = static_cast<size_t>(fileSize.QuadPart);

	this->hMapping_ = CreateFileMappingW(this->hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!this->hMapping_)
		throw std::runtime_error("Failed to create file mapping: " + filePath.string());

	this->pMapView_ = MapViewOfFile(this->hMapping_, FILE_MAP_READ, 0, 0, 0);
	if (!this->pMapView_)
		throw std::runtime_error("Failed to map view of file: " + filePath.string());
}

MyGuiDataStream::~MyGuiDataStream() {
	if (this->pMapView_) {
		UnmapViewOfFile(this->pMapView_);
		this->pMapView_ = nullptr;
	}

	if (this->hMapping_) {
		CloseHandle(this->hMapping_);
		this->hMapping_ = nullptr;
	}

	if (this->hFile_ != INVALID_HANDLE_VALUE) {
		CloseHandle(this->hFile_);
		this->hFile_ = INVALID_HANDLE_VALUE;
	}
}

bool MyGuiDataStream::eof() {
	return this->currentPosition_ >= this->fileSize_;
}

size_t MyGuiDataStream::size() {
	return this->fileSize_;
}

void MyGuiDataStream::readline(std::string& _source, MyGUI::Char _delim) {
	_source.clear();
	if (this->eof())
		return;

	const char* data = static_cast<const char*>(this->pMapView_);
	while (this->currentPosition_ < this->fileSize_) {
		const char character = data[this->currentPosition_++];
		if (character == _delim)
			break;

		_source += character;
	}
}

size_t MyGuiDataStream::read(void* _buf, size_t _count) {
	if (this->eof())
		return 0;

	size_t bytesToRead = std::min(_count, this->fileSize_ - this->currentPosition_);
	memcpy(_buf, static_cast<const char*>(this->pMapView_) + this->currentPosition_, bytesToRead);

	this->currentPosition_ += bytesToRead;
	return bytesToRead;
}