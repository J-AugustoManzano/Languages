// repl.hpp
// Ambiente interativo (REPL) da ECMA-55 Minimal BASIC.
//
// Isto e ferramenta do AMBIENTE, nao parte da linguagem BASIC em si,
// a norma (secao 3.7) so define "modo interativo" em termos gerais,
// sem especificar comandos de ambiente. Os comandos abaixo (RUN, LIST,
// NEW, LOAD, SAVE, SYSTEM, HELP, LOCALIZE) seguem a convencao universal
// dos BASICs historicos da epoca (Dartmouth, GW-BASIC, etc.), presente
// em praticamente toda implementacao real, mesmo sem constar da
// gramatica formal.
//
// Duas linhas de execucao, propositalmente separadas para manter o
// estado simples e previsivel:
//   - RUN monta o programa armazenado inteiro e executa numa instancia
//     nova do interpretador (zera variaveis, arrays, DATA etc. A mesma
//     convencao classica de que RUN comeca do zero).
//   - Comandos escritos sem numero de linha (modo imediato) rodam
//     numa instancia separada e persistente, que dura a sessao
//     inteira, mas NAO e afetada por um RUN nem o afeta. Ou seja,
//     variaveis do modo imediato nao sobrevivem a um RUN e
//     vice-versa. E uma limitacao consciente desta primeira entrega
//     do ambiente, nao um descuido.
//
// LOCALIZE PT|EN troca o idioma de TODAS as mensagens do interpretador
// (banner, HELP, status de RUN/LOAD/SAVE, avisos de INPUT, e as
// mensagens de erro do lexer/parser/interpretador em si).

#ifndef ECMA55_REPL_HPP
#define ECMA55_REPL_HPP

#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "ast.hpp"
#include "interpreter.hpp"
#include "locale.hpp"

namespace ecma55 {

class Repl {
public:
    Repl(std::istream& in, std::ostream& out);

    // Roda o laco principal do ambiente ate SYSTEM ou fim da entrada
    // (Ctrl-D).
    void run();

    // Idioma inicial das mensagens do ambiente (banner, HELP, etc.) -
    // ver LOCALIZE no repl.cpp. Chame antes de run().
    void setInitialLocale(bool portuguese);

private:
    std::istream& in_;
    std::ostream& out_;
    Locale locale_ = Locale::English;

    std::map<int, std::string> programLines_; // numero -> resto da linha
    std::vector<Line> immediateHistory_;      // mantem vivos os Stmts do
                                              // modo imediato (DEF FN etc.)
    Interpreter workspace_;                   // ambiente do modo imediato

    void printBanner();
    void printHelp();
    void handleLine(const std::string& rawLine);
    void storeOrDeleteLine(int number, const std::string& rest);
    void cmdRun();
    void cmdList() const;
    void cmdNew();
    void cmdLoad(const std::string& arg);
    void cmdSave(const std::string& arg) const;
    void cmdLocalize(const std::string& arg);
    void cmdHelpTopic(const std::string& topic);
    void runImmediateStatement(const std::string& text);

    // Devolve pt ou en conforme locale_, forma curta de escrever
    // mensagens bilingues sem duplicar a estrutura toda do metodo.
    const char* L(const char* pt, const char* en) const;

    std::string buildProgramText() const;
};

} // namespace ecma55

#endif // ECMA55_REPL_HPP
