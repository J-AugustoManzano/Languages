// lexer.cpp

#include "lexer.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace ecma55 {

namespace {

bool isAlphaChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isDigitChar(char c) {
    return c >= '0' && c <= '9';
}

char toUpperChar(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// As 24 keywords "puras" da secao 3.8 (GO, TO e SUB cobrem GOTO/GOSUB,
// ver a explicacao em token.hpp), mais as 11 funcoes da secao 9.2 e o
// TAB da secao 14.2. Chaves ja em maiusculas.
const std::unordered_map<std::string, TokenType>& reservedWords() {
    static const std::unordered_map<std::string, TokenType> table = {
        {"BASE", TokenType::KwBASE},
        {"DATA", TokenType::KwDATA},
        {"DEF", TokenType::KwDEF},
        {"DIM", TokenType::KwDIM},
        {"END", TokenType::KwEND},
        {"FOR", TokenType::KwFOR},
        {"GO", TokenType::KwGO},
        {"IF", TokenType::KwIF},
        {"INPUT", TokenType::KwINPUT},
        {"LET", TokenType::KwLET},
        {"NEXT", TokenType::KwNEXT},
        {"ON", TokenType::KwON},
        {"OPTION", TokenType::KwOPTION},
        {"PRINT", TokenType::KwPRINT},
        {"RANDOMIZE", TokenType::KwRANDOMIZE},
        {"READ", TokenType::KwREAD},
        {"REM", TokenType::KwREM},
        {"RESTORE", TokenType::KwRESTORE},
        {"RETURN", TokenType::KwRETURN},
        {"STEP", TokenType::KwSTEP},
        {"STOP", TokenType::KwSTOP},
        {"SUB", TokenType::KwSUB},
        {"THEN", TokenType::KwTHEN},
        {"TO", TokenType::KwTO},

        {"ABS", TokenType::FnABS},
        {"ATN", TokenType::FnATN},
        {"COS", TokenType::FnCOS},
        {"EXP", TokenType::FnEXP},
        {"INT", TokenType::FnINT},
        {"LOG", TokenType::FnLOG},
        {"RND", TokenType::FnRND},
        {"SGN", TokenType::FnSGN},
        {"SIN", TokenType::FnSIN},
        {"SQR", TokenType::FnSQR},
        {"TAN", TokenType::FnTAN},

        {"TAB", TokenType::KwTAB},
    };
    return table;
}

// As "keywords puras" da secao 3.8 exigem espaco antes e depois (secao
// 5.4). Funcoes embutidas e TAB ficam de fora dessa lista. A propria
// norma usa "SQR(X^2+Y^2)" e "TAB(10)" coladas ao parentese (secoes 8.3
// e 14.3).
bool isTrueKeyword(TokenType type) {
    switch (type) {
        case TokenType::KwBASE:
        case TokenType::KwDATA:
        case TokenType::KwDEF:
        case TokenType::KwDIM:
        case TokenType::KwEND:
        case TokenType::KwFOR:
        case TokenType::KwGO:
        case TokenType::KwIF:
        case TokenType::KwINPUT:
        case TokenType::KwLET:
        case TokenType::KwNEXT:
        case TokenType::KwON:
        case TokenType::KwOPTION:
        case TokenType::KwPRINT:
        case TokenType::KwRANDOMIZE:
        case TokenType::KwREAD:
        case TokenType::KwREM:
        case TokenType::KwRESTORE:
        case TokenType::KwRETURN:
        case TokenType::KwSTEP:
        case TokenType::KwSTOP:
        case TokenType::KwSUB:
        case TokenType::KwTHEN:
        case TokenType::KwTO:
            return true;
        default:
            return false;
    }
}

} // namespace

Lexer::Lexer(std::string source, Locale locale)
    : source_(std::move(source)), locale_(locale) {}

const char* Lexer::L(const char* pt, const char* en) const {
    return locale_ == Locale::Portuguese ? pt : en;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

char Lexer::peek(std::size_t offset) const {
    std::size_t i = pos_ + offset;
    return i < source_.size() ? source_[i] : '\0';
}

char Lexer::advance() {
    char c = source_[pos_++];
    ++column_;
    return c;
}

void Lexer::newLine() {
    char c = source_[pos_++];
    if (c == '\r' && !isAtEnd() && source_[pos_] == '\n') {
        ++pos_;
    }
    ++line_;
    column_ = 1;
    atLineStart_ = true;
    lineStartPos_ = pos_;
}

void Lexer::reportError(const std::string& message, int line, int column) {
    errors_.push_back(LexError{message, line, column});
}

int Lexer::skipSpaces() {
    int count = 0;
    for (;;) {
        char c = peek();
        if (c == ' ') {
            advance();
            ++count;
            continue;
        }
        if (c == '\t') {
            reportError(L("caractere de tabulacao nao faz parte do "
                          "conjunto de caracteres da norma (secao 4)",
                          "tab character is not part of the standard's "
                          "character set (section 4)"),
                        line_, column_);
            advance();
            ++count;
            continue;
        }
        break;
    }
    return count;
}

Token Lexer::lexNumber() {
    int startLine = line_;
    int startColumn = column_;
    std::string text;
    bool sawDigit = false;

    while (isDigitChar(peek())) {
        text += advance();
        sawDigit = true;
    }
    if (peek() == '.') {
        text += advance();
        while (isDigitChar(peek())) {
            text += advance();
            sawDigit = true;
        }
    }
    if (!sawDigit) {
        reportError(L("numero invalido", "invalid number"), startLine,
                    startColumn);
    }

    // exrad opcional: E sinal? inteiro (secao 6.2)
    if (peek() == 'E' || peek() == 'e') {
        std::size_t savedPos = pos_;
        int savedColumn = column_;
        std::string exPart;
        exPart += 'E'; // normaliza para maiuscula
        advance();
        if (peek() == '+' || peek() == '-') {
            exPart += advance();
        }
        if (isDigitChar(peek())) {
            while (isDigitChar(peek())) {
                exPart += advance();
            }
            text += exPart;
        } else {
            // nao era mesmo um exrad; devolve a posicao
            pos_ = savedPos;
            column_ = savedColumn;
        }
    }

    Token token;
    token.type = TokenType::Number;
    token.lexeme = text;
    token.line = startLine;
    token.column = startColumn;
    try {
        token.numberValue = std::stod(text);
    } catch (const std::out_of_range&) {
        // secao 6.5: overflow na avaliacao de uma constante numerica
        // NAO e erro lexico, e uma excecao nao-fatal cuja recuperacao
        // recomendada e substituir por infinito com o sinal correto e
        // continuar (o sinal em si fica a cargo do parser/interpretador,
        // que tratam o "-" como operador unario, nao parte do literal).
        // Um expoente muito NEGATIVO (ex.: 3E-99999) e o caso oposto,
        // nao e overflow, e underflow, e vira 0, nao infinito.
        auto epos = text.find('E');
        bool isUnderflow =
            epos != std::string::npos && epos + 1 < text.size() && text[epos + 1] == '-';
        token.numberValue = isUnderflow ? 0.0 : std::numeric_limits<double>::infinity();
    } catch (const std::invalid_argument&) {
        reportError(L("nao foi possivel converter o numero '",
                       "could not convert the number '") +
                        text + "'",
                    startLine, startColumn);
    }
    return token;
}

Token Lexer::lexStringLiteral() {
    int startLine = line_;
    int startColumn = column_;
    advance(); // aspas de abertura

    std::string text;
    while (!isAtEnd() && peek() != '"' && peek() != '\n' && peek() != '\r') {
        text += advance(); // conteudo da string NAO e normalizado para maiuscula
    }
    if (peek() == '"') {
        advance(); // aspas de fechamento
    } else {
        reportError(L("string nao fechada (faltou a aspas de fechamento)",
                       "unterminated string (missing closing quote)"),
                    startLine, startColumn);
    }

    Token token;
    token.type = TokenType::StringLiteral;
    token.lexeme = text;
    token.line = startLine;
    token.column = startColumn;
    return token;
}

Token Lexer::lexRemark(int startLine, int startColumn) {
    if (!isAtEnd() && peek() != '\n' && peek() != '\r') {
        if (peek() == ' ') {
            advance(); // espaco separador exigido pela secao 5.4; nao faz
                       // parte do texto do comentario
        } else {
            reportError(L("REM precisa de um espaco antes do comentario",
                          "REM needs a space before the comment"),
                        line_, column_);
        }
    }
    std::string text;
    while (!isAtEnd() && peek() != '\n' && peek() != '\r') {
        text += advance(); // remark-string preserva o texto original (secao 19)
    }

    Token token;
    token.type = TokenType::KwREM;
    token.lexeme = text;
    token.line = startLine;
    token.column = startColumn;
    return token;
}

Token Lexer::lexDataList(int startLine, int startColumn) {
    if (!isAtEnd() && peek() != '\n' && peek() != '\r') {
        if (peek() == ' ') {
            advance(); // espaco separador exigido pela secao 5.4; nao faz
                       // parte do conteudo do data-list
        }
    }
    std::string text;
    while (!isAtEnd() && peek() != '\n' && peek() != '\r') {
        text += advance(); // texto bruto, o parser separa por virgula e
                            // classifica cada datum (secao 17)
    }

    Token token;
    token.type = TokenType::DataList;
    token.lexeme = text;
    token.line = startLine;
    token.column = startColumn;
    return token;
}

Token Lexer::lexWord() {
    int startLine = line_;
    int startColumn = column_;

    // Caso especial: GO (secao 12.2). Precisa ser reconhecido como
    // palavra de exatamente 2 letras, ANTES do loop generico abaixo,
    // porque GOTO e GOSUB sao apenas GO colado (zero espacos) em TO/SUB
    // (producoes "GO space* TO" / "GO space* SUB"). Se o loop generico
    // consumisse todas as letras de uma vez, "GOTO" viraria uma palavra
    // so, que nao esta na tabela de palavras reservadas.
    if (toUpperChar(peek()) == 'G' && toUpperChar(peek(1)) == 'O') {
        std::string text;
        text += toUpperChar(advance());
        text += toUpperChar(advance());
        Token token;
        token.type = TokenType::KwGO;
        token.lexeme = text;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    // Caso especial: FN + letra (nome de funcao definida, secao 10).
    // Precisa ser verificado antes da regra geral porque "FN" sozinho
    // (duas letras) nao seria um identificador valido por nenhuma outra
    // regra da norma.
    if (toUpperChar(peek()) == 'F' && toUpperChar(peek(1)) == 'N' &&
        isAlphaChar(peek(2))) {
        std::string text;
        text += toUpperChar(advance());
        text += toUpperChar(advance());
        text += toUpperChar(advance());
        Token token;
        token.type = TokenType::DefFunctionName;
        token.lexeme = text;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    std::string letters;
    while (isAlphaChar(peek())) {
        letters += toUpperChar(advance());
    }

    if (letters.size() == 1) {
        char letter = letters[0];
        if (isDigitChar(peek())) {
            std::string text;
            text += letter;
            text += advance();
            Token token;
            token.type = TokenType::Identifier;
            token.lexeme = text;
            token.line = startLine;
            token.column = startColumn;
            return token;
        }
        if (peek() == '$') {
            std::string text;
            text += letter;
            text += advance();
            Token token;
            token.type = TokenType::Identifier;
            token.lexeme = text;
            token.line = startLine;
            token.column = startColumn;
            return token;
        }
        Token token;
        token.type = TokenType::Identifier;
        token.lexeme = letters;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    // Duas ou mais letras: so pode ser uma palavra reservada (keyword,
    // funcao embutida ou TAB). Nenhuma outra combinacao de 2+ letras
    // existe na gramatica da norma.
    auto it = reservedWords().find(letters);
    if (it != reservedWords().end()) {
        if (it->second == TokenType::KwREM) {
            return lexRemark(startLine, startColumn);
        }
        Token token;
        token.type = it->second;
        token.lexeme = letters;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    reportError("'" + letters +
                    L("' nao e uma palavra reservada valida da norma "
                      "(variaveis simples tem no maximo uma letra)",
                      "' is not a reserved word defined by the standard "
                      "(simple variables have at most one letter)"),
                startLine, startColumn);
    Token token;
    token.type = TokenType::Invalid;
    token.lexeme = letters;
    token.line = startLine;
    token.column = startColumn;
    return token;
}

Token Lexer::nextToken() {
    if (pendingDataList_) {
        pendingDataList_ = false;
        int startLine = line_;
        int startColumn = column_;
        return lexDataList(startLine, startColumn);
    }

    if (atLineStart_) {
        if (peek() == ' ') {
            reportError(L("uma linha nao pode comecar com espaco (secao 5.4)",
                          "a line cannot start with a space (section 5.4)"),
                        line_, column_);
        }
        atLineStart_ = false;
    }

    int spacesSkipped = skipSpaces();
    bool spaceBefore = spacesSkipped > 0;

    int startLine = line_;
    int startColumn = column_;

    if (isAtEnd()) {
        if (pos_ - lineStartPos_ > 72) {
            reportError(L("linha com mais de 72 caracteres (secao 4.4)",
                          "line longer than 72 characters (section 4.4)"),
                        line_, 73);
        }
        Token token;
        token.type = TokenType::EndOfFile;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    char c = peek();

    if (c == '\n' || c == '\r') {
        if (pos_ - lineStartPos_ > 72) {
            reportError(L("linha com mais de 72 caracteres (secao 4.4)",
                          "line longer than 72 characters (section 4.4)"),
                        line_, 73);
        }
        newLine();
        Token token;
        token.type = TokenType::EndOfLine;
        token.line = startLine;
        token.column = startColumn;
        return token;
    }

    if (isDigitChar(c) || (c == '.' && isDigitChar(peek(1)))) {
        return lexNumber();
    }

    if (c == '"') {
        return lexStringLiteral();
    }

    if (isAlphaChar(c)) {
        Token word = lexWord();
        if (isTrueKeyword(word.type)) {
            // GO seguido de TO/SUB colado (GOTO/GOSUB) e a unica excecao
            // de espaco obrigatorio antes de uma keyword (secao 12.2:
            // "GO space* TO" / "GO space* SUB").
            bool exemptSpaceBefore =
                lastTokenType_ == TokenType::KwGO &&
                (word.type == TokenType::KwTO || word.type == TokenType::KwSUB);
            if (!spaceBefore && !exemptSpaceBefore) {
                reportError(L("a palavra reservada '", "the reserved word '") +
                                word.lexeme +
                                L("' precisa de um espaco antes (secao 5.4)",
                                  "' needs a space before it (section 5.4)"),
                            startLine, startColumn);
            }
            // GO admite zero espacos antes de TO/SUB (GOTO/GOSUB colados);
            // REM ja validou seu proprio espaco separador em lexRemark.
            if (word.type != TokenType::KwGO &&
                word.type != TokenType::KwREM) {
                char after = peek();
                if (!isAtEnd() && after != '\n' && after != '\r' &&
                    after != ' ') {
                    reportError(L("a palavra reservada '", "the reserved word '") +
                                    word.lexeme +
                                    L("' precisa de um espaco depois, ou fim "
                                      "de linha (secao 5.4)",
                                      "' needs a space after it, or end of "
                                      "line (section 5.4)"),
                                line_, column_);
                }
            }
            if (word.type == TokenType::KwDATA) {
                pendingDataList_ = true;
            }
        }
        return word;
    }

    switch (c) {
        case '+': advance(); return Token{TokenType::Plus, "+", 0.0, startLine, startColumn};
        case '-': advance(); return Token{TokenType::Minus, "-", 0.0, startLine, startColumn};
        case '*': advance(); return Token{TokenType::Star, "*", 0.0, startLine, startColumn};
        case '/': advance(); return Token{TokenType::Slash, "/", 0.0, startLine, startColumn};
        case '^': advance(); return Token{TokenType::Caret, "^", 0.0, startLine, startColumn};
        case '=': advance(); return Token{TokenType::Equals, "=", 0.0, startLine, startColumn};
        case '(': advance(); return Token{TokenType::LParen, "(", 0.0, startLine, startColumn};
        case ')': advance(); return Token{TokenType::RParen, ")", 0.0, startLine, startColumn};
        case ',': advance(); return Token{TokenType::Comma, ",", 0.0, startLine, startColumn};
        case ';': advance(); return Token{TokenType::Semicolon, ";", 0.0, startLine, startColumn};
        case '<':
            advance();
            if (peek() == '=') { advance(); return Token{TokenType::LessEquals, "<=", 0.0, startLine, startColumn}; }
            if (peek() == '>') { advance(); return Token{TokenType::NotEquals, "<>", 0.0, startLine, startColumn}; }
            return Token{TokenType::Less, "<", 0.0, startLine, startColumn};
        case '>':
            advance();
            if (peek() == '=') { advance(); return Token{TokenType::GreaterEquals, ">=", 0.0, startLine, startColumn}; }
            return Token{TokenType::Greater, ">", 0.0, startLine, startColumn};
        default:
            break;
    }

    reportError(std::string(L("caractere invalido '", "invalid character '")) +
                    c + "'",
                startLine, startColumn);
    advance();
    Token token;
    token.type = TokenType::Invalid;
    token.lexeme = std::string(1, c);
    token.line = startLine;
    token.column = startColumn;
    return token;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
        Token token = nextToken();
        tokens.push_back(token);
        lastTokenType_ = token.type;
        if (token.type == TokenType::EndOfFile) {
            break;
        }
    }
    return tokens;
}

} // namespace ecma55
