// repl.cpp

#include "repl.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

#include "lexer.hpp"
#include "parser.hpp"
#include "version.hpp"

namespace ecma55 {

namespace {

std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string toUpperAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    return out;
}

// Separa o numero de linha do resto, como um programa .bas exige (ex.
// "100 LET X = 5" -> numero 100, resto "LET X = 5"). Devolve false se
// a linha nao comecar com digitos.
bool splitLeadingLineNumber(const std::string& raw, int& number, std::string& rest) {
    std::size_t i = 0;
    while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) {
        ++i;
    }
    if (i == 0) {
        return false;
    }
    number = std::stoi(raw.substr(0, i));
    rest = trim(raw.substr(i));
    return true;
}

// Separa o primeiro "token" (comando) do resto da linha, sem exigir
// que seja uma palavra reservada da linguagem. Isto e so o
// reconhecimento de comandos do ambiente (ver repl.hpp).
std::string firstWord(const std::string& s, std::string& rest) {
    std::size_t i = 0;
    while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    rest = trim(s.substr(i));
    return toUpperAscii(s.substr(0, i));
}

std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

struct HelpTopic {
    std::string name;
    std::string syntaxPt;
    std::string syntaxEn;
    std::string pt;
    std::string en;
    std::string examplePt;
    std::string exampleEn;
};

