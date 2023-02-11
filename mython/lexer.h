#pragma once

 #include <utility>
#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <deque>
#include <vector>
#include <cassert>
#include <set>

namespace parse {

namespace token_type {
struct Number {  // Лексема «число»
    int value;   // число
};

struct Id {             // Лексема «идентификатор»
    std::string value;  // Имя идентификатора
};

struct Char {    // Лексема «символ»
    char value;  // код символа
};

struct String {  // Лексема «строковая константа»
    std::string value;
};

struct Class {};    // Лексема «class»
struct Return {};   // Лексема «return»
struct If {};       // Лексема «if»
struct Else {};     // Лексема «else»
struct Def {};      // Лексема «def»
struct Newline {};  // Лексема «конец строки»
struct Print {};    // Лексема «print»
struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};  // Лексема «уменьшение отступа»
struct Eof {};     // Лексема «конец файла»
struct And {};     // Лексема «and»
struct Or {};      // Лексема «or»
struct Not {};     // Лексема «not»
struct Eq {};      // Лексема «==»
struct NotEq {};   // Лексема «!=»
struct LessOrEq {};     // Лексема «<=»
struct GreaterOrEq {};  // Лексема «>=»
struct None {};         // Лексема «None»
struct True {};         // Лексема «True»
struct False {};        // Лексема «False»

const static std::set<char> Special_Char {'=','.',',','(',')','+','>','<','-','*','/',':', '!'};

const static std::set<std::string> KeyWords {"Class", "Return", "If", "Else", "Def", "Newline", "Print",
                               "Indent", "Dedent", "And", "Or", "Not", "Eq", "NotEq", "LessOrEq",
                               "GreaterOrEq", "None", "True", "False", "Eof"};

const static std::set<char> EscapeSequence {'n', 't'};

}  // namespace token_type

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::istream& input);

    void SplitStream(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const {
        using namespace std::literals;

        if (CurrentToken().Is<T>()){
            return CurrentToken().As<T>();
        }

        throw LexerError("parse::Expect is not valid"s);

    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U& value) const {
        using namespace std::literals;

        if constexpr(std::is_same <T, token_type::Number>() && std::is_same <U, int>()){
            const token_type::Number& token = Expect<T>();
            if(token.value == value){
                return;
            }
        }else if constexpr(std::is_same <T, token_type::Id>() && std::is_same <U, std::string>()){
            const token_type::Id& token = Expect<T>();
            if(token.value == value){
                return;
            }
        }else if constexpr(std::is_same <T, token_type::Char>() && std::is_same <U, char>()){
            const token_type::Char& token = Expect<T>();
            if(token.value == value){
                return;
            }
        }else if constexpr(std::is_same <T, token_type::String>() && std::is_same <U, std::string>()){
            const token_type::String& token = Expect<T>();
            if(token.value == value){
                return;
            }
        }

        throw LexerError("Not implemented"s);
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext() {
        using namespace std::literals;
        [[maybe_unused]] const Token token = NextToken();

        return Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U& value) {
        using namespace std::literals;

        [[maybe_unused]] const Token token = NextToken();

        return Expect<T>(value);
    }

private:
    std::vector<Token> tokens;
    size_t index_current_token = 0;
};

}  // namespace parse
