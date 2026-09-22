// parser.hpp
// Parser (analisador sintatico) da ECMA-55 Minimal BASIC.
//
// Recebe a sequencia de tokens produzida pelo lexer (Camada 1) e monta
// a arvore sintatica do programa inteiro: uma lista de linhas (numero
// + comando), na ordem em que aparecem no fonte.
//
// Alem da sintaxe de cada comando (secoes 11 a 20), este parser tambem
// valida algumas regras estruturais do programa como um todo, porque
// sao regras facilmente verificaveis so com a sintaxe, sem precisar
// executar nada:
//   - numeros de linha em ordem estritamente crescente (secao 5.4);
//   - a ultima linha do programa e END, e END so aparece na ultima
//     linha (secao 5.2 e 5.4);
//   - blocos FOR/NEXT bem formados: cada NEXT fecha o FOR mais
//     interno ainda aberto, blocos FOR nao se entrelacam, e um FOR
//     aninhado nao reusa a variavel de controle de um FOR mais externo
//     (secao 13.4).
//
// Verificacoes que dependem de mais de uma linha em um sentido mais
// profundo (uma funcao DEF FN definida antes do primeiro uso, um
// GOTO/GOSUB apontando para uma linha que de fato existe, a aridade
// de uma chamada FNx bater com o parametro da definicao) ficam para
// a Camada 3 (o interpretador), que ja precisa ter o programa inteiro
// carregado para executar.

#ifndef ECMA55_PARSER_HPP
#define ECMA55_PARSER_HPP

#include <string>
#include <vector>

#include "ast.hpp"
#include "locale.hpp"
#include "token.hpp"

namespace ecma55 {

struct ParseError {
    std::string message;
    int line = 0;
    int column = 0;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, Locale locale = Locale::English);

    // Analisa o programa inteiro. Pode ser chamado uma unica vez.
    std::vector<Line> parseProgram();

    const std::vector<ParseError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::vector<Token> tokens_;
    Locale locale_;
    std::size_t pos_ = 0;
    std::vector<ParseError> errors_;

    // -- leitura de tokens --
    const Token& peek(std::size_t offset = 0) const;
    const Token& advance();
    bool check(TokenType type) const;
    const Token& expect(TokenType type, const std::string& message);
    void skipToEndOfLine();
    void reportError(const std::string& message, int line, int column);
    const char* L(const char* pt, const char* en) const;

    // -- programa e linhas --
    Line parseLine();
    void validateProgramStructure(std::vector<Line>& lines);
    int expectLineNumberValue(const Token& numberToken);
    int expectIntegerBound(const Token& numberToken);

    // -- comandos (secoes 11 a 20) --
    Stmt parseStatement(int lineNumber);
    Stmt parseLet(int lineNumber);
    Stmt parsePrint(int lineNumber);
    Stmt parseInput(int lineNumber);
    Stmt parseRead(int lineNumber);
    Stmt parseData(int lineNumber);
    Stmt parseDim(int lineNumber);
    Stmt parseOption(int lineNumber);
    Stmt parseDef(int lineNumber);
    Stmt parseGoto(int lineNumber);
    Stmt parseGosub(int lineNumber);
    Stmt parseIfThen(int lineNumber);
    Stmt parseOnGoto(int lineNumber);
    Stmt parseFor(int lineNumber);
    Stmt parseNext(int lineNumber);
    Stmt parseRemark(int lineNumber, const std::string& text);

    std::vector<DataItem> splitDataList(const std::string& raw, int line,
                                         int column);

    // -- expressoes (secao 8, mais variaveis da 7, funcoes da 9/10 e
    //    relacoes da 12) --
    ExprPtr parseExpression();           // numeric-expression / string-expression
    ExprPtr parseRelationalExpression(); // usado so pelo IF-THEN
    ExprPtr parseNumericExpression();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parsePrimary();
    ExprPtr parseVariableRef(); // variavel simples, de array ou de string
};

} // namespace ecma55

#endif // ECMA55_PARSER_HPP
