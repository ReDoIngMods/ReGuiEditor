#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ReGuiEditor {
	struct FuzzyMatch {
		size_t index;
		int score;
	};

	[[nodiscard]] std::optional<int> fuzzyCalculateScore(std::string_view pattern, std::string_view text);
	[[nodiscard]] std::vector<FuzzyMatch> fuzzySearch(std::string_view pattern, const std::vector<std::string>& candidates);
}