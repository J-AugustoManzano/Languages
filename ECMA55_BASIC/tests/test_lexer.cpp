// test_lexer.cpp
// Testes do lexer, sem framework externo (so assert simples).
// Os casos vem, sempre que possivel, dos proprios exemplos da norma
// ECMA-55 (secoes 6.3, 7.3, 8.3, 9, 10.3, 12.3, 13.3, 14.3, etc.).

#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

#include "../src/lexer.hpp"
#include "../src/token.hpp"

using ecma55::Lexer;
using ecma55::Token;
using ecma55::TokenType;

namespace {

int testsRun = 0;
int testsFailed = 0;

void check(bool condition, const char* description) {
    ++testsRun;
    if (!condition) {
        ++testsFailed;
        std::printf("FALHOU: %s\n", description);
    }
}

std::vector<Token> tokensOf(const std::string& source) {
    Lexer lexer(source);
    return lexer.tokenize();
}

// -------------------------------------------------------------------
// Secao 6.3 - constantes numericas
// -------------------------------------------------------------------
void testNumericConstants() {
    {
        auto t = tokensOf("500");
        check(t.size() == 2, "500: dois tokens (Number, EOF)");
        check(t[0].type == TokenType::Number, "500: tipo Number");
        check(t[0].numberValue == 500.0, "500: valor 500");
    }
    {
        // sinal fica fora do numero (fica a cargo do parser)
        auto t = tokensOf("-21.");
        check(t.size() == 3, "-21.: tres tokens (Minus, Number, EOF)");
        check(t[0].type == TokenType::Minus, "-21.: primeiro token Minus");
        check(t[1].type == TokenType::Number, "-21.: segundo token Number");
        check(t[1].numberValue == 21.0, "-21.: valor 21");
    }
    {
        auto t = tokensOf(".255");
        check(t[0].type == TokenType::Number, ".255: tipo Number");
        check(std::fabs(t[0].numberValue - 0.255) < 1e-9, ".255: valor 0.255");
    }
    {
        auto t = tokensOf("1E10");
        check(t[0].type == TokenType::Number, "1E10: tipo Number");
        check(t[0].numberValue == 1e10, "1E10: valor 1e10");
    }
    {
        auto t = tokensOf("5E-1");
        check(t[0].type == TokenType::Number, "5E-1: tipo Number");
        check(std::fabs(t[0].numberValue - 0.5) < 1e-9, "5E-1: valor 0.5");
    }
    {
        auto t = tokensOf(".4E+1");
        check(t[0].type == TokenType::Number, ".4E+1: tipo Number");
        check(std::fabs(t[0].numberValue - 4.0) < 1e-9, ".4E+1: valor 4.0");
    }
}

// -------------------------------------------------------------------
// Secao 6.3 - constantes string (conteudo preserva maiusc/minusc)
// -------------------------------------------------------------------
void testStringConstants() {
    {
        auto t = tokensOf("\"XYZ\"");
        check(t[0].type == TokenType::StringLiteral, "\"XYZ\": tipo StringLiteral");
        check(t[0].lexeme == "XYZ", "\"XYZ\": conteudo XYZ");
    }
    {
        auto t = tokensOf("\"X - 3B2\"");
        check(t[0].lexeme == "X - 3B2", "\"X - 3B2\": conteudo preservado com espacos");
    }
    {
        auto t = tokensOf("\"Ola, mundo!\"");
        check(t[0].lexeme == "Ola, mundo!",
              "string com minuscula preserva o texto original (nao normaliza)");
    }
}

// -------------------------------------------------------------------
// Secao 7.3 - variaveis
// -------------------------------------------------------------------
void testVariables() {
    {
        auto t = tokensOf("X A5");
        check(t[0].type == TokenType::Identifier && t[0].lexeme == "X", "X: identificador simples");
        check(t[1].type == TokenType::Identifier && t[1].lexeme == "A5", "A5: identificador letra+digito");
    }
    {
        auto t = tokensOf("S$");
        check(t[0].type == TokenType::Identifier && t[0].lexeme == "S$", "S$: variavel string");
    }
    {
        // minuscula aceita e normalizada (decisao confirmada com o autor)
        auto t = tokensOf("x");
        check(t[0].type == TokenType::Identifier && t[0].lexeme == "X",
              "minuscula normalizada para maiuscula");
    }
}

// -------------------------------------------------------------------
// Secao 8.3 - expressoes (funcoes coladas ao parentese, sem espaco)
// -------------------------------------------------------------------
void testExpressionsAndFunctions() {
    {
        auto t = tokensOf("SQR(X^2+Y^2)");
        check(!Lexer(("SQR(X^2+Y^2)")).hasErrors(), "SQR colado ao parentese nao e erro");
        check(t[0].type == TokenType::FnSQR, "SQR: tipo FnSQR");
        check(t[1].type == TokenType::LParen, "SQR(: parentese aberto em seguida");
    }
    {
        auto t = tokensOf("3*X - Y^2");
        std::vector<TokenType> expected = {
            TokenType::Number, TokenType::Star, TokenType::Identifier,
            TokenType::Minus, TokenType::Identifier, TokenType::Caret,
            TokenType::Number, TokenType::EndOfFile,
        };
        check(t.size() == expected.size(), "3*X - Y^2: numero de tokens");
        for (std::size_t i = 0; i < expected.size() && i < t.size(); ++i) {
            check(t[i].type == expected[i], "3*X - Y^2: tipo do token na posicao esperada");
        }
    }
}

// -------------------------------------------------------------------
// Secao 10.3 - DEF FN
// -------------------------------------------------------------------
void testDefFunction() {
    Lexer lexer("100 DEF FNF(X) = X^4 - 1");
    auto t = lexer.tokenize();
    check(!lexer.hasErrors(), "100 DEF FNF(X) = X^4 - 1: sem erros");
    check(t[1].type == TokenType::KwDEF, "DEF: tipo KwDEF");
    check(t[2].type == TokenType::DefFunctionName && t[2].lexeme == "FNF",
          "FNF: tipo DefFunctionName");
}

// -------------------------------------------------------------------
// Secao 12.3 - GO TO / GO SUB, com e sem espaco (GOTO/GOSUB colados)
// -------------------------------------------------------------------
void testGotoGosub() {
    // Todos os casos levam um numero de linha na frente (ex.: "120 "):
    // e isso que garante o espaco exigido antes da keyword (secao 5.4).
    // Testar so o fragmento "GO TO 999" sem numero de linha simularia
    // uma keyword literalmente no inicio do texto, sem nada antes dela
    // -- uma linha assim nao existe num programa valido.
    {
        Lexer lexer("120 GO TO 999");
        auto t = lexer.tokenize();
        check(!lexer.hasErrors(), "120 GO TO 999: sem erros");
        check(t[1].type == TokenType::KwGO, "120 GO TO 999: token 1 KwGO");
        check(t[2].type == TokenType::KwTO, "120 GO TO 999: token 2 KwTO");
        check(t[3].type == TokenType::Number, "120 GO TO 999: token 3 Number");
    }
    {
        // GOTO colado (zero espacos entre GO e TO) e explicitamente valido
        // pela producao "GO space* TO" da secao 12.2
        Lexer lexer("120 GOTO 999");
        auto t = lexer.tokenize();
        check(!lexer.hasErrors(), "120 GOTO (colado): sem erros");
        check(t[1].type == TokenType::KwGO, "GOTO: token 1 KwGO");
        check(t[2].type == TokenType::KwTO, "GOTO: token 2 KwTO");
    }
    {
        Lexer lexer("120 GOSUB 500");
        auto t = lexer.tokenize();
        check(!lexer.hasErrors(), "120 GOSUB (colado): sem erros");
        check(t[1].type == TokenType::KwGO, "GOSUB: token 1 KwGO");
        check(t[2].type == TokenType::KwSUB, "GOSUB: token 2 KwSUB");
    }
}

// -------------------------------------------------------------------
// Secao 14.3 - PRINT / TAB (TAB tambem colado ao parentese)
// -------------------------------------------------------------------
void testPrintAndTab() {
    Lexer lexer("100 PRINT TAB(10); A$; \"IS DONE.\"");
    auto t = lexer.tokenize();
    check(!lexer.hasErrors(), "100 PRINT TAB(10)...: sem erros");
    check(t[1].type == TokenType::KwPRINT, "PRINT: tipo KwPRINT");
    check(t[2].type == TokenType::KwTAB, "TAB: tipo KwTAB");
    check(t[3].type == TokenType::LParen, "TAB(: parentese colado, sem espaco exigido");
}

// -------------------------------------------------------------------
// Secao 19 - REM (o resto da linha e livre, nao e tokenizado)
// -------------------------------------------------------------------
void testRemark() {
    Lexer lexer("999 REM FINAL CHECK");
    auto t = lexer.tokenize();
    check(!lexer.hasErrors(), "REM FINAL CHECK: sem erros");
    check(t[1].type == TokenType::KwREM, "REM: tipo KwREM");
    check(t[1].lexeme == "FINAL CHECK", "REM: texto do comentario preservado");
}

// -------------------------------------------------------------------
// Secao 5.4 - regras de espaco em torno de keywords
// -------------------------------------------------------------------
void testSpacingRules() {
    {
        Lexer lexer("100LET X=5");
        lexer.tokenize();
        check(lexer.hasErrors(), "100LET colado: deve reportar erro de espaco antes da keyword");
    }
    {
        Lexer lexer(" 100 LET X = 5");
        lexer.tokenize();
        check(lexer.hasErrors(), "linha comecando com espaco deve reportar erro");
    }
    {
        Lexer lexer("100 LET X = 5");
        lexer.tokenize();
        check(!lexer.hasErrors(), "100 LET X = 5: forma correta, sem erros");
    }
}

// -------------------------------------------------------------------
// Programa completo: Crivo de Eratostenes (exemplo da propria norma,
// via Wikipedia/ECMA-55) -- valida um programa real de ponta a ponta
// -------------------------------------------------------------------
void testFullProgramNoErrors() {
    const std::string program =
        "1000 REM SIEVE OF ERATOSTHENES\n"
        "2030 LET L = 1000\n"
        "2050 DIM N(1000)\n"
        "2070 FOR I = 1 TO L\n"
        "2080 LET N(I) = I\n"
        "2090 NEXT I\n"
        "2110 LET P = 2\n"
        "2120 PRINT P,\n"
        "2140 FOR I = P TO L STEP P\n"
        "2150 LET N(I) = 0\n"
        "2160 NEXT I\n"
        "2180 LET P = P + 1\n"
        "2190 IF P = L THEN 2220\n"
        "2200 IF N(P) <> 0 THEN 2120\n"
        "2210 GOTO 2180\n"
        "2220 PRINT\n"
        "2230 END\n";
    Lexer lexer(program);
    lexer.tokenize();
    check(!lexer.hasErrors(), "Crivo de Eratostenes: programa inteiro sem erros lexicos");
}

// -------------------------------------------------------------------
// Secao 17 - DATA (o resto da linha e capturado bruto; o parser separa
// por virgula e classifica cada datum)
// -------------------------------------------------------------------
void testDataStatement() {
    Lexer lexer("100 DATA 3.14159, PI, 5E-10, \",\"");
    auto t = lexer.tokenize();
    check(!lexer.hasErrors(),
          "DATA 3.14159, PI, 5E-10, \",\": sem erros (PI e unquoted-string, nao keyword)");
    check(t[1].type == TokenType::KwDATA, "DATA: segundo token e KwDATA");
    check(t[2].type == TokenType::DataList, "DATA: terceiro token e DataList");
    check(t[2].lexeme == "3.14159, PI, 5E-10, \",\"",
          "DATA: conteudo bruto preservado para o parser separar");
}

} // namespace

int main() {
    testNumericConstants();
    testStringConstants();
    testVariables();
    testExpressionsAndFunctions();
    testDefFunction();
    testGotoGosub();
    testPrintAndTab();
    testRemark();
    testDataStatement();
    testSpacingRules();
    testFullProgramNoErrors();

    std::printf("%d/%d testes passaram\n", testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
