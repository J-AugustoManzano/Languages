// interpreter.cpp

#include "interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace ecma55 {

namespace {

// Usada so para desenrolar a pilha de chamadas C++ ate o laco
// principal em run() quando ocorre um erro fatal (secao 3.5/3.4: a
// norma exige que erros fatais terminem o programa).
struct FatalErrorSignal {};

// secao 16.4: um datum sem aspas so pode ir para uma variavel numerica
// se "for uma representacao numerica valida", mesma forma de
// numeric-constant da secao 6.2, sem espacos.
bool looksNumeric(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (text[i] == '+' || text[i] == '-') {
        ++i;
    }
    bool sawDigit = false;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        ++i;
        sawDigit = true;
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            sawDigit = true;
        }
    }
    if (!sawDigit) {
        return false;
    }
    if (i < text.size() && (text[i] == 'E' || text[i] == 'e')) {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            ++i;
        }
        bool sawExpDigit = false;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            sawExpDigit = true;
        }
        if (!sawExpDigit) {
            return false;
        }
    }
    return i == text.size();
}

// Separa uma resposta de INPUT (ou um datum bruto de DATA) em itens
// por virgula, respeitando strings entre aspas, mesma ideia da
// secao 15.2 (input-list = padded-datum (comma padded-datum)*) e da
// secao 17 (DATA), reaproveitada aqui em tempo de execucao.
std::vector<DataItem> splitReplyList(const std::string& raw) {
    std::vector<DataItem> items;
    std::size_t i = 0;
    std::size_t n = raw.size();
    for (;;) {
        while (i < n && raw[i] == ' ') {
            ++i;
        }
        DataItem item;
        if (i < n && raw[i] == '"') {
            ++i;
            std::string content;
            while (i < n && raw[i] != '"') {
                content += raw[i];
                ++i;
            }
            if (i < n && raw[i] == '"') {
                ++i;
            }
            item.text = content;
            item.wasQuoted = true;
            while (i < n && raw[i] == ' ') {
                ++i;
            }
        } else {
            std::string content;
            while (i < n && raw[i] != ',') {
                content += raw[i];
                ++i;
            }
            std::size_t end = content.size();
            while (end > 0 && content[end - 1] == ' ') {
                --end;
            }
            item.text = content.substr(0, end);
            item.wasQuoted = false;
        }
        items.push_back(std::move(item));
        if (i < n && raw[i] == ',') {
            ++i;
            continue;
        }
        break;
    }
    return items;
}

constexpr int kPrintZoneWidth = 16;
constexpr int kPrintZoneCount = 5;
constexpr int kMargin = kPrintZoneWidth * kPrintZoneCount; // 80 colunas -
    // largura de terminal padrao historica; confirmado batendo a saida
    // contra o bas55 (Jorge Giner Cordero), que usa zonas de 16
    // colunas. A norma (secao 14.6) so da "quinze" como exemplo
    // ilustrativo, nao como exigencia.

// Converte o texto de um datum numerico (DATA/INPUT) para double. Um
// valor fora da faixa do double e overflow nao-fatal (secao 6.5/16.5).
// Substitui por infinito com o sinal correto, nao trava o programa.
double parseDataNumber(const std::string& text) {
    try {
        return std::stod(text);
    } catch (const std::out_of_range&) {
        bool negative = !text.empty() && text[0] == '-';
        auto epos = text.find('E');
        bool isUnderflow = epos != std::string::npos && epos + 1 < text.size() &&
                            text[epos + 1] == '-';
        if (isUnderflow) {
            return 0.0; // expoente muito negativo -> underflow, nao overflow
        }
        return negative ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();
    }
}

// secao 10.4: "A function definition may refer to other defined
// functions, but not to the function being defined." Verificado
// estaticamente (antes de rodar qualquer linha) para o caso de
// auto-referencia direta, o mesmo espirito de validateLineReferences.
bool exprReferencesFunction(const Expr* e, const std::string& name) {
    if (e == nullptr) {
        return false;
    }
    if (e->kind == ExprKind::DefFunctionCall && e->defFunctionName == name) {
        return true;
    }
    for (const auto& sub : e->subscripts) {
        if (exprReferencesFunction(sub.get(), name)) {
            return true;
        }
    }
    return exprReferencesFunction(e->left.get(), name) ||
           exprReferencesFunction(e->right.get(), name);
}

} // namespace

Interpreter::Interpreter(const std::vector<Line>& lines, std::istream& in,
                         std::ostream& out, Locale locale)
    : lines_(lines), in_(in), out_(out), locale_(locale), rng_(12345u) {
    // semente fixa: sem RANDOMIZE, RND repete sempre a mesma sequencia
    // (secao 9.6)
}

const char* Interpreter::L(const char* pt, const char* en) const {
    return locale_ == Locale::Portuguese ? pt : en;
}

void Interpreter::run() {
    std::size_t pc = 0;
    try {
        buildLineIndex();
        buildForNextMap();
        collectDataAndFunctions();
        validateLineReferences();
        validateStaticSemantics();

        if (lines_.empty()) {
            return;
        }

        while (pc < lines_.size()) {
            const Stmt& s = lines_[pc].statement;
            if (s.kind == StmtKind::End || s.kind == StmtKind::Stop) {
                break;
            }
            std::size_t nextPc = pc + 1;
            execStatement(pc, nextPc);
            pc = nextPc;
        }
    } catch (const FatalErrorSignal&) {
        // hadFatalError_ e fatalErrorMessage_ ja foram preenchidos em
        // fatalError() antes de lancar o sinal
    }
}