// Descricao, sintaxe e exemplo de cada um dos 20 comandos e 11 funcoes
// da linguagem. A versao condensada, pra terminal, do que o manual
// (docs/05 e docs/06) explica com mais calma. Sintaxe e exemplo tem
// versao PT e EN de verdade: os metanomes da sintaxe ("variavel",
// "expressao"...) e o texto ilustrativo dentro de string/REM dos
// exemplos mudam de idioma junto com LOCALIZE, igual a descricao.
const std::vector<HelpTopic>& helpTopics() {
    static const std::vector<HelpTopic> topics = {
        {"LET", "LET variavel = expressao", "LET variable = expression",
         "Atribui o valor de uma expressao a uma variavel (simples, "
         "de array ou de string). Os dois lados precisam ser do "
         "mesmo tipo.",
         "Assigns the value of an expression to a variable (simple, "
         "array, or string). Both sides must be the same type.",
         "100 LET X = 5\n110 LET N$ = \"MARIA\"",
         "100 LET X = 5\n110 LET N$ = \"MARY\""},
        {"PRINT", "PRINT [item] [(,|;) item]...", "PRINT [item] [(,|;) item]...",
         "Imprime uma lista de itens (numeros, strings, ou TAB(n)), "
         "separados por virgula (pula pra proxima zona) ou "
         "ponto-e-virgula (sem espaco). Sozinho, imprime uma linha em "
         "branco.",
         "Prints a list of items (numbers, strings, or TAB(n)), "
         "separated by a comma (jumps to the next zone) or a "
         "semicolon (no space). Alone, prints a blank line.",
         "100 PRINT \"X =\"; X\n110 PRINT \"A\", \"B\", \"C\"",
         "100 PRINT \"X =\"; X\n110 PRINT \"A\", \"B\", \"C\""},
        {"INPUT", "INPUT variavel [, variavel]...",
         "INPUT variable [, variable]...",
         "Pausa o programa, mostra '?' e espera o usuario digitar um "
         "ou mais valores. Entrada invalida nao e fatal - o programa "
         "pede de novo.",
         "Pauses the program, shows '?' and waits for the user to "
         "enter one or more values. Invalid input is not fatal - the "
         "program asks again.",
         "100 PRINT \"SEU NOME?\"\n110 INPUT N$",
         "100 PRINT \"YOUR NAME?\"\n110 INPUT N$"},
        {"READ", "READ variavel [, variavel]...",
         "READ variable [, variable]...",
         "Le o proximo valor (ou valores) da lista de DATA do "
         "programa e guarda nas variaveis indicadas. Dado insuficiente "
         "ou do tipo errado e erro fatal.",
         "Reads the next value(s) from the program's DATA list into "
         "the given variables. Not enough data, or the wrong type, is "
         "a fatal error.",
         "100 READ A, B\n110 DATA 10, 20",
         "100 READ A, B\n110 DATA 10, 20"},
        {"DATA", "DATA valor [, valor]...", "DATA value [, value]...",
         "Declara uma lista de valores constantes (numero, string "
         "entre aspas, ou string sem aspas), consumida pelo READ. "
         "Pode aparecer em qualquer linha do programa.",
         "Declares a list of constant values (number, quoted string, "
         "or unquoted string), consumed by READ. Can appear on any "
         "line of the program.",
         "100 DATA 3.14159, PI, 5E-10",
         "100 DATA 3.14159, PI, 5E-10"},
        {"RESTORE", "RESTORE", "RESTORE",
         "Volta o ponteiro de leitura do READ para o primeiro valor "
         "de DATA do programa.",
         "Resets the READ pointer back to the first DATA value in "
         "the program.",
         "100 RESTORE\n110 READ A",
         "100 RESTORE\n110 READ A"},
        {"DIM", "DIM nome(limite) [, nome(limite)]...",
         "DIM name(bound) [, name(bound)]...",
         "Declara o tamanho de um array (uma ou duas dimensoes), "
         "antes do primeiro uso. Sem DIM, um array nasce com indices "
         "de 0 a 10.",
         "Declares the size of an array (one or two dimensions), "
         "before its first use. Without DIM, an array defaults to "
         "indices 0 to 10.",
         "100 DIM A(50), B(10, 10)",
         "100 DIM A(50), B(10, 10)"},
        {"OPTION", "OPTION BASE 0|1", "OPTION BASE 0|1",
         "Muda o indice inicial dos arrays de 0 (padrao) para 1. So "
         "pode aparecer uma vez, antes de qualquer DIM ou uso de "
         "array.",
         "Changes the starting array index from 0 (default) to 1. "
         "Can only appear once, before any DIM or array use.",
         "100 OPTION BASE 1\n110 DIM A(10)",
         "100 OPTION BASE 1\n110 DIM A(10)"},
        {"DEF", "DEF FNx = expr   ou   DEF FNx(param) = expr",
         "DEF FNx = expr   or   DEF FNx(param) = expr",
         "Define uma funcao (nome FN + uma letra), com zero ou um "
         "parametro. Precisa vir antes do primeiro uso e nao pode "
         "chamar a si mesma.",
         "Defines a function (name FN + one letter), with zero or "
         "one parameter. Must come before its first use and cannot "
         "call itself.",
         "100 DEF FNQ(X) = X * X\n110 PRINT FNQ(5)",
         "100 DEF FNQ(X) = X * X\n110 PRINT FNQ(5)"},
        {"GOTO", "GOTO linha   (tambem: GO TO linha)",
         "GOTO line   (also: GO TO line)",
         "Transfere a execucao direto para o numero de linha "
         "indicado, sem condicao nenhuma.",
         "Transfers execution straight to the given line number, "
         "unconditionally.",
         "100 GOTO 200",
         "100 GOTO 200"},
        {"GOSUB", "GOSUB linha   (tambem: GO SUB linha)",
         "GOSUB line   (also: GO SUB line)",
         "Chama uma sub-rotina na linha indicada; RETURN volta pra "
         "linha logo depois deste GOSUB.",
         "Calls a subroutine at the given line; RETURN comes back to "
         "the line right after this GOSUB.",
         "100 GOSUB 500\n...\n500 REM SUB-ROTINA\n510 RETURN",
         "100 GOSUB 500\n...\n500 REM SUBROUTINE\n510 RETURN"},
        {"RETURN", "RETURN", "RETURN",
         "Volta da sub-rotina para a linha logo depois do GOSUB que a "
         "chamou. Sem um GOSUB correspondente, e erro fatal.",
         "Returns from a subroutine to the line right after the "
         "GOSUB that called it. With no matching GOSUB, it's a fatal "
         "error.",
         "500 LET R = N * 2\n510 RETURN",
         "500 LET R = N * 2\n510 RETURN"},
        {"IF", "IF condicao THEN linha", "IF condition THEN line",
         "Testa uma comparacao; se verdadeira, desvia para a linha "
         "indicada. Se falsa, a execucao so continua na linha "
         "seguinte. Nao existe ELSE.",
         "Tests a comparison; if true, jumps to the given line. If "
         "false, execution just continues on the next line. There is "
         "no ELSE.",
         "100 IF X > 0 THEN 200",
         "100 IF X > 0 THEN 200"},
        {"ON", "ON expressao GOTO linha [, linha]...",
         "ON expression GOTO line [, line]...",
         "Arredonda a expressao para inteiro e usa esse numero para "
         "escolher uma linha da lista (1a = primeira linha, etc.). "
         "Fora da faixa e erro fatal.",
         "Rounds the expression to an integer and uses it to pick a "
         "line from the list (1 = first line, etc.). Out of range is "
         "a fatal error.",
         "100 ON N GOTO 200, 300, 400",
         "100 ON N GOTO 200, 300, 400"},
        {"FOR", "FOR variavel = inicio TO limite [STEP incremento]",
         "FOR variable = start TO limit [STEP increment]",
         "Repete o bloco ate o NEXT correspondente, do inicio ao "
         "limite (inclusive), de incremento em incremento (1 se "
         "omitido). Testado antes de cada volta.",
         "Repeats the block up to the matching NEXT, from start to "
         "limit (inclusive), by increment (1 if omitted). Tested "
         "before each pass.",
         "100 FOR I = 1 TO 5\n110   PRINT I\n120 NEXT I",
         "100 FOR I = 1 TO 5\n110   PRINT I\n120 NEXT I"},
        {"NEXT", "NEXT variavel", "NEXT variable",
         "Fecha o laco FOR aberto com a mesma variavel: incrementa, "
         "testa a condicao, e volta ao corpo do laco ou segue em "
         "frente.",
         "Closes the FOR loop opened with the same variable: "
         "increments, tests the condition, and either loops back or "
         "moves on.",
         "120 NEXT I",
         "120 NEXT I"},
        {"RANDOMIZE", "RANDOMIZE", "RANDOMIZE",
         "Reinicia a sequencia de RND a partir de uma semente "
         "imprevisivel, para uma sequencia diferente a cada execucao.",
         "Reseeds the RND sequence unpredictably, for a different "
         "sequence on every run.",
         "100 RANDOMIZE\n110 PRINT INT(RND * 6) + 1",
         "100 RANDOMIZE\n110 PRINT INT(RND * 6) + 1"},
        {"STOP", "STOP", "STOP",
         "Interrompe a execucao do programa imediatamente. Pode "
         "aparecer quantas vezes forem precisas.",
         "Stops the program's execution immediately. Can appear as "
         "many times as needed.",
         "100 STOP",
         "100 STOP"},
        {"END", "END", "END",
         "Marca o fim do texto do programa. Todo programa precisa de "
         "exatamente um END, e ele precisa ser a ultima linha.",
         "Marks the end of the program's text. Every program needs "
         "exactly one END, and it must be the last line.",
         "999 END",
         "999 END"},
        {"REM", "REM texto livre", "REM free text",
         "Marca o resto da linha como comentario - nao e executado.",
         "Marks the rest of the line as a comment - not executed.",
         "100 REM ESTE PROGRAMA CALCULA UMA MEDIA",
         "100 REM THIS PROGRAM CALCULATES AN AVERAGE"},
        {"ABS", "ABS(X)", "ABS(X)", "Valor absoluto de X.",
         "Absolute value of X.",
         "PRINT ABS(-5)\nREM RESULTADO: 5", "PRINT ABS(-5)\nREM RESULT: 5"},
        {"ATN", "ATN(X)", "ATN(X)", "Arco-tangente de X, em radianos.",
         "Arctangent of X, in radians.",
         "PRINT ATN(1) * 4\nREM RESULTADO: PI",
         "PRINT ATN(1) * 4\nREM RESULT: PI"},
        {"COS", "COS(X)", "COS(X)", "Cosseno de X (X em radianos).",
         "Cosine of X (X in radians).",
         "PRINT COS(0)\nREM RESULTADO: 1", "PRINT COS(0)\nREM RESULT: 1"},
        {"EXP", "EXP(X)", "EXP(X)", "'e' elevado a X.", "'e' raised to X.",
         "PRINT EXP(1)\nREM RESULTADO: 2.7182818",
         "PRINT EXP(1)\nREM RESULT: 2.7182818"},
        {"INT", "INT(X)", "INT(X)",
         "Maior inteiro nao maior que X (arredonda para baixo, "
         "inclusive em negativos).",
         "Largest integer not greater than X (rounds down, including "
         "for negative numbers).",
         "PRINT INT(3.9)\nREM RESULTADO: 3\nPRINT INT(-3.1)\nREM RESULTADO: -4",
         "PRINT INT(3.9)\nREM RESULT: 3\nPRINT INT(-3.1)\nREM RESULT: -4"},
        {"LOG", "LOG(X)", "LOG(X)",
         "Logaritmo natural de X. X precisa ser positivo (senao, erro "
         "fatal).",
         "Natural logarithm of X. X must be positive (otherwise, "
         "fatal error).",
         "PRINT LOG(EXP(1))\nREM RESULTADO: 1",
         "PRINT LOG(EXP(1))\nREM RESULT: 1"},
        {"RND", "RND", "RND",
         "Um numero pseudoaleatorio entre 0 (incluso) e 1 (exclusivo). "
         "Sem RANDOMIZE, repete sempre a mesma sequencia.",
         "A pseudo-random number between 0 (inclusive) and 1 "
         "(exclusive). Without RANDOMIZE, always repeats the same "
         "sequence.",
         "PRINT RND", "PRINT RND"},
        {"SGN", "SGN(X)", "SGN(X)",
         "1 se X e positivo, -1 se negativo, 0 se X e zero.",
         "1 if X is positive, -1 if negative, 0 if X is zero.",
         "PRINT SGN(-7)\nREM RESULTADO: -1", "PRINT SGN(-7)\nREM RESULT: -1"},
        {"SIN", "SIN(X)", "SIN(X)", "Seno de X (X em radianos).",
         "Sine of X (X in radians).",
         "PRINT SIN(0)\nREM RESULTADO: 0", "PRINT SIN(0)\nREM RESULT: 0"},
        {"SQR", "SQR(X)", "SQR(X)",
         "Raiz quadrada de X. X nao pode ser negativo (senao, erro "
         "fatal).",
         "Square root of X. X cannot be negative (otherwise, fatal "
         "error).",
         "PRINT SQR(16)\nREM RESULTADO: 4", "PRINT SQR(16)\nREM RESULT: 4"},
        {"TAN", "TAN(X)", "TAN(X)", "Tangente de X (X em radianos).",
         "Tangent of X (X in radians).",
         "PRINT TAN(0)\nREM RESULTADO: 0", "PRINT TAN(0)\nREM RESULT: 0"},
    };
    return topics;
}
} // namespace

