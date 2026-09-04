#include "parser.h"

#include <cctype>

namespace regui::format {

namespace {

enum class Kind {
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null_,
    End,
};

struct Token {
    Kind kind = Kind::End;
    std::string text;   // decoded string literal content
    double number = 0.0;
    std::vector<Comment> leadingComments;
    size_t line = 1;
    size_t col = 1;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : mSrc(src) {}

    Token next() {
        std::vector<Comment> comments;
        skipWhitespaceAndComments(comments);

        Token tok;
        tok.leadingComments = std::move(comments);
        tok.line = mLine;
        tok.col = mCol;

        if (atEnd()) {
            tok.kind = Kind::End;
            return tok;
        }

        char c = peek();
        switch (c) {
            case '{':
                advanceChar();
                tok.kind = Kind::LBrace;
                return tok;
            case '}':
                advanceChar();
                tok.kind = Kind::RBrace;
                return tok;
            case '[':
                advanceChar();
                tok.kind = Kind::LBracket;
                return tok;
            case ']':
                advanceChar();
                tok.kind = Kind::RBracket;
                return tok;
            case ':':
                advanceChar();
                tok.kind = Kind::Colon;
                return tok;
            case ',':
                advanceChar();
                tok.kind = Kind::Comma;
                return tok;
            case '"':
                lexString(tok);
                return tok;
            case 't':
                lexKeyword(tok, "true", Kind::True);
                return tok;
            case 'f':
                lexKeyword(tok, "false", Kind::False);
                return tok;
            case 'n':
                lexKeyword(tok, "null", Kind::Null_);
                return tok;
            default:
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    lexNumber(tok);
                    return tok;
                }
                throw ParseError(std::string("Unexpected character '") + c + "'", mLine, mCol);
        }
    }

private:
    const std::string& mSrc;
    size_t mPos = 0;
    size_t mLine = 1;
    size_t mCol = 1;

    bool atEnd() const { return mPos >= mSrc.size(); }
    char peek() const { return mSrc[mPos]; }
    char peekAt(size_t offset) const {
        size_t p = mPos + offset;
        return p < mSrc.size() ? mSrc[p] : '\0';
    }

    char advanceChar() {
        char c = mSrc[mPos++];
        if (c == '\n') {
            ++mLine;
            mCol = 1;
        } else {
            ++mCol;
        }
        return c;
    }

    void skipWhitespaceAndComments(std::vector<Comment>& comments) {
        while (true) {
            while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
                advanceChar();
            }
            if (atEnd()) {
                return;
            }
            if (peek() == '/' && peekAt(1) == '/') {
                size_t startLine = mLine;
                advanceChar();
                advanceChar();
                std::string text;
                while (!atEnd() && peek() != '\n') {
                    text += advanceChar();
                }
                if (!text.empty() && text.back() == '\r') {
                    text.pop_back();
                }
                comments.push_back(Comment{std::move(text), true, startLine});
                continue;
            }
            if (peek() == '/' && peekAt(1) == '*') {
                size_t startLine = mLine;
                size_t startCol = mCol;
                advanceChar();
                advanceChar();
                std::string text;
                while (true) {
                    if (atEnd()) {
                        throw ParseError("Unterminated block comment", startLine, startCol);
                    }
                    if (peek() == '*' && peekAt(1) == '/') {
                        advanceChar();
                        advanceChar();
                        break;
                    }
                    text += advanceChar();
                }
                comments.push_back(Comment{std::move(text), false, startLine});
                continue;
            }
            return;
        }
    }

    void lexKeyword(Token& tok, const char* keyword, Kind kind) {
        size_t line = mLine;
        size_t col = mCol;
        for (const char* p = keyword; *p != '\0'; ++p) {
            if (atEnd() || peek() != *p) {
                throw ParseError(std::string("Invalid literal, expected '") + keyword + "'", line, col);
            }
            advanceChar();
        }
        tok.kind = kind;
    }

