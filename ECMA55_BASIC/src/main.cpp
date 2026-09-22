// main.cpp
// Ponto de entrada do interpretador ECMA-55 BASIC.
//
// Uso: ecma55basic [--pt|--en] [programa.bas]
//
// Sem arquivo, abre o ambiente interativo (REPL); --pt/--en escolhem
// o idioma inicial de TODAS as mensagens (ambiente e as de erro do
// lexer/parser/interpretador) - ver LOCALIZE no capitulo 8 do manual.
// Com um arquivo, le, tokeniza (Camada 1), analisa (Camada 2) e
// executa (Camada 3) o programa direto, sem abrir o ambiente.

#include <fstream>
#include <iostream>
#include <sstream>

#include "interpreter.hpp"
#include "lexer.hpp"
#include "locale.hpp"
#include "parser.hpp"
#include "repl.hpp"
#include "version.hpp"

namespace {

int executaArquivo(const std::string& caminho, ecma55::Locale locale) {
    bool pt = locale == ecma55::Locale::Portuguese;
    std::ifstream arquivo(caminho);
    if (!arquivo) {
        std::cerr << (pt ? "Nao foi possivel abrir o arquivo: "
                          : "Could not open the file: ")
                   << caminho << '\n';
        return 1;
    }
    std::ostringstream conteudo;
    conteudo << arquivo.rdbuf();

    ecma55::Lexer lexer(conteudo.str(), locale);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (const auto& erro : lexer.errors()) {
            std::cerr << (pt ? "erro lexico (linha " : "lexical error (line ")
                      << erro.line << "): " << erro.message << '\n';
        }
        return 1;
    }

    ecma55::Parser parser(std::move(tokens), locale);
    auto programa = parser.parseProgram();
    if (parser.hasErrors()) {
        for (const auto& erro : parser.errors()) {
            std::cerr << (pt ? "erro de sintaxe (linha " : "syntax error (line ")
                      << erro.line << "): " << erro.message << '\n';
        }
        return 1;
    }

    ecma55::Interpreter interpretador(programa, std::cin, std::cout, locale);
    interpretador.run();
    if (interpretador.hadFatalError()) {
        std::cerr << interpretador.fatalErrorMessage() << '\n';
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    bool localePortuguese = false; // ambiente abre em ingles por padrao
    std::string arquivo;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--pt") {
            localePortuguese = true;
        } else if (arg == "--en") {
            localePortuguese = false;
        } else {
            arquivo = arg;
        }
    }

    ecma55::Locale locale =
        localePortuguese ? ecma55::Locale::Portuguese : ecma55::Locale::English;

    if (arquivo.empty()) {
        ecma55::Repl repl(std::cin, std::cout);
        repl.setInitialLocale(localePortuguese);
        repl.run();
        return 0;
    }
    return executaArquivo(arquivo, locale);
}
