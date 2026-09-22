// test_interpreter.cpp
// Testes do interpretador, sem framework externo (so assert simples).
// Cada teste roda um programa completo (lexer + parser + interpretador)
// e confere a saida gerada.

#include <cstdio>
#include <sstream>
#include <string>

#include "../src/interpreter.hpp"
#include "../src/lexer.hpp"
#include "../src/parser.hpp"

using ecma55::Interpreter;
using ecma55::Lexer;
using ecma55::Parser;

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

void checkEqual(const std::string& got, const std::string& expected,
                 const char* description) {
    ++testsRun;
    if (got != expected) {
        ++testsFailed;
        std::printf("FALHOU: %s\n  esperado: %s\n  obtido:   %s\n",
                     description, expected.c_str(), got.c_str());
    }
}

// Roda um programa com uma entrada (para INPUT) e devolve a saida
// gerada, mais um ponteiro pro interpretador (que fica vivo enquanto o
// "programa" referenciado por ele nao for destruido).
struct RunResult {
    std::string output;
    bool hadFatalError = false;
    std::string errorMessage;
};

RunResult run(const std::string& source, const std::string& input = "") {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();

    std::istringstream in(input);
    std::ostringstream out;
    Interpreter interpreter(program, in, out);
    interpreter.run();

    RunResult result;
    result.output = out.str();
    result.hadFatalError = interpreter.hadFatalError();
    result.errorMessage = interpreter.fatalErrorMessage();
    return result;
}

// -------------------------------------------------------------------
// LET, expressoes e PRINT basico
// -------------------------------------------------------------------
void testLetAndPrint() {
    auto r = run("100 LET X = 2 + 3 * 4\n110 PRINT X\n120 END\n");
    check(!r.hadFatalError, "LET X = 2+3*4 / PRINT X: sem erro");
    checkEqual(r.output, " 14 \n", "2+3*4 = 14, formatado com espaco antes e depois");

    auto r2 = run("100 LET A$ = \"OLA\"\n110 PRINT A$\n120 END\n");
    check(!r2.hadFatalError, "LET A$ / PRINT A$: sem erro");
    checkEqual(r2.output, "OLA\n", "string impressa sem espacos extras");
}

// -------------------------------------------------------------------
// Variavel usada antes de receber valor (secao 7.6) - decisao
// confirmada com o autor: erro fatal
// -------------------------------------------------------------------
void testUninitializedVariableIsFatal() {
    auto r = run("100 PRINT X\n110 END\n");
    check(r.hadFatalError, "PRINT X sem LET antes: erro fatal (secao 7.6)");
}

// -------------------------------------------------------------------
// FOR/NEXT - inclusive o caso "top-tested" (nao executa nem uma vez)
// -------------------------------------------------------------------
void testForNextExecution() {
    auto r = run(
        "100 LET S = 0\n"
        "110 FOR I = 1 TO 5\n"
        "120 LET S = S + I\n"
        "130 NEXT I\n"
        "140 PRINT S\n"
        "150 END\n");
    check(!r.hadFatalError, "FOR I=1 TO 5 somando: sem erro");
    checkEqual(r.output, " 15 \n", "soma de 1 a 5 = 15");

    // secao 13.4: FOR e top-tested - se o limite ja falha de cara, o
    // corpo nao roda nenhuma vez
    auto r2 = run(
        "100 LET S = 0\n"
        "110 FOR I = 5 TO 1\n"
        "120 LET S = S + 1\n"
        "130 NEXT I\n"
        "140 PRINT S\n"
        "150 END\n");
    check(!r2.hadFatalError, "FOR I=5 TO 1 (sem STEP negativo): sem erro");
    checkEqual(r2.output, " 0 \n", "corpo nao executa nenhuma vez (top-tested)");

    // FOR aninhado
    auto r3 = run(
        "100 LET N = 0\n"
        "110 FOR I = 1 TO 3\n"
        "120 FOR J = 1 TO 3\n"
        "130 LET N = N + 1\n"
        "140 NEXT J\n"
        "150 NEXT I\n"
        "160 PRINT N\n"
        "170 END\n");
    check(!r3.hadFatalError, "FOR aninhado 3x3: sem erro");
    checkEqual(r3.output, " 9 \n", "3x3 = 9 iteracoes");
}

// -------------------------------------------------------------------
// GOTO / GOSUB / RETURN / IF-THEN
// -------------------------------------------------------------------
void testControlFlow() {
    auto r = run(
        "100 LET X = 1\n"
        "110 GOTO 130\n"
        "120 LET X = 99\n"
        "130 PRINT X\n"
        "140 END\n");
    check(!r.hadFatalError, "GOTO pulando uma linha: sem erro");
    checkEqual(r.output, " 1 \n", "GOTO pulou a linha 120 (X continua 1)");

    auto r2 = run(
        "100 GOSUB 200\n"
        "110 PRINT X\n"
        "120 GOTO 300\n"
        "200 LET X = 42\n"
        "210 RETURN\n"
        "300 END\n");
    check(!r2.hadFatalError, "GOSUB/RETURN: sem erro");
    checkEqual(r2.output, " 42 \n", "GOSUB atribuiu X e voltou certo");

    auto r3 = run(
        "100 LET X = 5\n"
        "110 IF X > 3 THEN 140\n"
        "120 PRINT \"NAO\"\n"
        "130 GOTO 150\n"
        "140 PRINT \"SIM\"\n"
        "150 END\n");
    check(!r3.hadFatalError, "IF X > 3 THEN 140: sem erro");
    checkEqual(r3.output, "SIM\n", "IF verdadeiro pulou pro THEN");
}