void Interpreter::executeImmediate(const Stmt& s) {
    try {
        switch (s.kind) {
            case StmtKind::Let:
                execLet(s);
                break;
            case StmtKind::Print:
                execPrint(s);
                break;
            case StmtKind::Input:
                execInput(s);
                break;
            case StmtKind::Read:
                execRead(s);
                break;
            case StmtKind::Dim:
                execDim(s);
                break;
            case StmtKind::Option:
                execOption(s);
                break;
            case StmtKind::Data:
                for (const auto& item : s.dataItems) {
                    dataSequence_.push_back(item);
                }
                break;
            case StmtKind::Def: {
                DefFunction fn;
                fn.hasParam = !s.defParam.empty();
                fn.param = s.defParam;
                fn.body = s.defBody.get();
                fn.definedAtLine = s.line;
                functions_[s.defName] = fn;
                break;
            }
            case StmtKind::Randomize:
                rng_.seed(static_cast<unsigned int>(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
                break;
            case StmtKind::Restore:
                dataPointer_ = 0;
                break;
            case StmtKind::Remark:
                break;
            case StmtKind::Stop:
            case StmtKind::End:
                break;
            default:
                out_ << L("Este comando so funciona dentro de um programa "
                          "numerado (escreva RUN).\n",
                          "This statement only works inside a numbered "
                          "program (type RUN).\n");
        }
    } catch (const FatalErrorSignal&) {
        out_ << fatalErrorMessage_ << '\n';
        hadFatalError_ = false; // um erro no modo imediato nao derruba a sessao
        // o modo imediato reusa esta mesma instancia entre chamadas
        // (ao contrario do RUN, que sempre comeca do zero), entao uma
        // pilha de chamada de FN deixada suja por um erro no meio de
        // uma avaliacao precisa ser limpa aqui
        functionCallStack_.clear();
    }
}

// ---------------------------------------------------------------------
// Montagem inicial
// ---------------------------------------------------------------------

void Interpreter::buildLineIndex() {
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        lineIndexByNumber_[lines_[i].number] = i;
    }
}

void Interpreter::buildForNextMap() {
    std::vector<std::size_t> stack;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].statement.kind == StmtKind::For) {
            stack.push_back(i);
        } else if (lines_[i].statement.kind == StmtKind::Next) {
            if (!stack.empty()) {
                forToNext_[stack.back()] = i;
                stack.pop_back();
            }
        }
    }
}

void Interpreter::collectDataAndFunctions() {
    for (const auto& ln : lines_) {
        const Stmt& s = ln.statement;
        if (s.kind == StmtKind::Data) {
            for (const auto& item : s.dataItems) {
                dataSequence_.push_back(item);
            }
        } else if (s.kind == StmtKind::Def) {
            if (functions_.count(s.defName)) {
                // secao 10.4: "A function shall be defined at most once
                // in a program." Verificado antes de rodar qualquer
                // linha (mesma fase de montagem que buildLineIndex),
                // entao nao ha saida parcial antes deste erro.
                fatalError(L("a funcao ", "function ") + s.defName +
                               L(" ja foi definida antes (secao 10.4)",
                                 " has already been defined (section 10.4)"),
                           s.line);
            }
            DefFunction fn;
            fn.hasParam = !s.defParam.empty();
            fn.param = s.defParam;
            fn.body = s.defBody.get();
            fn.definedAtLine = s.line;
            if (exprReferencesFunction(fn.body, s.defName)) {
                fatalError(L("a funcao ", "function ") + s.defName +
                               L(" nao pode chamar a si mesma, direta ou "
                                 "indiretamente (secao 10.4)",
                                 " cannot call itself, directly or "
                                 "indirectly (section 10.4)"),
                           s.line);
            }
            functions_[s.defName] = fn;
        }
    }
}

void Interpreter::validateLineReferences() {
    // Blocos FOR que envolvem cada linha (para a checagem de "GOTO/
    // GOSUB/IF-THEN/ON-GOTO nao pode saltar para DENTRO de um bloco
    // FOR vindo de fora dele", secao 13.4). enclosingFor[i] guarda o
    // indice de cada FOR cujo corpo contem a linha i.
    std::vector<std::vector<std::size_t>> enclosingFor(lines_.size());
    for (const auto& pair : forToNext_) {
        std::size_t forIdx = pair.first;
        std::size_t nextIdx = pair.second;
        for (std::size_t i = forIdx + 1; i <= nextIdx && i < lines_.size(); ++i) {
            enclosingFor[i].push_back(forIdx);
        }
    }
    auto isInside = [&enclosingFor](std::size_t idx, std::size_t forIdx) {
        const auto& v = enclosingFor[idx];
        return std::find(v.begin(), v.end(), forIdx) != v.end();
    };
    auto checkJump = [&](std::size_t sourceIdx, int targetLineNumber) {
        std::size_t targetIdx = resolveLineTarget(targetLineNumber,
                                                    lines_[sourceIdx].statement.line);
        for (std::size_t forIdx : enclosingFor[targetIdx]) {
            if (!isInside(sourceIdx, forIdx)) {
                // secao 13.4: "A program shall not transfer control
                // into a for-body ... from outside that for-body."
                fatalError(L("desvio de controle para dentro de um bloco "
                             "FOR vindo de fora dele (secao 13.4)",
                             "control transfer into a FOR block from "
                             "outside it (section 13.4)"),
                           lines_[sourceIdx].statement.line);
            }
        }
    };

    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const Stmt& s = lines_[i].statement;
        switch (s.kind) {
            case StmtKind::Goto:
            case StmtKind::Gosub:
                checkJump(i, s.targetLine);
                break;
            case StmtKind::IfThen:
                checkJump(i, s.ifTargetLine);
                break;
            case StmtKind::OnGoto:
                for (int target : s.onTargets) {
                    checkJump(i, target);
                }
                break;
            default:
                break;
        }
    }
}

void Interpreter::walkExprForNameKinds(const Expr* e) {
    if (e == nullptr) {
        return;
    }
    switch (e->kind) {
        case ExprKind::SimpleVariable:
            checkAndSetNameKind(e->name, NameKind::Simple, e->line);
            break;
        case ExprKind::ArrayElement:
            if (optionBaseLine_ != -1 && e->line < optionBaseLine_) {
                // secao 18.4: OPTION BASE precisa vir antes de
                // qualquer DIM ou referencia a array no programa
                fatalError(L("OPTION BASE precisa vir antes de qualquer "
                             "DIM ou uso de array (secao 18.4)",
                             "OPTION BASE must come before any DIM or "
                             "array use (section 18.4)"),
                           e->line);
            }
            {
                auto dimIt = dimLineByName_.find(e->name);
                if (dimIt != dimLineByName_.end() && e->line < dimIt->second) {
                    // secao 18.4: "The dimension-statement, if any, for
                    // an array shall precede the first reference to
                    // that array."
                    fatalError(L("o array ", "array ") + e->name +
                                   L(" e referenciado antes do seu DIM "
                                     "(secao 18.4)",
                                     " is referenced before its DIM "
                                     "(section 18.4)"),
                               e->line);
                }
            }
            checkAndSetNameKind(
                e->name,
                e->subscripts.size() == 2 ? NameKind::Array2D : NameKind::Array1D,
                e->line);
            for (const auto& sub : e->subscripts) {
                walkExprForNameKinds(sub.get());
            }
            break;
        case ExprKind::StringVariable:
        case ExprKind::NumberLiteral:
        case ExprKind::StringLiteral:
            break; // string e numero nao entram nessa checagem
        case ExprKind::BuiltinCall:
            for (const auto& sub : e->subscripts) {
                walkExprForNameKinds(sub.get());
            }
            break;
        case ExprKind::DefFunctionCall: {
            // secao 10.4: "A function definition shall occur in a
            // lower numbered line than that of the first reference to
            // the function." (a existencia da funcao em si ja foi
            // conferida em tempo de execucao, na hora de chamar -
            // aqui so a ORDEM das linhas.)
            auto it = functions_.find(e->defFunctionName);
            if (it == functions_.end()) {
                fatalError(L("a funcao ", "function ") + e->defFunctionName +
                               L(" nao foi definida com DEF (secao 10.4)",
                                 " was not defined with DEF (section 10.4)"),
                           e->line);
            }
            if (e->line < it->second.definedAtLine) {
                fatalError(L("a funcao ", "function ") + e->defFunctionName +
                               L(" e usada antes de ser definida com DEF "
                                 "(secao 10.4)",
                                 " is used before being defined with DEF "
                                 "(section 10.4)"),
                           e->line);
            }
            {
                bool callHasArg = !e->subscripts.empty();
                if (it->second.hasParam != callHasArg) {
                    fatalError(L("o numero de argumentos de ",
                                 "the number of arguments of ") +
                                   e->defFunctionName +
                                   L(" nao bate com a definicao (secao 8.4)",
                                     " does not match the definition "
                                     "(section 8.4)"),
                               e->line);
                }
            }
            for (const auto& sub : e->subscripts) {
                walkExprForNameKinds(sub.get());
            }
            break;
        }
        case ExprKind::Unary:
            walkExprForNameKinds(e->left.get());
            break;
        case ExprKind::Binary:
            walkExprForNameKinds(e->left.get());
            walkExprForNameKinds(e->right.get());
            break;
    }
}

