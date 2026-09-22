// test_parser.cpp
// Testes do parser, sem framework externo (so assert simples).

#include <cstdio>
#include <cmath>
#include <vector>

#include "../src/lexer.hpp"
#include "../src/parser.hpp"
#include "../src/token.hpp"

using ecma55::Lexer;
using ecma55::Parser;
using ecma55::Line;
using ecma55::Stmt;
using ecma55::StmtKind;
using ecma55::Expr;
using ecma55::ExprKind;
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

std::vector<Line> parse(const std::string& source, Parser** outParser = nullptr) {
    static std::vector<std::unique_ptr<Parser>> keepAlive; // simples: mantem o Parser vivo
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    auto parser = std::make_unique<Parser>(std::move(tokens));
    auto lines = parser->parseProgram();
    if (outParser) {
        keepAlive.push_back(std::move(parser));
        *outParser = keepAlive.back().get();
    }
    return lines;
}

// -------------------------------------------------------------------
// Programa completo: Crivo de Eratostenes
// -------------------------------------------------------------------
void testFullProgram() {
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
    Parser* parser = nullptr;
    auto lines = parse(program, &parser);
    check(!parser->hasErrors(), "Crivo de Eratostenes: sem erros de parse");
    check(lines.size() == 17, "Crivo de Eratostenes: 17 linhas");
    check(lines.back().statement.kind == StmtKind::End,
          "Crivo de Eratostenes: ultima linha e END");
    check(lines[0].statement.kind == StmtKind::Remark, "linha 1000 e REM");
    check(lines[3].statement.kind == StmtKind::For, "linha 2070 e FOR");
    check(lines[5].statement.kind == StmtKind::Next, "linha 2090 e NEXT");
}

// -------------------------------------------------------------------
// LET (secao 11) e precedencia de expressao (secao 8)
// -------------------------------------------------------------------
void testLetAndExpressionPrecedence() {
    Parser* parser = nullptr;
    auto lines = parse("100 LET P = 3.14159\n110 END\n", &parser);
    check(!parser->hasErrors(), "LET P = 3.14159: sem erros");
    check(lines[0].statement.kind == StmtKind::Let, "LET: StmtKind::Let");
    check(lines[0].statement.letValue->kind == ExprKind::NumberLiteral,
          "LET P = 3.14159: valor e NumberLiteral");

    // 3*X - Y^2 : precedencia ^ > * > + - (secao 8.4)
    auto lines2 = parse("100 LET A = 3*X - Y^2\n110 END\n", &parser);
    check(!parser->hasErrors(), "3*X - Y^2: sem erros");
    const Expr* top = lines2[0].statement.letValue.get();
    check(top->kind == ExprKind::Binary && top->op == TokenType::Minus,
          "3*X - Y^2: raiz e Minus (a subtracao e a operacao de menor precedencia)");
    check(top->left->op == TokenType::Star,
          "3*X - Y^2: lado esquerdo e a multiplicacao 3*X");
    check(top->right->op == TokenType::Caret,
          "3*X - Y^2: lado direito e a involucao Y^2");

    // A^B^C = (A^B)^C : involucao associa a esquerda (secao 8.4)
    auto lines3 = parse("100 LET R = A^B^C\n110 END\n", &parser);
    check(!parser->hasErrors(), "A^B^C: sem erros");
    const Expr* r = lines3[0].statement.letValue.get();
    check(r->op == TokenType::Caret && r->left->op == TokenType::Caret,
          "A^B^C: associa como (A^B)^C, nao A^(B^C)");
}

// -------------------------------------------------------------------
// Tipos incompativeis no LET (numero vs string)
// -------------------------------------------------------------------
void testLetTypeMismatch() {
    Parser* parser = nullptr;
    parse("100 LET A = \"X\"\n110 END\n", &parser);
    check(parser->hasErrors(), "LET A = \"X\": deve reportar erro (variavel numerica recebendo string)");

    parse("100 LET A$ = \"ABC\"\n110 END\n", &parser);
    check(!parser->hasErrors(), "LET A$ = \"ABC\": forma correta, sem erros");

    parse("100 LET A$ = B$\n110 END\n", &parser);
    check(!parser->hasErrors(), "LET A$ = B$: forma correta, sem erros");
}

