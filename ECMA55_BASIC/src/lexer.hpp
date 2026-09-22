// lexer.hpp
// Lexer (analisador lexico) da ECMA-55 Minimal BASIC.
//
// Recebe o texto-fonte inteiro (um programa, com varias linhas) e
// devolve a sequencia de tokens, mais a lista de erros lexicos
// encontrados. Nao interpreta a estrutura das linhas (numero de linha,
// comandos, expressoes), isso e trabalho do parser (Camada 2). O
// lexer so garante que os caracteres, palavras reservadas, numeros,
// strings e operadores estao corretos segundo a secao 4 em diante da
// norma.
//
// Decisao de projeto: letras minusculas no fonte sao aceitas e 
// normalizadas para maiusculas, embora a norma (secao 4.4) defina 
// "letter" como somente A-Z maiusculo. O limite de 72 caracteres 
// por linha (secao 5.4) ainda nao e verificado nesta camada.

#ifndef ECMA55_LEXER_HPP
#define ECMA55_LEXER_HPP

#include <string>
#include <vector>

#include "locale.hpp"
#include "token.hpp"

namespace ecma55 {

struct LexError {
    std::string message;
    int line = 0;
    int column = 0;
};

class Lexer {
public:
    explicit Lexer(std::string source, Locale locale = Locale::English);

    // Tokeniza o texto inteiro. Pode ser chamado uma unica vez.
    std::vector<Token> tokenize();

    const std::vector<LexError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::string source_;
    Locale locale_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    bool atLineStart_ = true;
    std::size_t lineStartPos_ = 0; // posicao (em pos_) do inicio da linha atual
    TokenType lastTokenType_ = TokenType::Invalid; // usado so para a
                                                   // excecao GO+TO/GO+SUB
    std::vector<LexError> errors_;

    // -- leitura de caracteres --
    bool isAtEnd() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    void newLine();

    // -- espacos --
    int skipSpaces(); // devolve quantos espacos 
	                  //(ou TABs invalidos) foram pulados

    // -- reconhecimento de tokens --
    Token nextToken();
    Token lexNumber();
    Token lexStringLiteral();
    Token lexWord(); // keyword, funcao embutida, TAB, FN+letra ou identificador
    Token lexRemark(int line, int column); // resto da linha apos REM
    Token lexDataList(int line, int column); // resto da linha apos DATA (bruto)

    void reportError(const std::string& message, int line, int column);
    const char* L(const char* pt, const char* en) const;

    bool pendingDataList_ = false; // true logo apos emitir KwDATA
};

} // namespace ecma55

#endif // ECMA55_LEXER_HPP