void Interpreter::validateStaticSemantics() {
    // OPTION BASE (no maximo uma no programa, secao 18.4) precisa ser
    // conhecida antes de conferir os limites do DIM abaixo; aproveita
    // o mesmo laco pra flagrar uma segunda OPTION, sem esperar chegar
    // nela em tempo de execucao (o que ja teria impresso saida antes).
    bool sawOption = false;
    int optionLine = -1;
    for (const auto& ln : lines_) {
        if (ln.statement.kind == StmtKind::Option) {
            if (sawOption) {
                fatalError(L("um programa so pode ter um OPTION BASE "
                             "(secao 18.4)",
                             "a program can only have one OPTION BASE "
                             "(section 18.4)"),
                           ln.statement.line);
            }
            optionBase_ = ln.statement.optionBase;
            optionLine = ln.statement.line;
            sawOption = true;
        }
    }
    optionBaseLine_ = optionLine;

    for (const auto& ln : lines_) {
        if (ln.statement.kind == StmtKind::Dim) {
            for (const auto& decl : ln.statement.dimDeclarations) {
                auto res = dimLineByName_.emplace(decl.name, ln.statement.line);
                if (!res.second && res.first->second != ln.statement.line) {
                    // duas linhas DIFERENTES declarando o mesmo array
                    // (revisitar a MESMA linha via GOTO/loop nao entra
                    // aqui, so duas DIM de verdade no texto do programa)
                    fatalError(L("o array ", "array ") + decl.name +
                                   L(" so pode ser dimensionado uma vez "
                                     "(secao 18.4)",
                                     " can only be dimensioned once "
                                     "(section 18.4)"),
                               ln.statement.line);
                }
                // O DIM define o tamanho do array pela sua mera
                // presenca no texto do programa, mesmo que o comando
                // nunca seja executado de verdade (um GOTO pode pular
                // por cima dele). Confirmado contra o bas55, que
                // preve exatamente esse caso (teste NBS P062: "DIM
                // SETS ARRAY SIZE EVEN IF JUMPED OVER"). Por isso o
                // array ja nasce aqui, na validacao estatica, nao
                // esperando a linha DIM executar.
                if (!arrays_.count(decl.name)) {
                    int base = optionBase_;
                    if (decl.bound1 < base ||
                        (decl.bound2 >= 0 && decl.bound2 < base)) {
                        fatalError(L("o limite do DIM de ",
                                     "the DIM bound of ") +
                                       decl.name +
                                       L(" e menor que a base dos indices "
                                         "(secao 18.4)",
                                         " is smaller than the index base "
                                         "(section 18.4)"),
                                   ln.statement.line);
                    }
                    NumericArray arr;
                    arr.bound1 = decl.bound1;
                    arr.bound2 = decl.bound2;
                    std::size_t size1 = static_cast<std::size_t>(
                        std::max(0, arr.bound1 - base + 1));
                    std::size_t total =
                        (decl.bound2 >= 0)
                            ? size1 * static_cast<std::size_t>(std::max(
                                          0, arr.bound2 - base + 1))
                            : size1;
                    arr.values.assign(total, 0.0);
                    arr.defined.assign(total, 0);
                    arrays_.emplace(decl.name, std::move(arr));
                }
            }
        }
    }

    for (const auto& ln : lines_) {
        const Stmt& s = ln.statement;
        switch (s.kind) {
            case StmtKind::Let:
                walkExprForNameKinds(s.letTarget.get());
                walkExprForNameKinds(s.letValue.get());
                break;
            case StmtKind::Print:
                for (const auto& item : s.printItems) {
                    if (item.expr) {
                        walkExprForNameKinds(item.expr.get());
                    }
                }
                break;
            case StmtKind::Input:
            case StmtKind::Read:
                for (const auto& v : s.variableList) {
                    walkExprForNameKinds(v.get());
                }
                break;
            case StmtKind::Dim: {
                int base = optionBase_;
                for (const auto& decl : s.dimDeclarations) {
                    if (optionBaseLine_ != -1 && s.line < optionBaseLine_) {
                        fatalError(L("OPTION BASE precisa vir antes de "
                                     "qualquer DIM ou uso de array "
                                     "(secao 18.4)",
                                     "OPTION BASE must come before any DIM "
                                     "or array use (section 18.4)"),
                                   s.line);
                    }
                    if (decl.bound1 < base ||
                        (decl.bound2 >= 0 && decl.bound2 < base)) {
                        fatalError(L("o limite do DIM de ",
                                     "the DIM bound of ") +
                                       decl.name +
                                       L(" e menor que a base dos indices "
                                         "(secao 18.4)",
                                         " is smaller than the index base "
                                         "(section 18.4)"),
                                   s.line);
                    }
                    checkAndSetNameKind(decl.name,
                                        decl.bound2 >= 0 ? NameKind::Array2D
                                                          : NameKind::Array1D,
                                        s.line);
                }
                break;
            }
            case StmtKind::For:
                checkAndSetNameKind(s.forVariable, NameKind::Simple, s.line);
                walkExprForNameKinds(s.forInitial.get());
                walkExprForNameKinds(s.forLimit.get());
                if (s.forStep) {
                    walkExprForNameKinds(s.forStep.get());
                }
                break;
            case StmtKind::Next:
                checkAndSetNameKind(s.nextVariable, NameKind::Simple, s.line);
                break;
            case StmtKind::IfThen:
                walkExprForNameKinds(s.ifCondition.get());
                break;
            case StmtKind::OnGoto:
                walkExprForNameKinds(s.onExpr.get());
                break;
            default:
                // REM/DATA/OPTION/DEF/GOTO/GOSUB/RETURN/RANDOMIZE/
                // RESTORE/STOP/END nao referenciam variavel simples ou
                // array diretamente (o corpo do DEF FN e um escopo a
                // parte - secao 10.4 - e e checado quando a funcao e
                // chamada de verdade, nao aqui)
                break;
        }
    }
}

