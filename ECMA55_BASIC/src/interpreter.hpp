// interpreter.hpp
// Interpretador (Camada 3) da ECMA-55 Minimal BASIC.
//
// Executa a arvore sintatica montada pelo parser (Camada 2) linha a
// linha, com um "program counter" (indice na lista de linhas, nao o
// numero de linha BASIC em si) que anda para frente a cada comando e
// pula para outro lugar em GOTO/GOSUB/RETURN/IF-THEN verdadeiro/
// ON...GOTO e no controle de FOR/NEXT.
//
// Decisoes de projeto:
//   - Variavel usada antes de receber valor e um erro fatal (segue a
//     recomendacao da secao 7.6, em vez do costume historico de
//     assumir zero).
//   - "Infinito"/"infinitesimo da maquina" (secao 6.4) usam a faixa
//     nativa do double (~1E±308), nao o minimo de 1E-38/1E+38 exigido
//     pela norma, overflow/underflow praticamente nao aparecem na
//     pratica com essa escolha.
//   - Zonas de impressao do PRINT (secao 14.4): 5 zonas de 16
//     posicoes (80 colunas), confirmado contra o bas55 (Jorge Giner
//     Cordero), que usa essa largura; a norma (secao 14.6) so cita
//     "quinze" como exemplo ilustrativo, nao como exigencia.
//   - Largura de significancia (d) e do exrad (e) na formatacao de
//     numeros (secao 14.4): os minimos da norma, d=6 e e=2.
//   - Prompt do INPUT (secao 15.6): "? ", exatamente a recomendacao
//     da norma.
//   - RND sem RANDOMIZE repete sempre a mesma sequencia (secao 9.6);
//     RANDOMIZE gera uma nova sequencia a partir do relogio.

#ifndef ECMA55_INTERPRETER_HPP
#define ECMA55_INTERPRETER_HPP

#include <cstddef>
#include <istream>
#include <ostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"
#include "locale.hpp"

namespace ecma55 {

class Interpreter {
public:
    // "lines" precisa viver durante toda a execucao (o interpretador so
    // guarda referencias/ponteiros para dentro dela, nao copia a AST).
    Interpreter(const std::vector<Line>& lines, std::istream& in,
                std::ostream& out, Locale locale = Locale::English);

    // Muda o idioma das mensagens de erro depois de construido (usado
    // pelo modo imediato do REPL, cuja instancia persiste entre
    // comandos LOCALIZE - ver repl.hpp).
    void setLocale(Locale locale) { locale_ = locale; }

    // Executa o programa do comeco ao fim (ou ate STOP/END/erro fatal).
    // So pode ser chamado uma vez por instancia.
    void run();

    // Executa um unico comando isolado, sem percorrer o programa (usado
    // pelo modo imediato do ambiente interativo - ver repl.hpp). Reusa
    // o estado atual de variaveis/arrays/DATA desta mesma instancia.
    // Comandos de desvio (GOTO, GOSUB, RETURN, IF-THEN, ON...GOTO,
    // FOR, NEXT) nao fazem sentido fora de um programa numerado e sao
    // recusados com uma mensagem, em vez de executados.
    void executeImmediate(const Stmt& s);

    bool hadFatalError() const { return hadFatalError_; }
    const std::string& fatalErrorMessage() const { return fatalErrorMessage_; }

private:
    // Um valor em tempo de execucao: numero OU string (a norma nao tem
    // um tipo generico "variant", e so um struct com tag, como o
    // resto da AST, para manter o mesmo estilo do projeto).
    struct Value {
        bool isString = false;
        double number = 0.0;
        std::string text;
    };

    struct NumericArray {
        std::vector<double> values;
        std::vector<char> defined; // vector<bool> tem particularidades de
                                   // bitset que atrapalham aqui; char basta
        int bound1 = 0;
        int bound2 = -1; // -1 = array de uma dimensao
    };

    struct DefFunction {
        std::string param; // vazio quando DEF FNx nao tem parametro
        bool hasParam = false;
        const Expr* body = nullptr;
        int definedAtLine = 0;
    };

    struct ActiveFor {
        std::string variable;
        double limit;
        double step;
        std::size_t forIndex;
        std::size_t nextIndex;
    };

    const std::vector<Line>& lines_;
    std::istream& in_;
    std::ostream& out_;
    Locale locale_;

    std::unordered_map<std::string, double> numericVars_;
    std::unordered_map<std::string, bool> numericDefined_;
    std::unordered_map<std::string, std::string> stringVars_;
    std::unordered_map<std::string, bool> stringDefined_;
    std::unordered_map<std::string, NumericArray> arrays_;
    std::unordered_map<std::string, DefFunction> functions_;

    std::vector<DataItem> dataSequence_;
    std::size_t dataPointer_ = 0;

    std::unordered_map<int, std::size_t> lineIndexByNumber_;
    std::unordered_map<std::size_t, std::size_t> forToNext_;