    void lexNumber(Token& tok) {
        std::string text;
        if (peek() == '-') {
            text += advanceChar();
        }
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            text += advanceChar();
        }
        if (!atEnd() && peek() == '.') {
            text += advanceChar();
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                text += advanceChar();
            }
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            text += advanceChar();
            if (!atEnd() && (peek() == '+' || peek() == '-')) {
                text += advanceChar();
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                text += advanceChar();
            }
        }
        try {
            tok.number = std::stod(text);
        } catch (const std::exception&) {
            throw ParseError("Invalid number literal '" + text + "'", tok.line, tok.col);
        }
        tok.kind = Kind::Number;
    }

    void lexString(Token& tok) {
        size_t line = mLine;
        size_t col = mCol;
        advanceChar(); // opening quote
        std::string text;
        while (true) {
            if (atEnd()) {
                throw ParseError("Unterminated string literal", line, col);
            }
            char c = advanceChar();
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                if (atEnd()) {
                    throw ParseError("Unterminated string literal", line, col);
                }
                char esc = advanceChar();
                switch (esc) {
                    case '"':
                        text += '"';
                        break;
                    case '\\':
                        text += '\\';
                        break;
                    case '/':
                        text += '/';
                        break;
                    case 'b':
                        text += '\b';
                        break;
                    case 'f':
                        text += '\f';
                        break;
                    case 'n':
                        text += '\n';
                        break;
                    case 'r':
                        text += '\r';
                        break;
                    case 't':
                        text += '\t';
                        break;
                    case 'u':
                        appendUnicodeEscape(text, line, col);
                        break;
                    default:
                        throw ParseError(std::string("Invalid escape sequence '\\") + esc + "'", line, col);
                }
                continue;
            }
            text += c;
        }
        tok.kind = Kind::String;
        tok.text = std::move(text);
    }

    void appendUnicodeEscape(std::string& out, size_t line, size_t col) {
        unsigned int codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            if (atEnd()) {
                throw ParseError("Invalid \\u escape", line, col);
            }
            char c = advanceChar();
            codepoint <<= 4;
            if (c >= '0' && c <= '9') {
                codepoint |= static_cast<unsigned int>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                codepoint |= static_cast<unsigned int>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                codepoint |= static_cast<unsigned int>(c - 'A' + 10);
            } else {
                throw ParseError("Invalid \\u escape", line, col);
            }
        }
        // Encode as UTF-8; surrogate pairs aren't handled (not needed here).
        if (codepoint < 0x80) {
            out += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            out += static_cast<char>(0xC0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
};

// Splits comments starting on `line` (trailing the value before it) from the rest (leading the next value).
std::pair<std::vector<Comment>, std::vector<Comment>> splitByLine(std::vector<Comment> comments, size_t line) {
    std::vector<Comment> sameLine, rest;
    for (auto& c : comments) {
        if (c.line == line) {
            sameLine.push_back(std::move(c));
        } else {
            rest.push_back(std::move(c));
        }
    }
    return {std::move(sameLine), std::move(rest)};
}

class Parser {
public:
    explicit Parser(const std::string& text) : mLexer(text) { mCurrent = mLexer.next(); }

    JsonValue parseDocument() {
        std::vector<Comment> rootLeading = mCurrent.leadingComments;
        JsonValue root = parseValue();
        if (root.leadingComments.empty()) {
            root.leadingComments = std::move(rootLeading);
        }
        if (mCurrent.kind != Kind::End) {
            throw ParseError("Unexpected trailing content after document", mCurrent.line, mCurrent.col);
        }
        return root;
    }

private:
    Lexer mLexer;
    Token mCurrent;

    void advance() { mCurrent = mLexer.next(); }

    JsonValue parseValue() {
        switch (mCurrent.kind) {
            case Kind::LBrace:
                return parseObject();
            case Kind::LBracket:
                return parseArray();
            case Kind::String:
                return finishPrimitive(JsonValue::makeString(mCurrent.text));
            case Kind::Number:
                return finishPrimitive(JsonValue::makeNumber(mCurrent.number));
            case Kind::True:
                return finishPrimitive(JsonValue::makeBool(true));
            case Kind::False:
                return finishPrimitive(JsonValue::makeBool(false));
            case Kind::Null_:
                return finishPrimitive(JsonValue::makeNull());
            default:
                throw ParseError("Expected a value", mCurrent.line, mCurrent.col);
        }
    }

    // Consumes the current primitive token, claiming same-line comments as trailing.
    JsonValue finishPrimitive(JsonValue value) {
        size_t valueLine = mCurrent.line;
        advance();
        auto [sameLine, rest] = splitByLine(std::move(mCurrent.leadingComments), valueLine);
        value.trailingComments = std::move(sameLine);
        mCurrent.leadingComments = std::move(rest);
        return value;
    }

    // Claims same-line comments after a comma as trailing comments of `target`.
    void claimCommentsAfterComma(JsonValue& target, size_t commaLine) {
        auto [sameLine, rest] = splitByLine(std::move(mCurrent.leadingComments), commaLine);
        for (auto& c : sameLine) {
            target.trailingComments.push_back(std::move(c));
        }
        mCurrent.leadingComments = std::move(rest);
    }

    JsonValue parseObject() {
        JsonValue obj = JsonValue::makeObject();
        advance(); // consume '{'

        std::vector<Comment> pendingLeading = std::move(mCurrent.leadingComments);
        mCurrent.leadingComments.clear();

        if (mCurrent.kind != Kind::RBrace) {
            while (true) {
                if (mCurrent.kind != Kind::String) {
                    throw ParseError("Expected object key string", mCurrent.line, mCurrent.col);
                }
                std::string key = mCurrent.text;
                advance(); // consume key
                if (mCurrent.kind != Kind::Colon) {
                    throw ParseError("Expected ':' after object key", mCurrent.line, mCurrent.col);
                }
                advance(); // consume ':'

                JsonValue value = parseValue();
                value.leadingComments = std::move(pendingLeading);
                pendingLeading.clear();
                obj.objectFields.emplace_back(std::move(key), std::move(value));

                if (mCurrent.kind == Kind::Comma) {
                    size_t commaLine = mCurrent.line;
                    advance();
                    claimCommentsAfterComma(obj.objectFields.back().second, commaLine);
                    pendingLeading = std::move(mCurrent.leadingComments);
                    mCurrent.leadingComments.clear();
                    if (mCurrent.kind == Kind::RBrace) {
                        break; // trailing comma
                    }
                    continue;
                }
                if (mCurrent.kind == Kind::RBrace) {
                    break;
                }
                throw ParseError("Expected ',' or '}' in object", mCurrent.line, mCurrent.col);
            }
        }

        size_t closeLine = mCurrent.line; // still '}'
        advance();
        auto [sameLine, rest] = splitByLine(std::move(mCurrent.leadingComments), closeLine);
        obj.trailingComments = std::move(sameLine);
        mCurrent.leadingComments = std::move(rest);
        return obj;
    }

    JsonValue parseArray() {
        JsonValue arr = JsonValue::makeArray();
        advance(); // consume '['

        std::vector<Comment> pendingLeading = std::move(mCurrent.leadingComments);
        mCurrent.leadingComments.clear();

        if (mCurrent.kind != Kind::RBracket) {
            while (true) {
                JsonValue value = parseValue();
                value.leadingComments = std::move(pendingLeading);
                pendingLeading.clear();
                arr.arrayItems.push_back(std::move(value));

                if (mCurrent.kind == Kind::Comma) {
                    size_t commaLine = mCurrent.line;
                    advance();
                    claimCommentsAfterComma(arr.arrayItems.back(), commaLine);
                    pendingLeading = std::move(mCurrent.leadingComments);
                    mCurrent.leadingComments.clear();
                    if (mCurrent.kind == Kind::RBracket) {
                        break; // trailing comma
                    }
                    continue;
                }
                if (mCurrent.kind == Kind::RBracket) {
                    break;
                }
                throw ParseError("Expected ',' or ']' in array", mCurrent.line, mCurrent.col);
            }
        }

        size_t closeLine = mCurrent.line; // still ']'
        advance();
        auto [sameLine, rest] = splitByLine(std::move(mCurrent.leadingComments), closeLine);
        arr.trailingComments = std::move(sameLine);
        mCurrent.leadingComments = std::move(rest);
        return arr;
    }
};

} // namespace

JsonValue parseDocument(const std::string& text) {
    Parser parser(text);
    return parser.parseDocument();
}

} // namespace regui::format