// ---------------------------------------------------------------------
// Controle de execucao
// ---------------------------------------------------------------------

void Interpreter::execStatement(std::size_t pc, std::size_t& nextPc) {
    const Stmt& s = lines_[pc].statement;
    switch (s.kind) {
        case StmtKind::Let:
            execLet(s);
            break;
        case StmtKind::Print:
            execPrint(s);
            break;
        case StmtKind::Input:
            execInput(s);
            break;
        case StmtKind::Read:
            execRead(s);
            break;
        case StmtKind::Data:
            break; // ja coletado em collectDataAndFunctions
        case StmtKind::Dim:
            execDim(s);
            break;
        case StmtKind::Option:
            execOption(s);
            break;
        case StmtKind::Def:
            break; // ja coletado em collectDataAndFunctions
        case StmtKind::Goto:
            execGoto(s, nextPc);
            break;
        case StmtKind::Gosub:
            execGosub(s, pc, nextPc);
            break;
        case StmtKind::Return:
            execReturn(pc, nextPc);
            break;
        case StmtKind::IfThen:
            execIfThen(s, pc, nextPc);
            break;
        case StmtKind::OnGoto:
            execOnGoto(s, nextPc);
            break;
        case StmtKind::For:
            execFor(s, pc, nextPc);
            break;
        case StmtKind::Next:
            execNext(s, pc, nextPc);
            break;
        case StmtKind::Randomize:
            // secao 20.4: uma nova sequencia imprevisivel a cada RANDOMIZE
            rng_.seed(static_cast<unsigned int>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
            break;
        case StmtKind::Restore:
            dataPointer_ = 0; // secao 16.4: volta o ponteiro pro inicio
            break;
        case StmtKind::Remark:
            break;
        case StmtKind::Stop:
        case StmtKind::End:
            break; // tratados no laco principal de run()
    }
}

void Interpreter::execLet(const Stmt& s) {
    Value v = evalExpr(s.letValue.get());
    assignTo(s.letTarget.get(), v);
}

void Interpreter::assignTo(const Expr* target, const Value& value) {
    switch (target->kind) {
        case ExprKind::SimpleVariable:
            setSimpleNumeric(target->name, value.number, target->line);
            break;
        case ExprKind::StringVariable:
            setStringVar(target->name, value.text);
            break;
        case ExprKind::ArrayElement: {
            double& slot = arrayElementRef(target, true);
            slot = value.number;
            break;
        }
        default:
            fatalError(L("alvo de atribuicao invalido", "invalid assignment target"),
                       target->line);
    }
}

void Interpreter::execDim(const Stmt& s) {
    // O array ja foi criado, com os limites certos, na validacao
    // estatica antes de rodar qualquer linha (secao 18.4, o DIM
    // define o tamanho do array so por existir no texto do programa,
    // mesmo que esta linha nunca chegue a executar de verdade porque
    // um GOTO pula por cima dela; confirmado com o teste NBS P062).
    // Nao sobra nada pra fazer aqui em tempo de execucao.
    (void)s;
}

void Interpreter::execOption(const Stmt& s) {
    // A unicidade (secao 18.4: no maximo um OPTION BASE no programa) e
    // a ordem (antes de qualquer DIM/array) ja foram conferidas
    // estaticamente em validateStaticSemantics(), antes de rodar
    // qualquer linha. Aqui so resta aplicar o valor, reexecutar esta
    // mesma linha via GOTO/loop (a mesma linha, nao uma segunda OPTION
    // de verdade) e permitido e inofensivo.
    optionBase_ = s.optionBase;
    optionBaseSet_ = true;
}

bool Interpreter::forConditionMet(double v, double limit, double step) {
    double s = (step > 0.0) - (step < 0.0);
    return (v - limit) * s > 0.0;
}

void Interpreter::execFor(const Stmt& s, std::size_t pc, std::size_t& nextPc) {
    double initial = evalNumeric(s.forInitial.get());
    double limit = evalNumeric(s.forLimit.get());
    double step = s.forStep ? evalNumeric(s.forStep.get()) : 1.0;
    setSimpleNumeric(s.forVariable, initial, s.line);

    auto itNext = forToNext_.find(pc);
    std::size_t nextIndex = (itNext != forToNext_.end()) ? itNext->second : pc;

    // secao 13.4: o FOR e "top-tested", se a condicao ja falha na
    // primeira vez, o corpo do laco nao executa nenhuma vez.
    if (forConditionMet(initial, limit, step)) {
        nextPc = nextIndex + 1;
    } else {
        forStack_.push_back(ActiveFor{s.forVariable, limit, step, pc, nextIndex});
        nextPc = pc + 1;
    }
}

void Interpreter::execNext(const Stmt& s, std::size_t pc, std::size_t& nextPc) {
    if (forStack_.empty() || forStack_.back().variable != s.nextVariable) {
        fatalError("NEXT " + s.nextVariable +
                       L(" sem um FOR correspondente em tempo de execucao "
                         "(secao 13.4)",
                         " has no matching FOR at run time (section 13.4)"),
                   s.line);
    }
    ActiveFor& af = forStack_.back();
    double v = getSimpleNumeric(af.variable, s.line) + af.step;
    setSimpleNumeric(af.variable, v, s.line);
    if (forConditionMet(v, af.limit, af.step)) {
        forStack_.pop_back();
        nextPc = pc + 1;
    } else {
        nextPc = af.forIndex + 1;
    }
}

void Interpreter::execIfThen(const Stmt& s, std::size_t pc, std::size_t& nextPc) {
    Value cond = evalExpr(s.ifCondition.get());
    if (cond.number != 0.0) {
        nextPc = resolveLineTarget(s.ifTargetLine, s.line);
    } else {
        nextPc = pc + 1;
    }
}

void Interpreter::execGoto(const Stmt& s, std::size_t& nextPc) {
    nextPc = resolveLineTarget(s.targetLine, s.line);
}

void Interpreter::execGosub(const Stmt& s, std::size_t pc, std::size_t& nextPc) {
    gosubStack_.push_back(pc + 1);
    nextPc = resolveLineTarget(s.targetLine, s.line);
}

void Interpreter::execReturn(std::size_t pc, std::size_t& nextPc) {
    if (gosubStack_.empty()) {
        fatalError(L("RETURN sem um GOSUB correspondente (secao 12.5)",
                     "RETURN with no matching GOSUB (section 12.5)"),
                   lines_[pc].statement.line);
    }
    nextPc = gosubStack_.back();
    gosubStack_.pop_back();
}

void Interpreter::execOnGoto(const Stmt& s, std::size_t& nextPc) {
    double val = evalNumeric(s.onExpr.get());
    int n = static_cast<int>(std::lround(val));
    if (n < 1 || static_cast<std::size_t>(n) > s.onTargets.size()) {
        fatalError(L("o valor do ON...GO TO ficou fora da lista de linhas "
                     "(secao 12.5)",
                     "the ON...GO TO value fell outside the line list "
                     "(section 12.5)"),
                   s.line);
    }
    nextPc = resolveLineTarget(s.onTargets[static_cast<std::size_t>(n - 1)], s.line);
}

std::size_t Interpreter::resolveLineTarget(int lineNumber, int fromLine) {
    auto it = lineIndexByNumber_.find(lineNumber);
    if (it == lineIndexByNumber_.end()) {
        fatalError(L("a linha ", "line ") + std::to_string(lineNumber) +
                       L(" nao existe no programa",
                         " does not exist in the program"),
                   fromLine);
    }
    return it->second;
}

// ---------------------------------------------------------------------
// PRINT / INPUT / READ
// ---------------------------------------------------------------------

void Interpreter::execPrint(const Stmt& s) {
    if (s.printItems.empty()) {
        out_ << '\n'; // secao 14.6: PRINT sozinho gera uma linha em branco
        columnPosition_ = 1;
        return;
    }
    bool endsWithSeparator = s.printItems.back().separator != '\0';
    for (const auto& item : s.printItems) {
        if (item.isTab) {
            int n = static_cast<int>(std::lround(evalNumeric(item.expr.get())));
            printAdvanceToTab(n);
        } else if (item.expr) {
            Value v = evalExpr(item.expr.get());
            printText(v.isString ? v.text : formatNumber(v.number));
        }
        if (item.separator == ',') {
            printAdvanceToNextZone();
        }
        // ';' gera a string nula (secao 14.4), nao faz nada aqui
    }
    if (!endsWithSeparator) {
        out_ << '\n';
        columnPosition_ = 1;
    }
}

void Interpreter::printText(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size()) {
        if (columnPosition_ > 1 &&
            columnPosition_ - 1 + static_cast<int>(text.size() - i) > kMargin) {
            out_ << '\n';
            columnPosition_ = 1;
        }
        int remaining = kMargin - (columnPosition_ - 1);
        std::size_t chunk = std::min(static_cast<std::size_t>(remaining),
                                      text.size() - i);
        if (chunk == 0) {
            out_ << '\n';
            columnPosition_ = 1;
            continue;
        }
        out_ << text.substr(i, chunk);
        columnPosition_ += static_cast<int>(chunk);
        i += chunk;
    }
}