Repl::Repl(std::istream& in, std::ostream& out)
    : in_(in), out_(out), workspace_(immediateHistory_, in, out, locale_) {}

const char* Repl::L(const char* pt, const char* en) const {
    return locale_ == Locale::Portuguese ? pt : en;
}

void Repl::setInitialLocale(bool portuguese) {
    locale_ = portuguese ? Locale::Portuguese : Locale::English;
    workspace_.setLocale(locale_);
}

void Repl::printBanner() {
    out_ << "ECMA-55 BASIC [AM-42] - " << kVersion << '\n';
    out_ << L("(c) 2016-2018 (2026 - Revisado)\n",
              "(c) 2016-2018 (2026 - Revised)\n");
    out_ << L("Augusto Manzano - Todos os direitos reservados\n",
              "Augusto Manzano - All rights reserved\n");
    out_ << L("Escreva HELP para ver os comandos do ambiente.\n",
              "Type HELP to see the environment commands.\n");
}

void Repl::printHelp() {
    out_ << L(
        "Comandos do ambiente (nao fazem parte da norma ECMA-55,\n"
        "sao ferramentas deste interpretador):\n"
        "  numero comando   entra ou substitui uma linha do programa\n"
        "  numero           (sem nada depois) apaga aquela linha\n"
        "  RUN              executa o programa armazenado\n"
        "  LIST             lista o programa armazenado\n"
        "  NEW              apaga o programa armazenado\n"
        "  LOAD \"arquivo\"   le um programa de um arquivo .bas\n"
        "  SAVE \"arquivo\"   grava o programa armazenado num arquivo\n"
        "  LOCALIZE PT|EN   troca o idioma das mensagens do ambiente\n"
        "  SYSTEM           sai do ambiente\n"
        "  HELP             mostra esta mensagem\n"
        "  HELP comando     explica um comando ou funcao (ex.: HELP PRINT)\n"
        "Qualquer outra linha, sem numero na frente, roda na hora\n"
        "(modo imediato) - ex.: PRINT 2+2\n",

        "Environment commands (not part of the ECMA-55 standard,\n"
        "these are tools of this interpreter):\n"
        "  number statement enters or replaces a program line\n"
        "  number           (with nothing after) deletes that line\n"
        "  RUN              runs the stored program\n"
        "  LIST             lists the stored program\n"
        "  NEW              clears the stored program\n"
        "  LOAD \"file\"      reads a program from a .bas file\n"
        "  SAVE \"file\"      writes the stored program to a file\n"
        "  LOCALIZE PT|EN   switches the environment message language\n"
        "  SYSTEM           exits the environment\n"
        "  HELP             shows this message\n"
        "  HELP statement   explains a statement or function (e.g.: HELP PRINT)\n"
        "Any other line, with no number in front, runs right away\n"
        "(immediate mode) - e.g.: PRINT 2+2\n");
}