// -------------------------------------------------------------------
// GO TO / GOTO / GO SUB / GOSUB (secao 12)
// -------------------------------------------------------------------
void testGotoGosub() {
    Parser* parser = nullptr;
    auto lines = parse("100 GOTO 999\n999 END\n", &parser);
    check(!parser->hasErrors(), "GOTO 999: sem erros");
    check(lines[0].statement.kind == StmtKind::Goto, "GOTO: StmtKind::Goto");
    check(lines[0].statement.targetLine == 999, "GOTO 999: alvo 999");

    auto lines2 = parse("100 GOSUB 500\n110 RETURN\n120 END\n", &parser);
    check(!parser->hasErrors(), "GOSUB 500 / RETURN: sem erros");
    check(lines2[0].statement.kind == StmtKind::Gosub, "GOSUB: StmtKind::Gosub");
    check(lines2[1].statement.kind == StmtKind::Return, "RETURN: StmtKind::Return");
}

// -------------------------------------------------------------------
// IF-THEN, relacoes numericas e restricao de strings (secao 12.4)
// -------------------------------------------------------------------
void testIfThen() {
    Parser* parser = nullptr;
    auto lines = parse("100 IF X > Y+83 THEN 200\n200 END\n", &parser);
    check(!parser->hasErrors(), "IF X > Y+83 THEN 200: sem erros");
    check(lines[0].statement.kind == StmtKind::IfThen, "IF: StmtKind::IfThen");
    check(lines[0].statement.ifTargetLine == 200, "IF ... THEN 200: alvo 200");
    check(lines[0].statement.ifCondition->op == TokenType::Greater,
          "IF X > Y+83: operador relacional Greater");

    auto lines2 = parse("100 IF A$ <> B$ THEN 550\n550 END\n", &parser);
    check(!parser->hasErrors(), "IF A$ <> B$ THEN 550: sem erros");

    // strings so podem usar = ou <> (secao 12.4) - '>' deve dar erro
    parse("100 IF A$ > B$ THEN 200\n200 END\n", &parser);
    check(parser->hasErrors(), "IF A$ > B$: deve reportar erro (relacao invalida p/ string)");
}

// -------------------------------------------------------------------
// ON...GO TO (secao 12)
// -------------------------------------------------------------------
void testOnGoto() {
    Parser* parser = nullptr;
    auto lines = parse("100 ON L+1 GO TO 300,400,500\n300 END\n", &parser);
    check(!parser->hasErrors(), "ON L+1 GO TO 300,400,500: sem erros");
    check(lines[0].statement.kind == StmtKind::OnGoto, "ON GO TO: StmtKind::OnGoto");
    check(lines[0].statement.onTargets.size() == 3, "ON GO TO: 3 alvos");
    check(lines[0].statement.onTargets[2] == 500, "ON GO TO: terceiro alvo 500");
}

// -------------------------------------------------------------------
// FOR/NEXT (secao 13) - bem formado, aninhamento e reuso de variavel
// -------------------------------------------------------------------
void testForNext() {
    Parser* parser = nullptr;
    auto lines = parse("100 FOR I = 1 TO 10\n110 NEXT I\n120 END\n", &parser);
    check(!parser->hasErrors(), "FOR I = 1 TO 10 / NEXT I: sem erros");
    check(lines[0].statement.forStep == nullptr,
          "FOR sem STEP: forStep fica nulo (equivale a +1 - secao 13.4)");

    auto lines2 = parse("100 FOR I = A TO B STEP -1\n110 NEXT I\n120 END\n", &parser);
    check(!parser->hasErrors(), "FOR I = A TO B STEP -1: sem erros");
    check(lines2[0].statement.forStep != nullptr, "FOR ... STEP -1: forStep presente");

    // FOR aninhado corretamente
    parse("100 FOR I = 1 TO 10\n110 FOR J = 1 TO 5\n120 NEXT J\n130 NEXT I\n140 END\n", &parser);
    check(!parser->hasErrors(), "FOR I / FOR J aninhado: sem erros");

    // NEXT sem FOR
    parse("100 NEXT I\n110 END\n", &parser);
    check(parser->hasErrors(), "NEXT I sem FOR: deve reportar erro");

    // FOR sem NEXT
    parse("100 FOR I = 1 TO 10\n110 END\n", &parser);
    check(parser->hasErrors(), "FOR sem NEXT: deve reportar erro");

    // blocos entrelacados (NEXT na ordem errada)
    parse("100 FOR I = 1 TO 10\n110 FOR J = 1 TO 5\n120 NEXT I\n130 NEXT J\n140 END\n", &parser);
    check(parser->hasErrors(), "FOR/NEXT entrelacados: deve reportar erro (secao 13.4)");

    // FOR aninhado reusando a mesma variavel de controle
    parse("100 FOR I = 1 TO 10\n110 FOR I = 1 TO 5\n120 NEXT I\n130 NEXT I\n140 END\n", &parser);
    check(parser->hasErrors(), "FOR aninhado reusando I: deve reportar erro (secao 13.4)");
}

