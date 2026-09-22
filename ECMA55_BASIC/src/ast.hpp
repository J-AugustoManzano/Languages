// ast.hpp
// Arvore sintatica da ECMA-55 Minimal BASIC.
//
// Expr e Stmt sao "structs com tag": um campo "kind"/"stmtKind" diz
// qual variante e valida, e so os campos daquela variante sao usados.
// E mais simples de ler para quem esta aprendendo do que uma
// hierarquia de classes com visitor e, como a gramatica da norma e
// pequena e fechada (nunca vai crescer com novas variantes), o
// principal argumento a favor do padrao visitor (extensibilidade) nao
// se aplica aqui.

#ifndef ECMA55_AST_HPP
#define ECMA55_AST_HPP

#include <memory>
#include <string>
#include <vector>

#include "token.hpp"

namespace ecma55 {

// ---------------------------------------------------------------------
// Expressoes (secao 8, mais variaveis da secao 7 e funcoes da 9/10)
// ---------------------------------------------------------------------

enum class ExprKind {
    NumberLiteral,   // numeric-rep (secao 6)
    StringLiteral,   // string-constant (secao 6)
    SimpleVariable,  // letra ou letra+digito (secao 7)
    StringVariable,  // letra + $ (secao 7)
    ArrayElement,    // nome(indice) ou nome(indice,indice) (secao 7)
    BuiltinCall,     // ABS(X), SQR(X), TAB(X) etc. (secao 9)
    DefFunctionCall, // FNx ou FNx(arg) (secao 10)
    Unary,           // sinal + ou - na frente de um termo (secao 8)
    Binary           // + - * / ^  ou uma relacao =,<>,<,<=,>,>= (secoes 8 e 12)
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    ExprKind kind;
    int line = 0;
    int column = 0;

    double numberValue = 0.0;               // NumberLiteral
    std::string stringValue;                // StringLiteral
    std::string name;                       // SimpleVariable / StringVariable / ArrayElement (nome do arranjo)
    std::vector<ExprPtr> subscripts;        // ArrayElement (1 ou 2) / BuiltinCall e DefFunctionCall (0 ou 1)
    TokenType builtin = TokenType::Invalid; // BuiltinCall: qual funcao (FnABS..FnTAN, KwTAB)
    std::string defFunctionName;            // DefFunctionCall: ex. "FNF"
    TokenType op = TokenType::Invalid;      // Unary/Binary: operador
    ExprPtr left;                           // Unary usa so "left"; Binary usa left e right
    ExprPtr right;
};

// ---------------------------------------------------------------------
// Comandos (secoes 11 a 20)
// ---------------------------------------------------------------------

enum class StmtKind {
    Data, Def, Dim, Gosub, Goto, IfThen, Input, Let, OnGoto, Option,
    Print, Randomize, Read, Remark, Restore, Return, Stop, For, Next, End
};

// Um item da lista do PRINT (secao 14): "expr" quando e um valor a
// imprimir, "isTab" quando e uma chamada TAB(...), ou nenhum dos dois
// quando o item esta vazio (dois separadores seguidos, ex. "PRINT ,,X").
// "separator" e o separador que vem logo depois: ',' ou ';', ou '\0'
// quando nao ha separador depois deste item (ultimo item da lista).
struct PrintItem {
    ExprPtr expr;
    bool isTab = false;
    char separator = '\0';
};

// Um datum do DATA (secao 17): texto original preservado, mais uma
// marca de "veio entre aspas" (string-constant) ou nao (unquoted-string
// ou numeric-constant, a distincao final entre os dois so importa no
// momento do READ, na Camada 3, dependendo do tipo da variavel-alvo).
struct DataItem {
    std::string text;
    bool wasQuoted = false;
};

// Uma declaracao do DIM (secao 18): "bound2" negativo significa array
// de uma dimensao.
struct ArrayDeclaration {
    std::string name;
    int bound1 = 0;
    int bound2 = -1;
};

struct Stmt {
    StmtKind kind;
    int line = 0;   // numero da linha BASIC (line-number) que contem o comando
    int column = 0;

    // LET (secao 11)
    ExprPtr letTarget;
    ExprPtr letValue;

    // PRINT (secao 14)
    std::vector<PrintItem> printItems;

    // INPUT / READ (secoes 15 e 16) - lista de variaveis-alvo
    std::vector<ExprPtr> variableList;

    // DATA (secao 17)
    std::vector<DataItem> dataItems;

    // DIM (secao 18)
    std::vector<ArrayDeclaration> dimDeclarations;

    // OPTION BASE (secao 18)
    int optionBase = 0;

    // DEF (secao 10)
    std::string defName;  // ex. "FNF"
    std::string defParam; // nome do parametro simples; vazio se DEF FNx sem parametro
    ExprPtr defBody;

    // GOTO / GOSUB (secao 12) e alvo do IF-THEN
    int targetLine = 0;

    // ON...GOTO (secao 12)
    ExprPtr onExpr;
    std::vector<int> onTargets;

    // IF-THEN (secao 12)
    ExprPtr ifCondition; // Expr::Binary com um operador relacional
    int ifTargetLine = 0;

    // FOR (secao 13)
    std::string forVariable;
    ExprPtr forInitial;
    ExprPtr forLimit;
    ExprPtr forStep; // nulo quando nao ha STEP (equivale a +1 - secao 13.4)

    // NEXT (secao 13)
    std::string nextVariable;

    // REM (secao 19)
    std::string remarkText;
};

struct Line {
    int number = 0;
    Stmt statement;
};

} // namespace ecma55

#endif // ECMA55_AST_HPP