void Repl::cmdHelpTopic(const std::string& topic) {
    for (const auto& t : helpTopics()) {
        if (t.name == topic) {
            out_ << t.name << " - " << L(t.pt.c_str(), t.en.c_str()) << "\n\n";
            out_ << L("Sintaxe:\n  ", "Syntax:\n  ")
                 << L(t.syntaxPt.c_str(), t.syntaxEn.c_str()) << "\n\n";
            out_ << L("Exemplo:\n  ", "Example:\n  ")
                 << L(t.examplePt.c_str(), t.exampleEn.c_str()) << "\n";
            return;
        }
    }
    out_ << L("Comando ou funcao desconhecido: ", "Unknown statement or function: ")
         << topic
         << L(". Escreva HELP para ver a lista.\n",
              ". Type HELP to see the list.\n");
}

void Repl::run() {
    printBanner();
    std::string line;
    for (;;) {
        out_ << "] ";
        if (!std::getline(in_, line)) {
            out_ << '\n';
            break;
        }
        handleLine(line);
    }
}

void Repl::handleLine(const std::string& rawLine) {
    std::string trimmed = trim(rawLine);
    if (trimmed.empty()) {
        return;
    }

    int number = 0;
    std::string rest;
    if (splitLeadingLineNumber(trimmed, number, rest)) {
        storeOrDeleteLine(number, rest);
        return;
    }

    std::string arg;
    std::string command = firstWord(trimmed, arg);
    if (command == "RUN") {
        cmdRun();
    } else if (command == "LIST") {
        cmdList();
    } else if (command == "NEW") {
        cmdNew();
    } else if (command == "LOAD") {
        cmdLoad(stripQuotes(arg));
    } else if (command == "SAVE") {
        cmdSave(stripQuotes(arg));
    } else if (command == "LOCALIZE") {
        cmdLocalize(toUpperAscii(arg));
    } else if (command == "HELP") {
        if (arg.empty()) {
            printHelp();
        } else {
            cmdHelpTopic(toUpperAscii(arg));
        }
    } else if (command == "SYSTEM" || command == "BYE" || command == "EXIT") {
        in_.setstate(std::ios::eofbit); // encerra o laco em run()
    } else {
        runImmediateStatement(trimmed);
    }
}