void Interpreter::printAdvanceToNextZone() {
    int currentZone = (columnPosition_ - 1) / kPrintZoneWidth;
    if (currentZone >= kPrintZoneCount - 1) {
        out_ << '\n';
        columnPosition_ = 1;
    } else {
        int nextZoneStart = (currentZone + 1) * kPrintZoneWidth + 1;
        out_ << std::string(static_cast<std::size_t>(nextZoneStart - columnPosition_), ' ');
        columnPosition_ = nextZoneStart;
    }
}

void Interpreter::printAdvanceToTab(int n) {
    if (n < 1) {
        n = 1; // secao 14.5: excecao nao-fatal, recuperacao recomendada = usar 1
    }
    if (n > kMargin) {
        n = n - kMargin * ((n - 1) / kMargin); // formula exata da secao 14.4
    }
    if (columnPosition_ <= n) {
        out_ << std::string(static_cast<std::size_t>(n - columnPosition_), ' ');
    } else {
        out_ << '\n' << std::string(static_cast<std::size_t>(n - 1), ' ');
    }
    columnPosition_ = n;
}

std::string Interpreter::formatNumber(double v) const {
    // Nota de escopo: esta e uma aproximacao razoavel do algoritmo
    // exato da secao 14.4, calibrada contra o bas55 (Jorge Giner
    // Cordero), 8 digitos significativos, e a escolha entre notacao
    // fixa e cientifica seguindo o orcamento que a propria
    // documentacao do bas55 descreve (8 casas decimais no maximo em
    // notacao fixa; alem disso, cientifica). Casos de borda bem
    // especificos ainda podem nao bater caractere a caractere com a
    // norma.
    constexpr int kSigDigits = 8;
    constexpr int kMaxDecimalPlaces = 8;

    bool neg = v < 0.0;
    double av = std::fabs(v);
    std::string body;

    if (std::isinf(av)) {
        body = "INF"; // C gera "inf" minusculo; a linguagem e toda maiuscula
    } else if (av == 0.0) {
        body = "0";
    } else {
        char sciBuf[64];
        std::snprintf(sciBuf, sizeof(sciBuf), "%.*e", kSigDigits - 1, av);
        std::string s(sciBuf);
        auto epos = s.find('e');
        std::string digits; // so os algarismos significativos, sem ponto
        for (char c : s.substr(0, epos)) {
            if (c != '.') {
                digits += c;
            }
        }
        int exp = std::atoi(s.c_str() + epos + 1);
        // tira zeros a direita (mantem pelo menos 1 digito)
        while (digits.size() > 1 && digits.back() == '0') {
            digits.pop_back();
        }
        int digitCount = static_cast<int>(digits.size());

        bool useFixed;
        int decimalPlaces;
        if (exp >= 0) {
            int intDigits = exp + 1;
            decimalPlaces = digitCount > intDigits ? digitCount - intDigits : 0;
            useFixed = intDigits <= kSigDigits && decimalPlaces <= kMaxDecimalPlaces;
        } else {
            int leadingZeros = -exp - 1;
            decimalPlaces = leadingZeros + digitCount;
            useFixed = decimalPlaces <= kMaxDecimalPlaces;
        }

        if (useFixed) {
            if (exp >= 0) {
                int intDigits = exp + 1;
                std::string intPart = digits.substr(
                    0, std::min(intDigits, digitCount));
                while (static_cast<int>(intPart.size()) < intDigits) {
                    intPart += '0'; // numero inteiro com menos digitos que exp+1
                }
                std::string fracPart =
                    digitCount > intDigits ? digits.substr(intDigits) : "";
                body = intPart;
                if (!fracPart.empty()) {
                    body += "." + fracPart;
                }
            } else {
                int leadingZeros = -exp - 1;
                body = "." + std::string(static_cast<std::size_t>(leadingZeros), '0') +
                       digits;
            }
        } else {
            std::string mantissa = digits.substr(0, 1) + "." + digits.substr(1);
            body = mantissa + "E" + (exp < 0 ? "-" : "+") +
                   std::to_string(std::abs(exp));
        }
    }
    std::string result;
    result += (neg ? '-' : ' ');
    result += body;
    result += ' ';
    return result;
}

