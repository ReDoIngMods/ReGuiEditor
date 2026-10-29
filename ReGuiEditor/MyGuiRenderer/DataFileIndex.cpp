#include "DataFileIndex.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

[[nodiscard]] static bool matchWildcard(const std::string& pattern, const std::string& str) {
	size_t patternIndex = 0;
	size_t stringIndex = 0;

	size_t starPatternIndex = std::string::npos;
	size_t starStringIndex = 0;

	while (stringIndex < str.size()) {
		const bool patternMatches = patternIndex < pattern.size() && (pattern[patternIndex] == str[stringIndex] || pattern[patternIndex] == '?');
		if (patternMatches) {
			++patternIndex;
			++stringIndex;
		} else if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
			starPatternIndex = patternIndex++;
			starStringIndex = stringIndex;
		} else if (starPatternIndex != std::string::npos) {
			patternIndex = starPatternIndex + 1;
			stringIndex = ++starStringIndex;
		} else {
			return false;
		}
	}

	while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
		++patternIndex;

	return patternIndex == pattern.size();
}

class PatternMatcher {
	std::string pattern_;

	bool hasPathSeparators_;
	bool hasWildcard_;
public:
	explicit PatternMatcher(const std::string& pattern) : pattern_(pattern) {
		std::replace(this->pattern_.begin(), this->pattern_.end(), '\\', '/');

		this->hasPathSeparators_ = this->pattern_.find('/') != std::string::npos;
		this->hasWildcard_ = this->pattern_.find_first_of("*?") != std::string::npos;
	}

	[[nodiscard]] bool matches(const std::string& name, const std::string& relativePath) const {
		if (this->pattern_.empty())
			return true;

		const std::string& subject = this->hasPathSeparators_ ? relativePath : name;
		return this->hasWildcard_ ? matchWildcard(this->pattern_, subject) : (this->pattern_ == subject);
	}
};

template<typename Iterator, typename Visitor>
void forEachFile(const std::filesystem::path& basePath, Visitor&& visit) {
	std::error_code errorCode;
	Iterator iterator(basePath, std::filesystem::directory_options::skip_permission_denied, errorCode);

	const Iterator end{};
	while (!errorCode && iterator != end) {
		const std::filesystem::directory_entry& entry = *iterator;

		std::error_code entryError;
		if (!entry.is_directory(entryError) && !entryError)
			visit(entry);

		iterator.increment(errorCode);
	}
}

void DataFileIndex::update(const std::vector<SearchFolder>& folders, bool forceRebuild) {
	bool matchesCurrent = !forceRebuild && (this->directories_.size() == folders.size());
	if (matchesCurrent) {
		for (size_t index = 0; index < folders.size(); ++index) {
			if (this->directories_[index].configuredPath == folders[index].configuredPath)
				continue;

			matchesCurrent = false;
			break;
		}
	}

	if (this->initialized_ && matchesCurrent)
		return;

	this->directories_.clear();
	this->directories_.resize(folders.size());

	for (size_t index = 0; index < folders.size(); ++index) {
		const SearchFolder& folder = folders[index];

		Directory& directory = this->directories_[index];
		directory.configuredPath = folder.configuredPath;

		if (folder.resolvedPath.empty())
			continue;

		const std::filesystem::path basePath(folder.resolvedPath);
		const auto addFile = [&directory, &basePath](const std::filesystem::directory_entry& entry) {
			Entry file;
			file.name = entry.path().filename().string();
			file.relativePath = entry.path().lexically_relative(basePath).generic_string();

			directory.files.push_back(std::move(file));
		};

		if (folder.recursive)
			forEachFile<std::filesystem::recursive_directory_iterator>(basePath, addFile);
		else
			forEachFile<std::filesystem::directory_iterator>(basePath, addFile);
	}

	this->initialized_ = true;
}

bool DataFileIndex::findFirst(const std::string& pattern, std::string& path) const {
	const PatternMatcher matcher(pattern);

	for (const Directory& directory : this->directories_) {
		for (const Entry& entry : directory.files) {
			if (!matcher.matches(entry.name, entry.relativePath))
				continue;

			path = (std::filesystem::path(directory.configuredPath) / entry.relativePath).string();
			return true;
		}
	}

	return false;
}

void DataFileIndex::collectNames(const std::string& pattern, std::vector<std::string>& names) const {
	const PatternMatcher matcher(pattern);

	for (const Directory& directory : this->directories_) {
		for (const Entry& entry : directory.files) {
			if (matcher.matches(entry.name, entry.relativePath))
				names.push_back(entry.name);
		}
	}
}
