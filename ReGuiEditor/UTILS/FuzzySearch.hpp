#pragma once
// Subsequence fuzzy matching (fzf-style). C++17, header-only.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fuzzy {

struct Match {
	std::size_t index;  // index into the candidate list
	int         score;  // higher is better
};

namespace detail {

inline char lower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

// True if text[i] starts a "word": string start, after a non-alnum, or camelCase hump.
inline bool isBoundary(std::string_view text, std::size_t i) {
	if (i == 0) return true;
	const auto prev = static_cast<unsigned char>(text[i - 1]);
	const auto cur  = static_cast<unsigned char>(text[i]);
	return !std::isalnum(prev) || (std::islower(prev) && std::isupper(cur));
}

}  // namespace detail

// Returns a score if every char of `pattern` appears in `text` in order
// (case-insensitive), otherwise std::nullopt. An empty pattern matches everything with score 0.
// O(|text|).
inline std::optional<int> score(std::string_view pattern, std::string_view text) {
	using detail::lower;
	if (pattern.empty()) return 0;
	if (pattern.size() > text.size()) return std::nullopt;

	// 1) Forward pass: find where the earliest greedy match ends.
	std::size_t p = 0, end = 0;
	for (std::size_t t = 0; t < text.size(); ++t) {
		if (lower(text[t]) == lower(pattern[p]) && ++p == pattern.size()) {
			end = t;
			break;
		}
	}
	if (p != pattern.size()) return std::nullopt;

	// 2) Backward pass: tighten the window to the latest possible start.
	std::size_t start = end;
	p = pattern.size();
	for (std::size_t t = end + 1; t-- > 0;) {
		if (lower(text[t]) == lower(pattern[p - 1]) && --p == 0) {
			start = t;
			break;
		}
	}

	// 3) Score the matched window.
	constexpr int kMatch = 1, kExactCase = 1, kBoundary = 8, kConsecutive = 5;
	constexpr int kMaxGapPenalty = 3, kMaxLeadPenalty = 5;

	int         s    = 0;
	std::size_t prev = 0;
	p = 0;
	for (std::size_t t = start; p < pattern.size(); ++t) {
		if (lower(text[t]) != lower(pattern[p])) continue;

		s += kMatch;
		if (text[t] == pattern[p]) s += kExactCase;
		if (detail::isBoundary(text, t)) s += kBoundary;
		if (p > 0) {
			const std::size_t gap = t - prev - 1;
			s += gap == 0 ? kConsecutive : -static_cast<int>(std::min<std::size_t>(gap, kMaxGapPenalty));
		}
		prev = t;
		++p;
	}
	s -= static_cast<int>(std::min<std::size_t>(start, kMaxLeadPenalty));
	return s;
}

// Ranks `candidates` against `pattern`: best score first, ties broken by
// shorter string, then original order. Non-matches are dropped.
inline std::vector<Match> fuzzySearch(std::string_view pattern,
                                      const std::vector<std::string>& candidates) {
	std::vector<Match> results;
	for (std::size_t i = 0; i < candidates.size(); ++i) {
		if (auto sc = score(pattern, candidates[i])) results.push_back({i, *sc});
	}
	std::stable_sort(results.begin(), results.end(), [&](const Match& a, const Match& b) {
		if (a.score != b.score) return a.score > b.score;
		return candidates[a.index].size() < candidates[b.index].size();
	});
	return results;
}

}  // namespace fuzzy