void Interpreter::execInput(const Stmt& s) {
    for (;;) {
        out_ << "? "; // secao 15.6: prompt recomendado pela norma
        std::string line;
        if (!std::getline(in_, line)) {
            fatalError(L("fim da entrada durante o INPUT (secao 15)",
                         "end of input during INPUT (section 15)"),
                       s.line);
        }
        auto items = splitReplyList(line);
        if (items.size() != s.variableList.size()) {
            // secao 15.5: quantidade errada de dados nao e fatal, 
            // recuperacao recomendada e pedir a resposta de novo
            out_ << L("ENTRADA INVALIDA: numero de valores nao bate, "
                      "tente novamente\n",
                      "INVALID INPUT: wrong number of values, "
                      "try again\n");
            continue;
        }
        std::vector<Value> parsed;
        bool ok = true;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const Expr* target = s.variableList[i].get();
            bool wantString = target->kind == ExprKind::StringVariable;
            Value v;
            if (wantString) {
                if (items[i].text.size() > 18) {
                    // secao 7.4/15.5: string maior que 18 caracteres no
                    // INPUT nao e fatal, pede a resposta de novo
                    ok = false;
                    break;
                }
                v.isString = true;
                v.text = items[i].text;
            } else {
                if (items[i].wasQuoted || !looksNumeric(items[i].text)) {
                    ok = false;
                    break;
                }
                v.number = parseDataNumber(items[i].text);
            }
            parsed.push_back(v);
        }
        if (!ok) {
            // secao 15.5: tipo errado ou string longa demais tambem nao
            // e fatal - pede a resposta de novo
            out_ << L("ENTRADA INVALIDA: valor numerico esperado, ou string "
                      "com mais de 18 caracteres, tente novamente\n",
                      "INVALID INPUT: expected a number, or the string "
                      "has more than 18 characters, try again\n");
            continue;
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            assignTo(s.variableList[i].get(), parsed[i]);
        }
        break;
    }
}

bool Interpreter::nextDataItem(DataItem& out) {
    if (dataPointer_ >= dataSequence_.size()) {
        return false;
    }
    out = dataSequence_[dataPointer_++];
    return true;
}

void Interpreter::execRead(const Stmt& s) {
    for (const auto& targetPtr : s.variableList) {
        const Expr* target = targetPtr.get();
        DataItem item;
        if (!nextDataItem(item)) {
            fatalError(L("faltam dados no DATA para o READ (secao 16.5)",
                         "not enough DATA for READ (section 16.5)"),
                       s.line);
        }
        bool wantString = target->kind == ExprKind::StringVariable;
        Value v;
        if (wantString) {
            v.isString = true;
            v.text = item.text;
        } else {
            if (item.wasQuoted || !looksNumeric(item.text)) {
                fatalError(L("o DATA tem uma string onde o READ esperava um "
                             "numero (secao 16.5)",
                             "DATA has a string where READ expected a "
                             "number (section 16.5)"),
                           s.line);
            }
            v.number = parseDataNumber(item.text);
        }
        assignTo(target, v);
    }
}

// ---------------------------------------------------------------------
// Expressoes
// ---------------------------------------------------------------------

Interpreter::Value Interpreter::evalExpr(const Expr* e) {
    switch (e->kind) {
        case ExprKind::NumberLiteral: {
            Value v;
            v.number = e->numberValue;
            return v;
        }
        case ExprKind::StringLiteral: {
            Value v;
            v.isString = true;
            v.text = e->stringValue;
            return v;
        }
        case ExprKind::SimpleVariable: {
            Value v;
            v.number = getSimpleNumeric(e->name, e->line);
            return v;
        }
        case ExprKind::StringVariable: {
            Value v;
            v.isString = true;
            v.text = getStringVar(e->name, e->line);
            return v;
        }
        case ExprKind::ArrayElement: {
            Value v;
            v.number = arrayElementRef(e, false);
            return v;
        }
        case ExprKind::BuiltinCall:
            return evalBuiltinCall(e);
        case ExprKind::DefFunctionCall:
            return evalDefFunctionCall(e);
        case ExprKind::Unary:
            return evalUnary(e);
        case ExprKind::Binary:
            return evalBinary(e);
    }
    fatalError(L("expressao interna invalida", "invalid internal expression"),
               e->line);
}

double Interpreter::evalNumeric(const Expr* e) {
    Value v = evalExpr(e);
    if (v.isString) {
        fatalError(L("esperava um valor numerico, recebeu uma string",
                     "expected a numeric value, got a string"),
                   e->line);
    }
    return v.number;
}

std::string Interpreter::evalString(const Expr* e) {
    Value v = evalExpr(e);
    if (!v.isString) {
        fatalError(L("esperava uma string, recebeu um valor numerico",
                     "expected a string, got a numeric value"),
                   e->line);
    }
    return v.text;
}

Interpreter::Value Interpreter::evalUnary(const Expr* e) {
    double x = evalNumeric(e->left.get());
    Value v;
    v.number = (e->op == TokenType::Minus) ? -x : x;
    return v;
}