// -------------------------------------------------------------------
// DEF FN (secao 10)
// -------------------------------------------------------------------
void testDefFn() {
    Parser* parser = nullptr;
    auto lines = parse("100 DEF FNF(X) = X^4 - 1\n110 END\n", &parser);
    check(!parser->hasErrors(), "DEF FNF(X) = X^4 - 1: sem erros");
    check(lines[0].statement.kind == StmtKind::Def, "DEF: StmtKind::Def");
    check(lines[0].statement.defName == "FNF", "DEF FNF: nome FNF");
    check(lines[0].statement.defParam == "X", "DEF FNF(X): parametro X");

    auto lines2 = parse("100 DEF FNP = 3.14159\n110 END\n", &parser);
    check(!parser->hasErrors(), "DEF FNP = 3.14159 (sem parametro): sem erros");
    check(lines2[0].statement.defParam.empty(),
          "DEF FNP sem parametro: defParam vazio");

    // uso de FNF numa expressao
    auto lines3 = parse("100 LET Y = FNF(5)\n110 END\n", &parser);
    check(!parser->hasErrors(), "LET Y = FNF(5): sem erros");
    check(lines3[0].statement.letValue->kind == ExprKind::DefFunctionCall,
          "FNF(5): ExprKind::DefFunctionCall");
}

// -------------------------------------------------------------------
// DIM e OPTION BASE (secao 18)
// -------------------------------------------------------------------
void testDimAndOptionBase() {
    Parser* parser = nullptr;
    auto lines = parse("100 DIM A(6), B(10,10)\n110 END\n", &parser);
    check(!parser->hasErrors(), "DIM A(6), B(10,10): sem erros");
    check(lines[0].statement.dimDeclarations.size() == 2, "DIM: duas declaracoes");
    check(lines[0].statement.dimDeclarations[0].bound1 == 6 &&
              lines[0].statement.dimDeclarations[0].bound2 == -1,
          "DIM A(6): array de uma dimensao, limite 6");
    check(lines[0].statement.dimDeclarations[1].bound1 == 10 &&
              lines[0].statement.dimDeclarations[1].bound2 == 10,
          "DIM B(10,10): array de duas dimensoes");

    auto lines2 = parse("100 OPTION BASE 1\n110 END\n", &parser);
    check(!parser->hasErrors(), "OPTION BASE 1: sem erros");
    check(lines2[0].statement.optionBase == 1, "OPTION BASE 1: valor 1");

    parse("100 OPTION BASE 2\n110 END\n", &parser);
    check(parser->hasErrors(), "OPTION BASE 2: deve reportar erro (so aceita 0 ou 1)");
}

// -------------------------------------------------------------------
// PRINT (secao 14), incluindo itens vazios e TAB
// -------------------------------------------------------------------
void testPrint() {
    Parser* parser = nullptr;
    auto lines = parse("100 PRINT X; (X+Z)/2\n110 END\n", &parser);
    check(!parser->hasErrors(), "PRINT X; (X+Z)/2: sem erros");
    check(lines[0].statement.printItems.size() == 2, "PRINT X; (X+Z)/2: dois itens");
    check(lines[0].statement.printItems[0].separator == ';',
          "PRINT X; ...: separador ; apos o primeiro item");

    auto lines2 = parse("100 PRINT ,,,X\n110 END\n", &parser);
    check(!parser->hasErrors(), "PRINT ,,,X: sem erros");
    check(lines2[0].statement.printItems.size() == 4, "PRINT ,,,X: quatro itens");
    check(lines2[0].statement.printItems[0].expr == nullptr,
          "PRINT ,,,X: primeiro item vazio");
    check(lines2[0].statement.printItems[3].expr != nullptr,
          "PRINT ,,,X: quarto item e X");

    auto lines3 = parse("100 PRINT TAB(10); A$; \"IS DONE.\"\n110 END\n", &parser);
    check(!parser->hasErrors(), "PRINT TAB(10); A$; \"IS DONE.\": sem erros");
    check(lines3[0].statement.printItems[0].isTab, "PRINT TAB(10): primeiro item e TAB");

    auto lines4 = parse("100 PRINT P,\n110 END\n", &parser);
    check(!parser->hasErrors(), "PRINT P,: sem erros");
    check(lines4[0].statement.printItems.size() == 1,
          "PRINT P,: um so item (a virgula final nao cria item fantasma)");
    check(lines4[0].statement.printItems[0].separator == ',',
          "PRINT P,: separador da virgula preservado no unico item");
}