void Repl::storeOrDeleteLine(int number, const std::string& rest) {
    if (rest.empty()) {
        programLines_.erase(number); // secao 5.6: linha so com numero apaga a linha
    } else {
        programLines_[number] = rest;
    }
}

std::string Repl::buildProgramText() const {
    std::ostringstream text;
    for (const auto& [number, rest] : programLines_) {
        text << number << ' ' << rest << '\n';
    }
    return text.str();
}

void Repl::cmdRun() {
    if (programLines_.empty()) {
        out_ << L("Nao ha programa armazenado (use numero+comando para "
                  "digitar um, ou LOAD).\n",
                  "No program is stored (use number+statement to enter "
                  "one, or LOAD).\n");
        return;
    }
    Lexer lexer(buildProgramText(), locale_);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (const auto& erro : lexer.errors()) {
            out_ << L("erro lexico (linha ", "lexical error (line ")
                 << erro.line << "): " << erro.message << '\n';
        }
        return;
    }
    Parser parser(std::move(tokens), locale_);
    auto program = parser.parseProgram();
    if (parser.hasErrors()) {
        for (const auto& erro : parser.errors()) {
            out_ << L("erro de sintaxe (linha ", "syntax error (line ")
                 << erro.line << "): " << erro.message << '\n';
        }
        return;
    }
    Interpreter interpreter(program, in_, out_, locale_); // RUN sempre comeca
                                                            // com variaveis
                                                            // zeradas
    interpreter.run();
    if (interpreter.hadFatalError()) {
        out_ << interpreter.fatalErrorMessage() << '\n';
    }
}