Interpreter::Value Interpreter::evalBinary(const Expr* e) {
    // relacoes (secao 12) - podem comparar numero ou string
    if (e->op == TokenType::Equals || e->op == TokenType::NotEquals ||
        e->op == TokenType::Less || e->op == TokenType::LessEquals ||
        e->op == TokenType::Greater || e->op == TokenType::GreaterEquals) {
        Value l = evalExpr(e->left.get());
        Value r = evalExpr(e->right.get());
        bool result;
        if (l.isString || r.isString) {
            // o parser ja garante que so = ou <> chegam aqui quando os
            // operandos sao string (secao 12.4)
            result = (e->op == TokenType::Equals) ? (l.text == r.text)
                                                    : (l.text != r.text);
        } else {
            switch (e->op) {
                case TokenType::Equals: result = l.number == r.number; break;
                case TokenType::NotEquals: result = l.number != r.number; break;
                case TokenType::Less: result = l.number < r.number; break;
                case TokenType::LessEquals: result = l.number <= r.number; break;
                case TokenType::Greater: result = l.number > r.number; break;
                default: result = l.number >= r.number; break;
            }
        }
        Value v;
        v.number = result ? 1.0 : 0.0;
        return v;
    }

    double l = evalNumeric(e->left.get());
    double r = evalNumeric(e->right.get());
    double result = 0.0;
    switch (e->op) {
        case TokenType::Plus:
            result = l + r;
            break;
        case TokenType::Minus:
            result = l - r;
            break;
        case TokenType::Star:
            result = l * r;
            break;
        case TokenType::Slash:
            if (r == 0.0) {
                // secao 8.5: divisao por zero nao e fatal - substitui
                // por infinito com o sinal do numerador e continua
                result = (l >= 0.0) ? std::numeric_limits<double>::infinity()
                                     : -std::numeric_limits<double>::infinity();
            } else {
                result = l / r;
            }
            break;
        case TokenType::Caret:
            if (l == 0.0 && r == 0.0) {
                result = 1.0; // secao 8.4: 0^0 = 1
            } else if (l < 0.0 && std::floor(r) != r) {
                fatalError(L("base negativa elevada a um expoente nao "
                             "inteiro (secao 8.5)",
                             "negative base raised to a non-integer "
                             "exponent (section 8.5)"),
                           e->line);
            } else if (l == 0.0 && r < 0.0) {
                result = std::numeric_limits<double>::infinity(); // secao 8.5
            } else {
                result = std::pow(l, r);
            }
            break;
        default:
            fatalError(L("operador interno invalido", "invalid internal operator"),
                       e->line);
    }
    Value v;
    v.number = result;
    return v;
}

Interpreter::Value Interpreter::evalBuiltinCall(const Expr* e) {
    if (e->builtin == TokenType::FnRND) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        Value v;
        v.number = dist(rng_);
        return v;
    }
    double x = evalNumeric(e->subscripts[0].get());
    double result = 0.0;
    switch (e->builtin) {
        case TokenType::FnABS: result = std::fabs(x); break;
        case TokenType::FnATN: result = std::atan(x); break;
        case TokenType::FnCOS: result = std::cos(x); break;
        case TokenType::FnEXP: result = std::exp(x); break;
        case TokenType::FnINT: result = std::floor(x); break; // secao 9.4
        case TokenType::FnLOG:
            if (x <= 0.0) {
                fatalError(L("LOG exige um argumento positivo (secao 9.5)",
                             "LOG requires a positive argument (section 9.5)"),
                           e->line);
            }
            result = std::log(x);
            break;
        case TokenType::FnSGN:
            result = static_cast<double>((x > 0.0) - (x < 0.0));
            break;
        case TokenType::FnSIN: result = std::sin(x); break;
        case TokenType::FnSQR:
            if (x < 0.0) {
                fatalError(L("SQR exige um argumento nao negativo (secao 9.5)",
                             "SQR requires a non-negative argument "
                             "(section 9.5)"),
                           e->line);
            }
            result = std::sqrt(x);
            break;
        case TokenType::FnTAN: result = std::tan(x); break;
        default:
            fatalError(L("funcao embutida invalida", "invalid builtin function"),
                       e->line);
    }
    Value v;
    v.number = result;
    return v;
}

Interpreter::Value Interpreter::evalDefFunctionCall(const Expr* e) {
    auto it = functions_.find(e->defFunctionName);
    if (it == functions_.end()) {
        fatalError(L("a funcao ", "function ") + e->defFunctionName +
                       L(" nao foi definida com DEF antes de ser usada "
                         "(secao 10.4)",
                         " was not defined with DEF before being used "
                         "(section 10.4)"),
                   e->line);
    }
    const DefFunction& fn = it->second;
    bool callHasArg = !e->subscripts.empty();
    if (fn.hasParam != callHasArg) {
        fatalError(L("o numero de argumentos de ",
                     "the number of arguments of ") +
                       e->defFunctionName +
                       L(" nao bate com a definicao (secao 8.4)",
                         " does not match the definition (section 8.4)"),
                   e->line);
    }

    // O argumento e avaliado ANTES de entrar no corpo da funcao (e
    // antes de marcar esta funcao como "em execucao" na pilha de
    // recursao abaixo). Isso importa de verdade: em algo como
    // "FNG(FNH(X))" onde FNH por sua vez chama FNG de novo, essa
    // segunda chamada a FNG e uma chamada nova e legitima (ainda
    // estamos calculando o ARGUMENTO da primeira, nao executando o
    // corpo dela), nao e recursao de FNG chamando FNG.
    double argVal = fn.hasParam ? evalNumeric(e->subscripts[0].get()) : 0.0;

    // secao 10.4: "A function definition may refer to other defined
    // functions, but not to the function being defined", ou seja,
    // FNA nao pode chamar FNA (direta ou indiretamente, via FNB que
    // chama FNA de volta) de dentro do proprio CORPO. Sem essa guarda,
    // isso vira recursao infinita em C++ e estoura a pilha (achado
    // testando contra o conjunto de testes da NBS - teste P161).
    for (const auto& active : functionCallStack_) {
        if (active == e->defFunctionName) {
            fatalError(L("a funcao ", "function ") + e->defFunctionName +
                           L(" nao pode chamar a si mesma, direta ou "
                             "indiretamente (secao 10.4)",
                             " cannot call itself, directly or indirectly "
                             "(section 10.4)"),
                       e->line);
        }
    }
    functionCallStack_.push_back(e->defFunctionName);

    Value result;
    if (!fn.hasParam) {
        result.number = evalNumeric(fn.body);
        functionCallStack_.pop_back();
        return result;
    }

    // o parametro e local a definicao (secao 10.4): salva o valor
    // anterior da variavel de mesmo nome, se houver, e restaura depois
    bool hadValue = numericVars_.count(fn.param) != 0;
    double savedValue = hadValue ? numericVars_[fn.param] : 0.0;
    bool hadDefinedFlag = hadValue && numericDefined_[fn.param];

    // Escrita direta (nao passa por setSimpleNumeric/checkAndSetNameKind)
    // de proposito: o parametro do DEF FN e local a definicao (secao
    // 10.4, "distinct from any variable with the same name outside"),
    // entao nao deve participar da checagem de conflito simples/array
    // do nome global de mesma grafia.
    numericVars_[fn.param] = argVal;
    numericDefined_[fn.param] = true;
    result.number = evalNumeric(fn.body);

    if (hadValue) {
        numericVars_[fn.param] = savedValue;
        numericDefined_[fn.param] = hadDefinedFlag;
    } else {
        numericVars_.erase(fn.param);
        numericDefined_.erase(fn.param);
    }
    functionCallStack_.pop_back();
    return result;
}

// ---------------------------------------------------------------------
// Variaveis e arrays
// ---------------------------------------------------------------------