// -------------------------------------------------------------------
// INPUT, READ e DATA (secoes 15, 16 e 17)
// -------------------------------------------------------------------
void testInputReadData() {
    Parser* parser = nullptr;
    auto lines = parse("100 INPUT X, A$, Y(2)\n110 END\n", &parser);
    check(!parser->hasErrors(), "INPUT X, A$, Y(2): sem erros");
    check(lines[0].statement.variableList.size() == 3, "INPUT: tres variaveis");
    check(lines[0].statement.variableList[2]->kind == ExprKind::ArrayElement,
          "INPUT ...Y(2): terceira variavel e ArrayElement");

    auto lines2 = parse("100 READ X, Y, Z\n110 END\n", &parser);
    check(!parser->hasErrors(), "READ X, Y, Z: sem erros");
    check(lines2[0].statement.variableList.size() == 3, "READ: tres variaveis");

    // exemplo da propria norma (secao 17.3), com unquoted-string ("PI")
    auto lines3 = parse("100 DATA 3.14159, PI, 5E-10, \",\"\n110 END\n", &parser);
    check(!parser->hasErrors(), "DATA 3.14159, PI, 5E-10, \",\": sem erros");
    auto& items = lines3[0].statement.dataItems;
    check(items.size() == 4, "DATA: quatro itens");
    check(items[0].text == "3.14159" && !items[0].wasQuoted, "DATA: item 1 = 3.14159 (nao veio entre aspas)");
    check(items[1].text == "PI" && !items[1].wasQuoted, "DATA: item 2 = PI (unquoted-string)");
    check(items[3].text == "," && items[3].wasQuoted, "DATA: item 4 = \",\" (veio entre aspas)");
}

// -------------------------------------------------------------------
// RANDOMIZE, STOP, RESTORE
// -------------------------------------------------------------------
void testMiscStatements() {
    Parser* parser = nullptr;
    auto lines = parse("100 RANDOMIZE\n110 STOP\n120 RESTORE\n130 END\n", &parser);
    check(!parser->hasErrors(), "RANDOMIZE / STOP / RESTORE: sem erros");
    check(lines[0].statement.kind == StmtKind::Randomize, "RANDOMIZE: StmtKind::Randomize");
    check(lines[1].statement.kind == StmtKind::Stop, "STOP: StmtKind::Stop");
    check(lines[2].statement.kind == StmtKind::Restore, "RESTORE: StmtKind::Restore");
}

// -------------------------------------------------------------------
// Funcoes embutidas (secao 9) - aridade obrigatoria (exceto RND)
// -------------------------------------------------------------------
void testBuiltinFunctionArity() {
    Parser* parser = nullptr;
    parse("100 LET X = SQR(X^2+Y^2)\n110 END\n", &parser);
    check(!parser->hasErrors(), "SQR(X^2+Y^2): sem erros");

    parse("100 LET X = ABS\n110 END\n", &parser);
    check(parser->hasErrors(), "ABS sem parenteses/argumento: deve reportar erro (secao 9.4)");

    parse("100 LET X = RND\n110 END\n", &parser);
    check(!parser->hasErrors(), "RND sem argumento: forma correta, sem erros");

    parse("100 LET X = RND(1)\n110 END\n", &parser);
    check(parser->hasErrors(), "RND(1): deve reportar erro (RND nao tem argumento)");
}

// -------------------------------------------------------------------
// Regras estruturais do programa (secao 5.4)
// -------------------------------------------------------------------
void testProgramStructureRules() {
    Parser* parser = nullptr;
    parse("100 LET X = 1\n", &parser);
    check(parser->hasErrors(), "programa sem END: deve reportar erro (secao 5.2)");

    parse("100 LET X = 1\n90 END\n", &parser);
    check(parser->hasErrors(), "numeros de linha fora de ordem: deve reportar erro (secao 5.4)");

    parse("100 END\n200 LET X = 1\n", &parser);
    check(parser->hasErrors(), "END no meio do programa: deve reportar erro (secao 5.4)");
}

} // namespace

int main() {
    testFullProgram();
    testLetAndExpressionPrecedence();
    testLetTypeMismatch();
    testGotoGosub();
    testIfThen();
    testOnGoto();
    testForNext();
    testDefFn();
    testDimAndOptionBase();
    testPrint();
    testInputReadData();
    testMiscStatements();
    testBuiltinFunctionArity();
    testProgramStructureRules();

    std::printf("%d/%d testes passaram\n", testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
