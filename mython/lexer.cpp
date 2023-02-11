#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

namespace detail {

bool CheckSpecialChar(char ch){
    return token_type::Special_Char.count(ch);
}

bool CheckKeyWord(std::string str){
    return token_type::KeyWords.count(str);
}

bool CheckEscapeSequence(char ch){
    return token_type::EscapeSequence.count(ch);
}

// Функция парсит строку в кавычках "str" или 'str'
std::string SplitAsStringInQuotes(std::istream& input, char quotes){
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    bool first_quote = false;
    while (true) {
        if (it == end) {
            throw logic_error("String parsing error");
        }
        const char ch = *it;
        if (ch == quotes && first_quote) {
            ++it;
            break;
        }else if(ch == quotes && first_quote == false){
            first_quote = true;
        }
        else if (ch == '\\') {
            ++it;
            if (it == end) {
                throw logic_error("String parsing error");
            }
            const char escaped_char = *(it);
            switch (escaped_char) {
            case 'n':
                s.push_back('\n');
                break;
            case 't':
                s.push_back('\t');
                break;
            case '"':
                s.push_back('"');
                break;
            case '\'':
                s.push_back('\'');
                break;
            default:
                throw logic_error("Unrecognized escape sequence \\"s + escaped_char);
            }
        }
        else if (ch == '\n' || ch == '\r') {
            throw logic_error("Unexpected end of line"s);
        }
        else {
            s.push_back(ch);
        }
        ++it;
    }

    return s;
}

std::string SplitAsStringWithoutQuotes(std::istream& input ){
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            break;
        }
        const char ch = *it;
        if (ch == ' ' || ch == '\n' || CheckSpecialChar(ch) || ch == '#') {
            break;
        }

        if(CheckKeyWord(s)){
            break;
        }

        else {
            s.push_back(ch);
        }
        ++it;
    }

    return s;
}

Token DefineToken(std::string_view str){

    if(str == "class"){
        return token_type::Class();
    }else if(str == "return"){
        return token_type::Return();
    }else if(str == "if"){
        return token_type::If();
    }else if(str == "else"){
        return token_type::Else();
    }else if(str == "def"){
        return token_type::Def();
    }else if(str == "\n"){
        return token_type::Newline();
    }else if(str == "print"){
        return token_type::Print();
    }else if(str == "and"){
        return token_type::And();
    }else if(str == "or"){
        return token_type::Or();
    }else if(str == "not"){
        return token_type::Not();
    }else if(str == "=="){
        return token_type::Eq();
    }else if(str == "!="){
        return token_type::NotEq();
    }else if(str == "<="){
        return token_type::LessOrEq();
    }else if(str == ">="){
        return token_type::GreaterOrEq();
    }else if(str == "None"){
        return token_type::None();
    }else if(str == "True"){
        return token_type::True();
    }else if(str == "False"){
        return token_type::False();
    }

    token_type::Id id;
    id.value = str;
    return id;
}

bool CheckSpecialCharCombination(char current_ch, char next_char){
    std::string check_str;
    check_str += current_ch;
    check_str += next_char;
    std::set<std::string> set_special_char = {"==","!=","<=",">="};
    return set_special_char.count(check_str);
}

int SplitAsNumber(std::istream& input) {
    std::string parsed_num;

    // Считывает в parsed_num очередной символ из input
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw LexerError("Failed to read number from stream"s);
        }
    };

    // Считывает одну или более цифр в parsed_num из input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw LexerError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }
    // Парсим целую часть числа
    if (input.peek() == '0') {
        read_char();
    }
    else {
        read_digits();
    }

    // Парсим экспоненциальную часть числа
    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
    }

    // Пробуем преобразовать строку в int
    try {
        return std::stoi(parsed_num);
    }
    catch (...) {
        throw LexerError("Failed to convert "s + parsed_num + " to number"s);
    }
    return 0;
}

} // end namespace detail

Lexer::Lexer(std::istream& input) {
    SplitStream(input);
}

void Lexer::SplitStream(std::istream& input) {
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    bool not_space_exist = false;
    int prev_count_space = 0;
    int current_count_space = 0;
    bool stroke_not_empty = false;
    bool is_comment = false;

    while(true){
        if (it == end) {
            if(current_count_space < prev_count_space){
                const int count_space = prev_count_space - current_count_space;
                for(int i = 2; count_space >= i; ){
                    i += 2;
                    tokens.push_back(token_type::Dedent()) ;
                }
            }
            if(stroke_not_empty && tokens[tokens.size() - 1] != token_type::Newline()){
                tokens.push_back(token_type::Newline());
            }
            tokens.push_back(token_type::Eof());
            break;
        }
        const char c = *it;

        if(c == '#' && !is_comment){
            is_comment = true;
        }       

        if(is_comment && c != '\n'){
            ++it;
            continue;
        }else{
            is_comment = false;
        }

        if(c == ' '){
            if(!not_space_exist){
                ++current_count_space;
            }
            ++it;
            continue;
        }else{
            stroke_not_empty = true;
        }

        if(c == '\t'){
            if(!not_space_exist){
                current_count_space += 2;
            }
            ++it;
            continue;
        }

        if(c != '\n'){
            if(!not_space_exist ){
                if(current_count_space >= prev_count_space){
                    const int count_space = current_count_space - prev_count_space;
                    for(int i = 2; count_space >= i; ){
                        i += 2;
                        tokens.push_back(token_type::Indent()) ;
                    }
                }else{
                    const int count_space = prev_count_space - current_count_space;
                    for(int i = 2; count_space >= i; ){
                        i += 2;
                        tokens.push_back(token_type::Dedent()) ;
                    }
                }
                prev_count_space = current_count_space;
                current_count_space = 0;
            }
            not_space_exist = true;
        }

        if(c == '"' || c == '\'' ){
            if(it == end){
                // конец потока
                tokens.push_back(token_type::Eof());
                break;
            }
            token_type::String str;
            str.value = detail::SplitAsStringInQuotes(input, c);
            tokens.push_back(str) ;
            continue;
        }
        if(c == '\n'){
            stroke_not_empty = false;
            if(not_space_exist){
                not_space_exist = false;
                tokens.push_back(token_type::Newline()) ;
            }else{
                current_count_space = 0;
            }
            ++it;
            continue;
        }
        if(std::isdigit(c)){
            token_type::Number num;
            num.value = detail::SplitAsNumber(input);
            tokens.push_back(num);
            continue;
        }
        if(detail::CheckSpecialChar(c)){
            ++it;
            if(it == end){
                token_type::Char char_;
                char_.value = c;
                tokens.push_back(char_) ;

                continue;
            }
            char next_ch = *it;
            if(detail::CheckSpecialCharCombination(c,next_ch)){
                std::string str_;
                str_ += c;
                str_ += next_ch;
                auto token = detail::DefineToken(str_);
                tokens.push_back(token) ;
                ++it;
            }else{
                token_type::Char char_;
                char_.value = c;
                tokens.push_back(char_) ;

            }
            continue;
        }

        std::string str_ = detail::SplitAsStringWithoutQuotes(input);
        auto token = detail::DefineToken(str_);
        tokens.push_back(token) ;
    }

}

const Token& Lexer::CurrentToken() const {
    if(tokens.size() == 0 || index_current_token >= tokens.size() - 1){
        return tokens[index_current_token];
    }
    return tokens[index_current_token];
}

Token Lexer::NextToken() {
    if(tokens.size() == 0 || index_current_token >= tokens.size() - 1){
        return tokens[index_current_token];
    }
    return tokens[++index_current_token];
}

}  // namespace parse
