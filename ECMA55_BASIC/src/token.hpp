// token.hpp
// Tipos de token do lexer da ECMA-55 Minimal BASIC.
//
// A lista de palavras reservadas segue a secao 3.8 da norma, que define
// exatamente 26 keywords: BASE, DATA, DEF, DIM, END, FOR, GO, GOSUB,
// GOTO, IF, INPUT, LET, NEXT, ON, OPTION, PRINT, RANDOMIZE, READ, REM,
// RESTORE, RETURN, STEP, STOP, SUB, THEN e TO.
//
// GOSUB e GOTO nao viram tokens proprios aqui: a propria gramatica da
// norma (secoes 12.2.1 e 12.2.9) define os dois como "GO <space> TO" e
// "GO <space> SUB", ou seja, GO, TO e SUB ja sao keywords individuais
// suficientes; GOTO/GOSUB sao apenas a grafia colada (zero espacos) de
// GO+TO e GO+SUB. Reconhecer GO, TO e SUB separadamente cobre os dois
// casos sem duplicar logica; cabe ao parser (Camada 2) juntar a
// sequencia GO TO / GO SUB no comando correspondente.
//
// As 11 funcoes embutidas (secao 9.2) e o TAB do print-statement
// (secao 14.2) NAO fazem parte da lista de keywords da secao 3.8.
// Por isso, diferente das keywords, elas nao exigem espaco
// obrigatorio antes/depois (ex.: "SQR(X^2+Y^2)" e "TAB(10)" na propria
// norma, colados ao parêntese). Por isso recebem tipos de token
// proprios, separados das keywords.

#ifndef ECMA55_TOKEN_HPP
#define ECMA55_TOKEN_HPP

#include <string>

namespace ecma55 {

enum class TokenType {
    // literais e nomes (secoes 6 e 7)
    Number,          // numeric-constant (sem sinal - ver lexer.hpp)
    StringLiteral,   // string-constant
    Identifier,      // variavel simples, array ou string (letra [digito|$]?)
    DefFunctionName, // FN + letra (secao 10)

    // as 24 keywords "puras" da secao 3.8 (GOTO/GOSUB ficam de fora - ver acima)
    KwBASE, KwDATA, KwDEF, KwDIM, KwEND, KwFOR, KwGO, KwIF, KwINPUT,
    KwLET, KwNEXT, KwON, KwOPTION, KwPRINT, KwRANDOMIZE, KwREAD, KwREM,
    KwRESTORE, KwRETURN, KwSTEP, KwSTOP, KwSUB, KwTHEN, KwTO,

    // funcoes embutidas (secao 9.2) - reservadas, mas nao sao "keyword"
    FnABS, FnATN, FnCOS, FnEXP, FnINT, FnLOG, FnRND, FnSGN, FnSIN,
    FnSQR, FnTAN,

    // TAB do print-statement (secao 14.2) - mesma observacao acima
    KwTAB,

    // resto da linha de um data-statement (secao 17), capturado bruto:
    // um datum pode ser um unquoted-string qualquer (ex.: "PI" no
    // exemplo "DATA 3.14159, PI, 5E-10, ,"" da propria norma), que nao
    // e reconhecivel pelas regras normais do lexer (nao e keyword, nem
    // identificador valido). Quem separa por virgula e classifica cada
    // datum (numero / string entre aspas / unquoted-string) e o parser.
    DataList,

    // operadores (secao 8.2) e relacoes (secao 12.2)
    Plus, Minus, Star, Slash, Caret,
    Equals, NotEquals, Less, LessEquals, Greater, GreaterEquals,

    // pontuacao
    LParen, RParen, Comma, Semicolon,

    // controle
    EndOfLine,
    EndOfFile,
    Invalid
};

// Um token reconhecido pelo lexer.
//
// "lexeme" guarda o texto original (ja normalizado para maiusculas),
// exceto para KwREM, onde guarda o comentario inteiro apos "REM ".
struct Token {
    TokenType type = TokenType::Invalid;
    std::string lexeme;
    double numberValue = 0.0; // valido somente quando type == Number
    int line = 0;
    int column = 0;
};

// Nome legivel do tipo de token, usado em mensagens de erro e em testes.
const char* tokenTypeName(TokenType type);

} // namespace ecma55

#endif // ECMA55_TOKEN_HPP