// -------------------------------------------------------------------
// ON...GO TO
// -------------------------------------------------------------------
void testOnGoto() {
    auto r = run(
        "100 LET L = 2\n"
        "110 ON L GO TO 200, 210, 220\n"
        "200 PRINT \"UM\"\n"
        "205 GOTO 230\n"
        "210 PRINT \"DOIS\"\n"
        "215 GOTO 230\n"
        "220 PRINT \"TRES\"\n"
        "230 END\n");
    check(!r.hadFatalError, "ON L GO TO com L=2: sem erro");
    checkEqual(r.output, "DOIS\n", "ON GO TO escolheu o segundo alvo");
}

// -------------------------------------------------------------------
// DATA / READ / RESTORE
// -------------------------------------------------------------------
void testDataReadRestore() {
    auto r = run(
        "100 READ A, B, C\n"
        "110 PRINT A; B; C\n"
        "120 RESTORE\n"
        "130 READ D\n"
        "140 PRINT D\n"
        "150 DATA 1, 2, 3\n"
        "160 END\n");
    check(!r.hadFatalError, "READ/RESTORE: sem erro");
    checkEqual(r.output, " 1  2  3 \n 1 \n", "READ leu 1,2,3 e RESTORE voltou ao inicio");
}

// -------------------------------------------------------------------
// DIM, OPTION BASE e indice fora do intervalo (secao 7.5, fatal)
// -------------------------------------------------------------------
void testArrays() {
    auto r = run(
        "100 DIM A(5)\n"
        "110 LET A(3) = 42\n"
        "120 PRINT A(3)\n"
        "130 END\n");
    check(!r.hadFatalError, "DIM A(5) / A(3)=42: sem erro");
    checkEqual(r.output, " 42 \n", "elemento de array lido de volta corretamente");

    auto r2 = run(
        "100 DIM A(5)\n"
        "110 LET A(9) = 1\n"
        "120 END\n");
    check(r2.hadFatalError, "A(9) com DIM A(5): erro fatal (secao 7.5)");

    auto r3 = run(
        "100 OPTION BASE 1\n"
        "110 DIM A(3)\n"
        "120 LET A(1) = 10\n"
        "130 LET A(3) = 30\n"
        "140 PRINT A(1); A(3)\n"
        "150 END\n");
    check(!r3.hadFatalError, "OPTION BASE 1 / DIM A(3): sem erro");
    checkEqual(r3.output, " 10  30 \n", "OPTION BASE 1 desloca o indice inicial para 1");
}

// -------------------------------------------------------------------
// DEF FN
// -------------------------------------------------------------------
void testDefFn() {
    auto r = run(
        "100 DEF FNF(X) = X^2 + 1\n"
        "110 PRINT FNF(3)\n"
        "120 END\n");
    check(!r.hadFatalError, "DEF FNF(X) = X^2+1 / FNF(3): sem erro");
    checkEqual(r.output, " 10 \n", "FNF(3) = 3^2+1 = 10");

    // parametro e local (secao 10.4): nao deve vazar/sobrescrever X de fora
    auto r2 = run(
        "100 LET X = 99\n"
        "110 DEF FNF(X) = X * 2\n"
        "120 LET Y = FNF(5)\n"
        "130 PRINT X; Y\n"
        "140 END\n");
    check(!r2.hadFatalError, "parametro local do DEF FN: sem erro");
    checkEqual(r2.output, " 99  10 \n", "X de fora nao foi afetado pelo parametro local");
}

// -------------------------------------------------------------------
// Funcoes embutidas e erros fatais (LOG/SQR - secao 9.5)
// -------------------------------------------------------------------
void testBuiltinFunctions() {
    auto r = run("100 PRINT SQR(16)\n110 END\n");
    check(!r.hadFatalError, "SQR(16): sem erro");
    checkEqual(r.output, " 4 \n", "SQR(16) = 4");

    auto r2 = run("100 PRINT SQR(-1)\n110 END\n");
    check(r2.hadFatalError, "SQR(-1): erro fatal (secao 9.5)");

    auto r3 = run("100 PRINT LOG(0)\n110 END\n");
    check(r3.hadFatalError, "LOG(0): erro fatal (secao 9.5)");
}

// -------------------------------------------------------------------
// INPUT (com entrada simulada via istringstream)
// -------------------------------------------------------------------
void testInput() {
    auto r = run(
        "100 INPUT X, Y\n"
        "110 PRINT X + Y\n"
        "120 END\n",
        "3,4\n");
    check(!r.hadFatalError, "INPUT X, Y com \"3,4\": sem erro");
    checkEqual(r.output, "?  7 \n",
               "INPUT leu 3 e 4, soma 7 (prompt '? ' + espaco do numero positivo)");
}

// -------------------------------------------------------------------
// Divisao por zero e 0^0 (secao 8.4/8.5) - nao fatais
// -------------------------------------------------------------------
void testNonFatalArithmetic() {
    auto r = run("100 PRINT 0^0\n110 END\n");
    check(!r.hadFatalError, "0^0: sem erro (secao 8.4)");
    checkEqual(r.output, " 1 \n", "0^0 = 1");
}

} // namespace

int main() {
    testLetAndPrint();
    testUninitializedVariableIsFatal();
    testForNextExecution();
    testControlFlow();
    testOnGoto();
    testDataReadRestore();
    testArrays();
    testDefFn();
    testBuiltinFunctions();
    testInput();
    testNonFatalArithmetic();

    std::printf("%d/%d testes passaram\n", testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
