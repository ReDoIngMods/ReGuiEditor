#include "FuzzySearch.hpp" // VCIC-Excluded

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr int SCORE_POINTS_MATCH       = 1; // Exact match of a character in the pattern
constexpr int SCORE_POINTS_EXACT_CASE  = 1; // Exact case match of a character in the pattern
constexpr int SCORE_POINTS_BOUNDARY    = 8; // Match at a word boundary (e.g., after a space, underscore, or hyphen)
constexpr int SCORE_POINTS_CONSECUTIVE = 5; // Consecutive matches in the pattern (e.g., "abc" in "abcdef")

constexpr int SCORE_PENALTY_MAX_GAP = 3; // Maximum penalty for gaps between matched characters in the pattern
constexpr int SCORE_PENALTY_GAP     = 1; // Penalty for each gap between matched characters in the pattern

inline static char lowerCharacter(char character) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

std::optional<int> ReGuiEditor::fuzzyCalculateScore(std::string_view pattern, std::string_view text) {
	if (pattern.empty())
		return 0;

	if (pattern.size() > text.size())
		return std::nullopt;

	size_t patternIndex = 0;
	size_t endIndex = 0;
	for (size_t textIndex = 0; textIndex < text.size(); ++textIndex) {
		if (lowerCharacter(text[textIndex]) != lowerCharacter(pattern[patternIndex]))
			continue;

		++patternIndex;
		if (patternIndex == pattern.size()) {
			endIndex = textIndex;
			break;
		}
	}

	if (patternIndex != pattern.size())
		return std::nullopt;

	size_t startIndex = endIndex;
	patternIndex = pattern.size();

	for (size_t textIndex = endIndex + 1; textIndex-- > 0;) {
		if (lowerCharacter(text[textIndex]) != lowerCharacter(pattern[patternIndex - 1]))
			continue;

		--patternIndex;
		if (patternIndex == 0) {
			startIndex = textIndex;
			break;
		}
	}

	int score = 0;
	size_t previousMatchIndex = 0;
	patternIndex = 0;

	for (size_t textIndex = startIndex; patternIndex < pattern.size(); ++textIndex) {
		if (lowerCharacter(text[textIndex]) != lowerCharacter(pattern[patternIndex]))
			continue;

		score += SCORE_POINTS_MATCH;
		if (text[textIndex] == pattern[patternIndex])
			score += SCORE_POINTS_EXACT_CASE;
	
		if (textIndex != 0) {
			const char previousChar = text[textIndex - 1];
			const char currentChar = text[textIndex];

			if (!std::isalnum(static_cast<unsigned char>(previousChar)) || (std::islower(static_cast<unsigned char>(previousChar)) && std::isupper(static_cast<unsigned char>(currentChar))))
				score += SCORE_POINTS_BOUNDARY;
		}

		if (patternIndex > 0) {
			const size_t gap = textIndex - previousMatchIndex - 1;
			score += gap == 0 ? SCORE_POINTS_CONSECUTIVE : -static_cast<int>(std::min<size_t>(gap, SCORE_PENALTY_MAX_GAP));
		}

		previousMatchIndex = textIndex;
		++patternIndex;
	}

	score -= static_cast<int>(std::min<size_t>(startIndex, SCORE_PENALTY_MAX_GAP));
	return score;
}

std::vector<ReGuiEditor::FuzzyMatch> ReGuiEditor::fuzzySearch(std::string_view pattern, const std::vector<std::string>& candidates) {
	std::vector<ReGuiEditor::FuzzyMatch> matches;
	for (size_t index = 0; index < candidates.size(); ++index) {
		if (auto score = fuzzyCalculateScore(pattern, candidates[index]))
			matches.emplace_back(index, score.value());
	}

	std::stable_sort(matches.begin(), matches.end(), [&](const ReGuiEditor::FuzzyMatch& a, const ReGuiEditor::FuzzyMatch& b) -> bool {
		if (a.score != b.score)
			return a.score > b.score;

		return candidates[a.index].size() < candidates[b.index].size();
	});

	return matches;
}