void Repl::cmdList() const {
    for (const auto& [number, rest] : programLines_) {
        out_ << number << ' ' << rest << '\n';
    }
}

void Repl::cmdNew() {
    programLines_.clear();
}

void Repl::cmdLoad(const std::string& arg) {
    if (arg.empty()) {
        out_ << L("Uso: LOAD \"arquivo.bas\"\n", "Usage: LOAD \"file.bas\"\n");
        return;
    }
    std::ifstream file(arg);
    if (!file) {
        out_ << L("Nao foi possivel abrir o arquivo: ",
                  "Could not open the file: ")
             << arg << '\n';
        return;
    }
    std::map<int, std::string> loaded;
    std::string rawLine;
    while (std::getline(file, rawLine)) {
        std::string t = trim(rawLine);
        if (t.empty()) {
            continue;
        }
        int number = 0;
        std::string rest;
        if (!splitLeadingLineNumber(t, number, rest) || rest.empty()) {
            out_ << L("Linha ignorada (sem numero valido): ",
                      "Line ignored (no valid number): ")
                 << rawLine << '\n';
            continue;
        }
        loaded[number] = rest;
    }
    programLines_ = std::move(loaded);
    out_ << L("Carregado: ", "Loaded: ") << arg << '\n';
}

void Repl::cmdSave(const std::string& arg) const {
    if (arg.empty()) {
        out_ << L("Uso: SAVE \"arquivo.bas\"\n", "Usage: SAVE \"file.bas\"\n");
        return;
    }
    std::ofstream file(arg);
    if (!file) {
        out_ << L("Nao foi possivel criar o arquivo: ",
                  "Could not create the file: ")
             << arg << '\n';
        return;
    }
    file << buildProgramText();
    out_ << L("Gravado: ", "Saved: ") << arg << '\n';
}

void Repl::cmdLocalize(const std::string& arg) {
    if (arg == "PT") {
        locale_ = Locale::Portuguese;
        workspace_.setLocale(locale_);
    } else if (arg == "EN") {
        locale_ = Locale::English;
        workspace_.setLocale(locale_);
    } else {
        out_ << L("Uso: LOCALIZE PT ou LOCALIZE EN\n",
                  "Usage: LOCALIZE PT or LOCALIZE EN\n");
    }
}

void Repl::runImmediateStatement(const std::string& text) {
    // Empacota como um "programa" de duas linhas (a digitada + um END
    // sintetico) so para reaproveitar o parser inteiro sem precisar de
    // uma API separada para "uma linha avulsa". Ver repl.hpp.
    std::ostringstream synthetic;
    synthetic << "1 " << text << "\n2 END\n";

    Lexer lexer(synthetic.str(), locale_);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (const auto& erro : lexer.errors()) {
            out_ << L("erro lexico: ", "lexical error: ") << erro.message
                 << '\n';
        }
        return;
    }
    Parser parser(std::move(tokens), locale_);
    auto parsed = parser.parseProgram();
    if (parser.hasErrors()) {
        for (const auto& erro : parser.errors()) {
            out_ << L("erro de sintaxe: ", "syntax error: ") << erro.message
                 << '\n';
        }
        return;
    }
    immediateHistory_.push_back(std::move(parsed.front()));
    workspace_.executeImmediate(immediateHistory_.back().statement);
}

} // namespace ecma55