double Interpreter::getSimpleNumeric(const std::string& name, int line) {
    auto it = numericVars_.find(name);
    if (it == numericVars_.end() || !numericDefined_[name]) {
        fatalError(L("a variavel ", "variable ") + name +
                       L(" foi usada antes de receber um valor (secao 7.6)",
                         " was used before being given a value (section 7.6)"),
                   line);
    }
    return it->second;
}

void Interpreter::setSimpleNumeric(const std::string& name, double v, int line) {
    checkAndSetNameKind(name, NameKind::Simple, line);
    numericVars_[name] = v;
    numericDefined_[name] = true;
}

void Interpreter::checkAndSetNameKind(const std::string& name, NameKind kind,
                                      int line) {
    auto it = nameKind_.find(name);
    if (it == nameKind_.end()) {
        nameKind_[name] = kind;
        return;
    }
    if (it->second != kind) {
        // secao 7.4: o mesmo nome nao pode ser variavel simples e
        // array ao mesmo tempo, nem array de 1 e de 2 dimensoes
        fatalError(L("a variavel ", "variable ") + name +
                       L(" ja foi usada de um jeito diferente antes "
                         "(simples/array de 1 dimensao/array de 2 "
                         "dimensoes nao se misturam - secao 7.4)",
                         " has already been used differently before "
                         "(simple/1-dimension array/2-dimension array "
                         "cannot mix - section 7.4)"),
                   line);
    }
}

const std::string& Interpreter::getStringVar(const std::string& name, int line) {
    auto it = stringVars_.find(name);
    if (it == stringVars_.end() || !stringDefined_[name]) {
        fatalError(L("a variavel ", "variable ") + name +
                       L(" foi usada antes de receber um valor (secao 7.6)",
                         " was used before being given a value (section 7.6)"),
                   line);
    }
    return it->second;
}

void Interpreter::setStringVar(const std::string& name, const std::string& v) {
    if (v.size() > 18) {
        // secao 7.4 / 11.5: uma string-variable aceita no maximo 18
        // caracteres; atribuir uma maior (via LET ou READ) e erro
        // fatal. Confirmado contra o bas55 2.0 (a versao 1.19 testada
        // antes tinha esse comportamento ainda nao corrigido).
        fatalError(L("a string atribuida a ", "the string assigned to ") + name +
                       L(" passa de 18 caracteres (secao 7.4 / 11.5)",
                         " is longer than 18 characters (section 7.4 / 11.5)"),
                   0);
    }
    stringVars_[name] = v;
    stringDefined_[name] = true;
}

Interpreter::NumericArray& Interpreter::getOrCreateArray(const std::string& name,
                                                         bool twoDimensional,
                                                         int line) {
    checkAndSetNameKind(
        name, twoDimensional ? NameKind::Array2D : NameKind::Array1D, line);
    auto it = arrays_.find(name);
    if (it != arrays_.end()) {
        return it->second;
    }
    // nao foi declarado com DIM: limite padrao 0..10 (ou base..10 com
    // OPTION BASE - secao 18.4)
    NumericArray arr;
    arr.bound1 = 10;
    arr.bound2 = twoDimensional ? 10 : -1;
    int base = optionBase_;
    std::size_t size1 = static_cast<std::size_t>(std::max(0, arr.bound1 - base + 1));
    std::size_t total = twoDimensional
                             ? size1 * static_cast<std::size_t>(
                                           std::max(0, arr.bound2 - base + 1))
                             : size1;
    arr.values.assign(total, 0.0);
    arr.defined.assign(total, 0);
    auto res = arrays_.emplace(name, std::move(arr));
    return res.first->second;
}

double& Interpreter::arrayElementRef(const Expr* e, bool forWrite) {
    bool twoDim = e->subscripts.size() == 2;
    NumericArray& arr = getOrCreateArray(e->name, twoDim, e->line);
    int base = optionBase_;

    double raw1 = evalNumeric(e->subscripts[0].get());
    if (std::isinf(raw1) || std::isnan(raw1)) {
        // secao 7.5: um subscrito que avalia pra infinito (ex.: por um
        // overflow anterior nao-fatal, secao 8.5) esta, por
        // definicao, fora de qualquer intervalo finito de array -
        // convertido direto pra "fora do intervalo" em vez de passar
        // por lround (comportamento indefinido para infinito/NaN).
        fatalError(L("indice fora do intervalo do array ",
                          "array index out of range: ") + e->name +
                       L(" (secao 7.5)", " (section 7.5)"),
                   e->line);
    }
    int idx1 = static_cast<int>(std::lround(raw1));
    if (idx1 < base || idx1 > arr.bound1) {
        fatalError(L("indice fora do intervalo do array ",
                          "array index out of range: ") + e->name +
                       L(" (secao 7.5)", " (section 7.5)"),
                   e->line);
    }

    std::size_t flatIndex;
    if (twoDim) {
        double raw2 = evalNumeric(e->subscripts[1].get());
        if (std::isinf(raw2) || std::isnan(raw2)) {
            fatalError(L("indice fora do intervalo do array ",
                              "array index out of range: ") + e->name +
                           L(" (secao 7.5)", " (section 7.5)"),
                       e->line);
        }
        int idx2 = static_cast<int>(std::lround(raw2));
        if (idx2 < base || idx2 > arr.bound2) {
            fatalError(L("indice fora do intervalo do array ",
                              "array index out of range: ") + e->name +
                           L(" (secao 7.5)", " (section 7.5)"),
                       e->line);
        }
        std::size_t width2 = static_cast<std::size_t>(arr.bound2 - base + 1);
        flatIndex = static_cast<std::size_t>(idx1 - base) * width2 +
                    static_cast<std::size_t>(idx2 - base);
    } else {
        flatIndex = static_cast<std::size_t>(idx1 - base);
    }

    if (!forWrite && !arr.defined[flatIndex]) {
        fatalError(L("o elemento ", "element ") + e->name +
                       L("(...) foi usado antes de receber um valor "
                         "(secao 7.6)",
                         "(...) was used before being given a value "
                         "(section 7.6)"),
                   e->line);
    }
    if (forWrite) {
        arr.defined[flatIndex] = 1;
    }
    return arr.values[flatIndex];
}

// ---------------------------------------------------------------------
// Erros
// ---------------------------------------------------------------------

void Interpreter::fatalError(const std::string& message, int line) {
    if (columnPosition_ > 1) {
        // fecha a linha de saida que ficou pela metade, pra nao
        // misturar visualmente a saida parcial do programa com a
        // mensagem de erro que vem em seguida
        out_ << '\n';
        columnPosition_ = 1;
    }
    hadFatalError_ = true;
    fatalErrorMessage_ = L("ERRO na linha ", "ERROR at line ") +
                         std::to_string(line) + ": " + message;
    throw FatalErrorSignal{};
}

} // namespace ecma55