    std::vector<ActiveFor> forStack_;
    std::vector<std::size_t> gosubStack_;
    std::vector<std::string> functionCallStack_; // detecta DEF FN recursiva

    int optionBase_ = 0;
    bool optionBaseSet_ = false;
    int optionBaseLine_ = -1; // linha da OPTION BASE, ou -1 se nao houver

    bool hadFatalError_ = false;
    std::string fatalErrorMessage_;

    std::mt19937 rng_;

    // -- montagem inicial (uma vez, antes de rodar) --
    void buildLineIndex();
    void buildForNextMap();
    void collectDataAndFunctions();

    // -- controle de execucao --
    // Cada execXxx recebe o indice da linha atual e devolve, via
    // "nextPc", o indice da proxima linha a executar. O padrao e
    // nextPc = pc + 1 (comando normal, cai pra linha de baixo); os
    // comandos de desvio (GOTO, GOSUB, RETURN, IF verdadeiro,
    // ON...GOTO, e a entrada/saida de FOR/NEXT) mudam nextPc para
    // outro lugar.
    void execStatement(std::size_t pc, std::size_t& nextPc);
    void execLet(const Stmt& s);
    void execPrint(const Stmt& s);
    void execInput(const Stmt& s);
    void execRead(const Stmt& s);
    void execDim(const Stmt& s);
    void execOption(const Stmt& s);
    void execFor(const Stmt& s, std::size_t pc, std::size_t& nextPc);
    void execNext(const Stmt& s, std::size_t pc, std::size_t& nextPc);
    void execIfThen(const Stmt& s, std::size_t pc, std::size_t& nextPc);
    void execGoto(const Stmt& s, std::size_t& nextPc);
    void execGosub(const Stmt& s, std::size_t pc, std::size_t& nextPc);
    void execReturn(std::size_t pc, std::size_t& nextPc);
    void execOnGoto(const Stmt& s, std::size_t& nextPc);

    std::size_t resolveLineTarget(int lineNumber, int fromLine);
    static bool forConditionMet(double v, double limit, double step);

    // -- expressoes --
    Value evalExpr(const Expr* e);
    Value evalBinary(const Expr* e);
    Value evalUnary(const Expr* e);
    Value evalBuiltinCall(const Expr* e);
    Value evalDefFunctionCall(const Expr* e);
    double evalNumeric(const Expr* e);
    std::string evalString(const Expr* e);

    // -- variaveis e arranjos --
    void assignTo(const Expr* target, const Value& value);
    double getSimpleNumeric(const std::string& name, int line);
    void setSimpleNumeric(const std::string& name, double v, int line = 0);
    const std::string& getStringVar(const std::string& name, int line);
    void setStringVar(const std::string& name, const std::string& v);
    double& arrayElementRef(const Expr* e, bool forWrite);
    NumericArray& getOrCreateArray(const std::string& name, bool twoDimensional,
                                    int line);

    // secao 7.4: o mesmo nome nao pode ser usado ora como variavel
    // simples, ora como arranjo de 1 dimensao, ora como arranjo de 2 -
    // rastreado por nome, fora dos mapas de valor propriamente ditos.
    enum class NameKind { Simple, Array1D, Array2D };
    std::unordered_map<std::string, NameKind> nameKind_;
    void checkAndSetNameKind(const std::string& name, NameKind kind, int line);
    std::unordered_map<std::string, int> dimLineByName_; // secao 18.4: DIM
                                                         // precisa vir
                                                         // antes do 1o uso

    // Verifica, antes de rodar qualquer linha, que todo alvo de GOTO,
    // GOSUB, IF-THEN e ON...GOTO aponta para uma linha que existe de
    // verdade no programa, assim como o resto da validacao inicial,
    // isso evita imprimir qualquer saida parcial antes de um erro que
    // da pra saber sem executar nada (mesmo espirito de buildLineIndex).
    void validateLineReferences();

    // Verifica, tambem antes de rodar qualquer linha, os limites do
    // DIM contra OPTION BASE e os conflitos de "mesmo nome usado como
    // variavel simples e como array" (secao 7.4), percorrendo o
    // programa inteiro estaticamente, do mesmo jeito que um compilador
    // real faria (confirmado comparando com o bas55: esses erros
    // aparecem SEM nenhuma saida impressa antes, ou seja, sao
    // detectados antes de executar qualquer linha, nao so quando a
    // linha com o problema e alcancada).
    void validateStaticSemantics();
    void walkExprForNameKinds(const Expr* e);

    // -- formatacao e impressao (secao 14) --
    std::string formatNumber(double v) const;
    int columnPosition_ = 1; // posicao (1-based) na "linha atual" de saida
    void printText(const std::string& text);
    void printAdvanceToTab(int n);
    void printAdvanceToNextZone();

    // -- DATA/READ (secao 16 e 17) --
    bool nextDataItem(DataItem& out);

    // -- erros --
    [[noreturn]] void fatalError(const std::string& message, int line);
    const char* L(const char* pt, const char* en) const;
};

} // namespace ecma55

#endif // ECMA55_INTERPRETER_HPP
