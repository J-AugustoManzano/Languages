// parser.cpp

#include "parser.hpp"

#include <cctype>

namespace ecma55 {

namespace {

bool isRelationToken(TokenType type) {
    switch (type) {
        case TokenType::Equals:
        case TokenType::NotEquals:
        case TokenType::Less:
        case TokenType::LessEquals:
        case TokenType::Greater:
        case TokenType::GreaterEquals:
            return true;
        default:
            return false;
    }
}

bool isBuiltinFunctionToken(TokenType type) {
    switch (type) {
        case TokenType::FnABS:
        case TokenType::FnATN:
        case TokenType::FnCOS:
        case TokenType::FnEXP:
        case TokenType::FnINT:
        case TokenType::FnLOG:
        case TokenType::FnRND:
        case TokenType::FnSGN:
        case TokenType::FnSIN:
        case TokenType::FnSQR:
        case TokenType::FnTAN:
            return true;
        default:
            return false;
    }
}

bool isStringIdentifier(const std::string& lexeme) {
    return !lexeme.empty() && lexeme.back() == '$';
}

// O token de cada palavra vem do lexer com a linha FISICA do arquivo
// (1, 2, 3... conforme o texto-fonte), nao o numero de linha do BASIC
// (100, 110, 120...). Todo Expr nasce com e->line = token.line (fisica);
// aqui, depois que uma linha inteira termina de ser parseada, sobrescreve
// e->line em toda a arvore para o numero de linha do BASIC de verdade
// (line.number), que e o que faz sentido aparecer numa mensagem de erro.
void fixExprLines(Expr* e, int basicLineNumber) {
    if (e == nullptr) {
        return;
    }
    e->line = basicLineNumber;
    for (auto& sub : e->subscripts) {
        fixExprLines(sub.get(), basicLineNumber);
    }
    fixExprLines(e->left.get(), basicLineNumber);
    fixExprLines(e->right.get(), basicLineNumber);
}

void fixStmtExprLines(Stmt& s, int basicLineNumber) {
    s.line = basicLineNumber;
    fixExprLines(s.letTarget.get(), basicLineNumber);
    fixExprLines(s.letValue.get(), basicLineNumber);
    for (auto& item : s.printItems) {
        fixExprLines(item.expr.get(), basicLineNumber);
    }
    for (auto& v : s.variableList) {
        fixExprLines(v.get(), basicLineNumber);
    }
    fixExprLines(s.defBody.get(), basicLineNumber);
    fixExprLines(s.onExpr.get(), basicLineNumber);
    fixExprLines(s.ifCondition.get(), basicLineNumber);
    fixExprLines(s.forInitial.get(), basicLineNumber);
    fixExprLines(s.forLimit.get(), basicLineNumber);
    fixExprLines(s.forStep.get(), basicLineNumber);
}

} // namespace

Parser::Parser(std::vector<Token> tokens, Locale locale)
    : tokens_(std::move(tokens)), locale_(locale) {}

const char* Parser::L(const char* pt, const char* en) const {
    return locale_ == Locale::Portuguese ? pt : en;
}

const Token& Parser::peek(std::size_t offset) const {
    std::size_t i = pos_ + offset;
    if (i >= tokens_.size()) {
        i = tokens_.size() - 1; // o ultimo token e sempre EndOfFile
    }
    return tokens_[i];
}

const Token& Parser::advance() {
    const Token& t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) {
        ++pos_;
    }
    return t;
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    const Token& t = peek();
    std::string found = t.lexeme.empty() ? tokenTypeName(t.type) : t.lexeme;
    reportError(message + L(" (encontrado '", " (found '") + found + "')",
                t.line, t.column);
    return t; // nao consome - quem chamou decide como seguir
}

void Parser::skipToEndOfLine() {
    while (!check(TokenType::EndOfLine) && !check(TokenType::EndOfFile)) {
        advance();
    }
}

void Parser::reportError(const std::string& message, int line, int column) {
    errors_.push_back(ParseError{message, line, column});
}

int Parser::expectLineNumberValue(const Token& t) {
    if (t.lexeme.find('.') != std::string::npos ||
        t.lexeme.find('E') != std::string::npos) {
        reportError(L("um numero de linha nao pode ter ponto decimal ou "
                      "expoente (secao 5.2)",
                      "a line number cannot have a decimal point or "
                      "exponent (section 5.2)"),
                    t.line, t.column);
    }
    if (t.lexeme.size() > 4) {
        reportError(L("um numero de linha pode ter no maximo 4 digitos "
                      "(secao 5.2)",
                      "a line number can have at most 4 digits "
                      "(section 5.2)"),
                    t.line, t.column);
    }
    int value = static_cast<int>(t.numberValue);
    if (value <= 0) {
        reportError(L("um numero de linha precisa ser positivo (secao 5.4)",
                      "a line number must be positive (section 5.4)"),
                    t.line, t.column);
    }
    return value;
}

int Parser::expectIntegerBound(const Token& t) {
    if (t.lexeme.find('.') != std::string::npos ||
        t.lexeme.find('E') != std::string::npos) {
        reportError(L("os limites do DIM precisam ser inteiros (secao 18.2)",
                      "DIM bounds must be integers (section 18.2)"),
                    t.line, t.column);
    }
    if (t.numberValue < 0) {
        reportError(L("os limites do DIM nao podem ser negativos (secao 18.2)",
                      "DIM bounds cannot be negative (section 18.2)"),
                    t.line, t.column);
    }
    return static_cast<int>(t.numberValue);
}

// ---------------------------------------------------------------------
// Programa e linhas
// ---------------------------------------------------------------------

std::vector<Line> Parser::parseProgram() {
    std::vector<Line> lines;
    while (!check(TokenType::EndOfFile)) {
        if (check(TokenType::EndOfLine)) {
            advance(); // linha em branco perdida - ignora e segue
            continue;
        }
        lines.push_back(parseLine());
    }
    validateProgramStructure(lines);
    return lines;
}

Line Parser::parseLine() {
    const Token& numTok =
        expect(TokenType::Number, L("esperado um numero de linha",
                                     "expected a line number"));
    Line line;
    line.number = expectLineNumberValue(numTok);

    // Assim como os Expr (ver fixStmtExprLines), um erro de sintaxe
    // reportado daqui pra frente usa a linha FISICA do token no
    // momento em que e gerado. Reescreve para o numero de linha do
    // BASIC de verdade (line.number) antes de devolver.
    std::size_t errorsBefore = errors_.size();

    line.statement = parseStatement(line.number);
    fixStmtExprLines(line.statement, line.number);

    if (!check(TokenType::EndOfLine) && !check(TokenType::EndOfFile)) {
        const Token& t = peek();
        reportError(L("esperado o fim da linha (a norma so permite um "
                      "comando por linha - secao 5)",
                      "expected end of line (the standard only allows "
                      "one statement per line - section 5)"),
                    t.line, t.column);
        skipToEndOfLine();
    }
    if (check(TokenType::EndOfLine)) {
        advance();
    }
    for (std::size_t i = errorsBefore; i < errors_.size(); ++i) {
        errors_[i].line = line.number;
    }
    return line;
}

void Parser::validateProgramStructure(std::vector<Line>& lines) {
    if (lines.empty()) {
        reportError(L("o programa esta vazio; precisa terminar com uma "
                      "linha END (secao 5.2)",
                      "the program is empty; it must end with an END "
                      "line (section 5.2)"),
                    0, 0);
        return;
    }

    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].number <= lines[i - 1].number) {
            reportError(L("os numeros de linha precisam estar em ordem "
                          "estritamente crescente (secao 5.4)",
                          "line numbers must be in strictly ascending "
                          "order (section 5.4)"),
                        lines[i].statement.line, 0);
        }
    }

    if (lines.back().statement.kind != StmtKind::End) {
        reportError(L("a ultima linha do programa precisa ser END (secao 5.2)",
                      "the last line of the program must be END "
                      "(section 5.2)"),
                    lines.back().statement.line, 0);
    }
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        if (lines[i].statement.kind == StmtKind::End) {
            reportError(L("END so pode aparecer como a ultima linha do "
                          "programa (secao 5.4)",
                          "END can only appear as the last line of the "
                          "program (section 5.4)"),
                        lines[i].statement.line, 0);
        }
    }

    struct ForFrame {
        std::string variable;
        int line;
    };
    std::vector<ForFrame> stack;
    for (auto& ln : lines) {
        if (ln.statement.kind == StmtKind::For) {
            for (auto& frame : stack) {
                if (frame.variable == ln.statement.forVariable) {
                    reportError(L("um FOR aninhado nao pode reusar a "
                                  "variavel de controle '",
                                  "a nested FOR cannot reuse the control "
                                  "variable '") +
                                    ln.statement.forVariable +
                                    L("' de um FOR mais externo (secao 13.4)",
                                      "' of an outer FOR (section 13.4)"),
                                ln.statement.line, 0);
                    break;
                }
            }
            stack.push_back({ln.statement.forVariable, ln.statement.line});
        } else if (ln.statement.kind == StmtKind::Next) {
            if (stack.empty()) {
                reportError("NEXT " + ln.statement.nextVariable +
                                L(" sem um FOR correspondente",
                                  " has no matching FOR"),
                            ln.statement.line, 0);
            } else if (stack.back().variable != ln.statement.nextVariable) {
                reportError(
                    "NEXT " + ln.statement.nextVariable +
                        L(" nao corresponde ao FOR mais interno (variavel '",
                          " does not match the innermost FOR (variable '") +
                        stack.back().variable +
                        L("'); blocos FOR nao podem se entrelacar (secao 13.4)",
                          "'); FOR blocks cannot overlap (section 13.4)"),
                    ln.statement.line, 0);
                stack.pop_back(); // recupera mesmo assim, para nao propagar erro
            } else {
                stack.pop_back();
            }
        }
    }
    for (auto& frame : stack) {
        reportError("FOR " + frame.variable +
                        L(" sem NEXT correspondente", " has no matching NEXT"),
                    frame.line, 0);
    }
}

// ---------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------

Stmt Parser::parseStatement(int lineNumber) {
    const Token& t = peek();
    switch (t.type) {
        case TokenType::KwLET:
            advance();
            return parseLet(lineNumber);
        case TokenType::KwPRINT:
            advance();
            return parsePrint(lineNumber);
        case TokenType::KwINPUT:
            advance();
            return parseInput(lineNumber);
        case TokenType::KwREAD:
            advance();
            return parseRead(lineNumber);
        case TokenType::KwDATA:
            advance();
            return parseData(lineNumber);
        case TokenType::KwDIM:
            advance();
            return parseDim(lineNumber);
        case TokenType::KwOPTION:
            advance();
            return parseOption(lineNumber);
        case TokenType::KwDEF:
            advance();
            return parseDef(lineNumber);
        case TokenType::KwGO: {
            advance();
            if (check(TokenType::KwTO)) {
                advance();
                return parseGoto(lineNumber);
            }
            if (check(TokenType::KwSUB)) {
                advance();
                return parseGosub(lineNumber);
            }
            reportError(L("GO precisa ser seguido de TO ou SUB (secao 12.2)",
                          "GO must be followed by TO or SUB (section 12.2)"),
                        t.line, t.column);
            Stmt s;
            s.kind = StmtKind::Goto;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwRETURN: {
            advance();
            Stmt s;
            s.kind = StmtKind::Return;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwIF:
            advance();
            return parseIfThen(lineNumber);
        case TokenType::KwON:
            advance();
            return parseOnGoto(lineNumber);
        case TokenType::KwFOR:
            advance();
            return parseFor(lineNumber);
        case TokenType::KwNEXT:
            advance();
            return parseNext(lineNumber);
        case TokenType::KwRANDOMIZE: {
            advance();
            Stmt s;
            s.kind = StmtKind::Randomize;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwSTOP: {
            advance();
            Stmt s;
            s.kind = StmtKind::Stop;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwEND: {
            advance();
            Stmt s;
            s.kind = StmtKind::End;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwRESTORE: {
            advance();
            Stmt s;
            s.kind = StmtKind::Restore;
            s.line = lineNumber;
            return s;
        }
        case TokenType::KwREM: {
            std::string text = t.lexeme;
            advance();
            return parseRemark(lineNumber, text);
        }
        default: {
            reportError(L("comando invalido ou nao reconhecido (secao 5.2 "
                          "lista os comandos validos)",
                          "invalid or unrecognized statement (section 5.2 "
                          "lists the valid statements)"),
                        t.line, t.column);
            skipToEndOfLine();
            Stmt s;
            s.kind = StmtKind::Remark;
            s.line = lineNumber;
            s.remarkText = L("(linha invalida)", "(invalid line)");
            return s;
        }
    }
}

Stmt Parser::parseLet(int lineNumber) {
    ExprPtr target = parseVariableRef();
    expect(TokenType::Equals, L("esperado '=' no LET (secao 11.2)",
                                 "expected '=' in LET (section 11.2)"));
    ExprPtr value = parseExpression();

    bool targetIsString = target->kind == ExprKind::StringVariable;
    bool valueIsString = value->kind == ExprKind::StringLiteral ||
                          value->kind == ExprKind::StringVariable;
    if (targetIsString != valueIsString) {
        reportError(L("o LET nao pode misturar string e numero entre o "
                      "lado esquerdo e o direito (secao 11.2)",
                      "LET cannot mix string and number between the "
                      "left and right sides (section 11.2)"),
                    target->line, target->column);
    }

    Stmt s;
    s.kind = StmtKind::Let;
    s.line = lineNumber;
    s.letTarget = std::move(target);
    s.letValue = std::move(value);
    return s;
}

Stmt Parser::parsePrint(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Print;
    s.line = lineNumber;

    if (check(TokenType::EndOfLine) || check(TokenType::EndOfFile)) {
        return s; // PRINT sozinho: so gera uma linha em branco (secao 14.6)
    }

    for (;;) {
        PrintItem item;
        if (check(TokenType::Comma) || check(TokenType::Semicolon) ||
            check(TokenType::EndOfLine) || check(TokenType::EndOfFile)) {
            // item vazio (dois separadores seguidos, ou fim de lista)
        } else if (check(TokenType::KwTAB)) {
            advance();
            expect(TokenType::LParen, L("esperado '(' apos TAB (secao 14.2)",
                                         "expected '(' after TAB (section 14.2)"));
            item.isTab = true;
            item.expr = parseNumericExpression();
            expect(TokenType::RParen, L("esperado ')' fechando TAB(...)",
                                         "expected ')' closing TAB(...)"));
        } else {
            item.expr = parseExpression();
        }

        if (check(TokenType::Comma) || check(TokenType::Semicolon)) {
            item.separator = advance().lexeme[0];
            s.printItems.push_back(std::move(item));
            if (check(TokenType::EndOfLine) || check(TokenType::EndOfFile)) {
                // separador final (ex.: "PRINT P,"): nao cria um item
                // fantasma depois dele - o print-item final da
                // producao (secao 14.2) e opcional, e aqui nao ha nada
                // mais pra parsear
                break;
            }
            continue;
        }
        s.printItems.push_back(std::move(item));
        break;
    }
    return s;
}

Stmt Parser::parseInput(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Input;
    s.line = lineNumber;
    s.variableList.push_back(parseVariableRef());
    while (check(TokenType::Comma)) {
        advance();
        s.variableList.push_back(parseVariableRef());
    }
    return s;
}

Stmt Parser::parseRead(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Read;
    s.line = lineNumber;
    s.variableList.push_back(parseVariableRef());
    while (check(TokenType::Comma)) {
        advance();
        s.variableList.push_back(parseVariableRef());
    }
    return s;
}

std::vector<DataItem> Parser::splitDataList(const std::string& raw, int line,
                                             int column) {
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
            } else {
                reportError(L("string nao fechada num datum do DATA (faltou "
                              "aspas de fechamento)",
                              "unterminated string in a DATA datum "
                              "(missing closing quote)"),
                            line, column);
            }
            item.text = content;
            item.wasQuoted = true;
            while (i < n && raw[i] == ' ') {
                ++i;
            }
        } else {
            std::string content;
            while (i < n && raw[i] != ',') {
                char c = raw[i];
                bool valid = c == ' ' || c == '+' || c == '-' || c == '.' ||
                             (c >= '0' && c <= '9') ||
                             (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
                if (!valid) {
                    // secao 4/17.1: um unquoted-string so admite letra,
                    // digito, espaco, + - . Qualquer outro caractere
                    // (ex.: "D?F") e invalido
                    reportError(std::string(L("caractere invalido '",
                                               "invalid character '")) +
                                    c +
                                    L("' num datum sem aspas do DATA "
                                      "(secao 17.1)",
                                      "' in an unquoted DATA datum "
                                      "(section 17.1)"),
                                line, column);
                }
                content += c;
                ++i;
            }
            std::size_t end = content.size();
            while (end > 0 && content[end - 1] == ' ') {
                --end;
            }
            item.text = content.substr(0, end);
            item.wasQuoted = false;
            if (item.text.empty()) {
                // secao 17.1: um datum vazio (ex.: "DATA 1,,3") e
                // invalido, esteja ele sozinho ou entre outros
                reportError(L("datum vazio no DATA (secao 17.1)",
                              "empty datum in DATA (section 17.1)"),
                            line, column);
            }
        }
        items.push_back(std::move(item));
        if (i < n && raw[i] == ',') {
            ++i;
            continue;
        }
        if (i < n) {
            // sobrou algo que nao e nem virgula nem fim da lista (ex.:
            // "*"?" ou "*""?" - aspas fechando e logo em seguida algo
            // que nao e separador). A norma nao tem mecanismo de
            // escape de aspas dentro de string, entao isso e invalido
            reportError(std::string(L("caractere '", "character '")) +
                            raw[i] +
                            L("' inesperado depois de um datum do DATA "
                              "(faltou virgula? - secao 17.1)",
                              "' unexpected after a DATA datum (missing "
                              "comma? - section 17.1)"),
                        line, column);
        }
        break;
    }
    return items;
}

Stmt Parser::parseData(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Data;
    s.line = lineNumber;
    const Token& listTok = expect(
        TokenType::DataList,
        L("esperada a lista de dados apos DATA", "expected the data list after DATA"));
    s.dataItems = splitDataList(listTok.lexeme, listTok.line, listTok.column);
    return s;
}

Stmt Parser::parseDim(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Dim;
    s.line = lineNumber;
    for (;;) {
        const Token& nameTok =
            expect(TokenType::Identifier, L("esperado o nome de um array",
                                             "expected an array name"));
        ArrayDeclaration decl;
        decl.name = nameTok.lexeme;
        expect(TokenType::LParen, L("esperado '(' apos o nome do array (secao 18.2)",
                                     "expected '(' after the array name (section 18.2)"));
        const Token& b1 = expect(TokenType::Number,
                                  L("esperado um limite inteiro",
                                    "expected an integer bound"));
        decl.bound1 = expectIntegerBound(b1);
        if (check(TokenType::Comma)) {
            advance();
            const Token& b2 = expect(TokenType::Number,
                                      L("esperado o segundo limite",
                                        "expected the second bound"));
            decl.bound2 = expectIntegerBound(b2);
        }
        expect(TokenType::RParen, L("esperado ')' fechando os limites do array",
                                     "expected ')' closing the array bounds"));
        s.dimDeclarations.push_back(std::move(decl));
        if (check(TokenType::Comma)) {
            advance();
            continue;
        }
        break;
    }
    return s;
}

Stmt Parser::parseOption(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Option;
    s.line = lineNumber;
    expect(TokenType::KwBASE, L("esperado BASE apos OPTION (secao 18.2)",
                                 "expected BASE after OPTION (section 18.2)"));
    const Token& t = expect(TokenType::Number,
                             L("esperado 0 ou 1 apos OPTION BASE",
                               "expected 0 or 1 after OPTION BASE"));
    if (t.numberValue != 0.0 && t.numberValue != 1.0) {
        reportError(L("OPTION BASE so aceita 0 ou 1 (secao 18.2)",
                      "OPTION BASE only accepts 0 or 1 (section 18.2)"),
                    t.line, t.column);
    }
    s.optionBase = static_cast<int>(t.numberValue);
    return s;
}

Stmt Parser::parseDef(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Def;
    s.line = lineNumber;
    const Token& fn = expect(TokenType::DefFunctionName,
                              L("esperado FNx apos DEF (secao 10.2)",
                                "expected FNx after DEF (section 10.2)"));
    s.defName = fn.lexeme;
    if (check(TokenType::LParen)) {
        advance();
        const Token& param =
            expect(TokenType::Identifier,
                   L("esperado o parametro (uma variavel simples)",
                     "expected the parameter (a simple variable)"));
        if (isStringIdentifier(param.lexeme)) {
            reportError(L("o parametro do DEF FN precisa ser uma variavel "
                          "numerica simples (secao 10.2)",
                          "the DEF FN parameter must be a simple numeric "
                          "variable (section 10.2)"),
                        param.line, param.column);
        }
        s.defParam = param.lexeme;
        expect(TokenType::RParen, L("esperado ')' apos o parametro",
                                     "expected ')' after the parameter"));
    }
    expect(TokenType::Equals, L("esperado '=' no DEF (secao 10.2)",
                                 "expected '=' in DEF (section 10.2)"));
    s.defBody = parseNumericExpression();
    return s;
}

Stmt Parser::parseGoto(int lineNumber) {
    const Token& t = expect(TokenType::Number,
                             L("esperado um numero de linha apos GO TO",
                               "expected a line number after GO TO"));
    Stmt s;
    s.kind = StmtKind::Goto;
    s.line = lineNumber;
    s.targetLine = expectLineNumberValue(t);
    return s;
}

Stmt Parser::parseGosub(int lineNumber) {
    const Token& t = expect(TokenType::Number,
                             L("esperado um numero de linha apos GO SUB",
                               "expected a line number after GO SUB"));
    Stmt s;
    s.kind = StmtKind::Gosub;
    s.line = lineNumber;
    s.targetLine = expectLineNumberValue(t);
    return s;
}

Stmt Parser::parseIfThen(int lineNumber) {
    ExprPtr condition = parseRelationalExpression();
    expect(TokenType::KwTHEN, L("esperado THEN (secao 12.2)",
                                 "expected THEN (section 12.2)"));
    const Token& lineNumTok =
        expect(TokenType::Number, L("esperado um numero de linha apos THEN",
                                     "expected a line number after THEN"));

    Stmt s;
    s.kind = StmtKind::IfThen;
    s.line = lineNumber;
    s.ifCondition = std::move(condition);
    s.ifTargetLine = expectLineNumberValue(lineNumTok);
    return s;
}

Stmt Parser::parseOnGoto(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::OnGoto;
    s.line = lineNumber;
    s.onExpr = parseNumericExpression();
    expect(TokenType::KwGO, L("esperado GO TO apos a expressao do ON (secao 12.2)",
                               "expected GO TO after the ON expression (section 12.2)"));
    expect(TokenType::KwTO, L("esperado TO apos GO no ON...GO TO (secao 12.2)",
                               "expected TO after GO in ON...GO TO (section 12.2)"));
    const Token& first = expect(TokenType::Number,
                                 L("esperado um numero de linha",
                                   "expected a line number"));
    s.onTargets.push_back(expectLineNumberValue(first));
    while (check(TokenType::Comma)) {
        advance();
        const Token& n = expect(TokenType::Number,
                                 L("esperado outro numero de linha",
                                   "expected another line number"));
        s.onTargets.push_back(expectLineNumberValue(n));
    }
    return s;
}

Stmt Parser::parseFor(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::For;
    s.line = lineNumber;
    const Token& var = expect(TokenType::Identifier,
                               L("esperada a variavel de controle do FOR",
                                 "expected the FOR control variable"));
    if (isStringIdentifier(var.lexeme)) {
        reportError(L("a variavel de controle do FOR precisa ser numerica "
                      "simples (secao 13.2)",
                      "the FOR control variable must be a simple numeric "
                      "variable (section 13.2)"),
                    var.line, var.column);
    }
    s.forVariable = var.lexeme;
    expect(TokenType::Equals, L("esperado '=' no FOR (secao 13.2)",
                                 "expected '=' in FOR (section 13.2)"));
    s.forInitial = parseNumericExpression();
    expect(TokenType::KwTO, L("esperado TO no FOR (secao 13.2)",
                               "expected TO in FOR (section 13.2)"));
    s.forLimit = parseNumericExpression();
    if (check(TokenType::KwSTEP)) {
        advance();
        s.forStep = parseNumericExpression();
    }
    return s;
}

Stmt Parser::parseNext(int lineNumber) {
    Stmt s;
    s.kind = StmtKind::Next;
    s.line = lineNumber;
    const Token& var = expect(TokenType::Identifier,
                               L("esperada a variavel de controle do NEXT",
                                 "expected the NEXT control variable"));
    s.nextVariable = var.lexeme;
    return s;
}

Stmt Parser::parseRemark(int lineNumber, const std::string& text) {
    Stmt s;
    s.kind = StmtKind::Remark;
    s.line = lineNumber;
    s.remarkText = text;
    return s;
}

// ---------------------------------------------------------------------
// Expressoes
// ---------------------------------------------------------------------

ExprPtr Parser::parseExpression() {
    const Token& t = peek();
    if (t.type == TokenType::StringLiteral) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::StringLiteral;
        e->stringValue = t.lexeme;
        e->line = t.line;
        e->column = t.column;
        return e;
    }
    if (t.type == TokenType::Identifier && isStringIdentifier(t.lexeme)) {
        // string-expression so aceita string-variable ou string-constant
        // (secao 8.2): nao existe concatenacao nem outro operador de
        // string na norma.
        return parseVariableRef();
    }
    return parseNumericExpression();
}

ExprPtr Parser::parseRelationalExpression() {
    const Token& t = peek();
    bool isStringSide =
        t.type == TokenType::StringLiteral ||
        (t.type == TokenType::Identifier && isStringIdentifier(t.lexeme));

    ExprPtr left = isStringSide ? parseExpression() : parseNumericExpression();
    int line = left->line;
    int column = left->column;

    const Token& relTok = peek();
    TokenType relOp = TokenType::Equals;
    if (!isRelationToken(relTok.type)) {
        reportError(L("esperado um operador relacional (=, <>, <, <=, >, "
                      ">=) no IF (secao 12.2)",
                      "expected a relational operator (=, <>, <, <=, >, "
                      ">=) in IF (section 12.2)"),
                    relTok.line, relTok.column);
    } else {
        relOp = advance().type;
        if (isStringSide && relOp != TokenType::Equals &&
            relOp != TokenType::NotEquals) {
            reportError(L("strings so podem ser comparadas com = ou <> "
                          "(secao 12.4)",
                          "strings can only be compared with = or <> "
                          "(section 12.4)"),
                        relTok.line, relTok.column);
        }
    }

    ExprPtr right = isStringSide ? parseExpression() : parseNumericExpression();
    if (isStringSide && right->kind != ExprKind::StringLiteral &&
        right->kind != ExprKind::StringVariable) {
        // secao 12.2: relational-expression so mistura string com
        // string, ou numero com numero. Nunca os dois lados
        // diferentes (aqui o lado esquerdo era string)
        reportError(L("os dois lados de uma comparacao de string precisam "
                      "ser string (secao 12.2/12.4)",
                      "both sides of a string comparison must be strings "
                      "(section 12.2/12.4)"),
                    right->line, right->column);
    }

    auto bin = std::make_unique<Expr>();
    bin->kind = ExprKind::Binary;
    bin->op = relOp;
    bin->left = std::move(left);
    bin->right = std::move(right);
    bin->line = line;
    bin->column = column;
    return bin;
}

ExprPtr Parser::parseNumericExpression() {
    int line = peek().line;
    int column = peek().column;
    ExprPtr left;
    if (check(TokenType::Plus) || check(TokenType::Minus)) {
        TokenType op = advance().type;
        ExprPtr operand = parseTerm();
        auto unary = std::make_unique<Expr>();
        unary->kind = ExprKind::Unary;
        unary->op = op;
        unary->left = std::move(operand);
        unary->line = line;
        unary->column = column;
        left = std::move(unary);
    } else {
        left = parseTerm();
    }

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        TokenType op = advance().type;
        ExprPtr right = parseTerm();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->op = op;
        bin->left = std::move(left);
        bin->right = std::move(right);
        bin->line = line;
        bin->column = column;
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseTerm() {
    ExprPtr left = parseFactor();
    int line = left->line;
    int column = left->column;
    while (check(TokenType::Star) || check(TokenType::Slash)) {
        TokenType op = advance().type;
        ExprPtr right = parseFactor();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->op = op;
        bin->left = std::move(left);
        bin->right = std::move(right);
        bin->line = line;
        bin->column = column;
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseFactor() {
    // Involucao (^) e associativa a esquerda nesta norma (secao 8.4:
    // "A^B^C as (A^B)^C"). Diferente da maioria das linguagens
    // modernas, onde ^/** costuma ser associativo a direita. Por isso o
    // laco abaixo e igual ao de parseTerm, e nao uma chamada recursiva.
    ExprPtr left = parsePrimary();
    int line = left->line;
    int column = left->column;
    while (check(TokenType::Caret)) {
        advance();
        ExprPtr right = parsePrimary();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->op = TokenType::Caret;
        bin->left = std::move(left);
        bin->right = std::move(right);
        bin->line = line;
        bin->column = column;
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parsePrimary() {
    const Token& t = peek();

    if (t.type == TokenType::Number) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::NumberLiteral;
        e->numberValue = t.numberValue;
        e->line = t.line;
        e->column = t.column;
        return e;
    }

    if (t.type == TokenType::LParen) {
        advance();
        ExprPtr inner = parseNumericExpression();
        expect(TokenType::RParen, L("esperado ')' fechando a expressao",
                                     "expected ')' closing the expression"));
        return inner;
    }

    if (t.type == TokenType::Identifier) {
        ExprPtr v = parseVariableRef();
        if (v->kind == ExprKind::StringVariable) {
            reportError(L("variavel de string nao pode aparecer numa "
                          "expressao numerica (secao 8.2)",
                          "a string variable cannot appear in a numeric "
                          "expression (section 8.2)"),
                        v->line, v->column);
        }
        return v;
    }

    if (t.type == TokenType::DefFunctionName) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::DefFunctionCall;
        e->defFunctionName = t.lexeme;
        e->line = t.line;
        e->column = t.column;
        if (check(TokenType::LParen)) {
            advance();
            e->subscripts.push_back(parseNumericExpression());
            expect(TokenType::RParen,
                   L("esperado ')' apos o argumento de ",
                     "expected ')' after the argument of ") +
                       t.lexeme);
        }
        return e;
    }

    if (isBuiltinFunctionToken(t.type)) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::BuiltinCall;
        e->builtin = t.type;
        e->line = t.line;
        e->column = t.column;
        bool hasArgs = false;
        if (check(TokenType::LParen)) {
            advance();
            e->subscripts.push_back(parseNumericExpression());
            expect(TokenType::RParen,
                   L("esperado ')' apos o argumento de ",
                     "expected ')' after the argument of ") +
                       t.lexeme);
            hasArgs = true;
        }
        if (t.type == TokenType::FnRND) {
            if (hasArgs) {
                reportError(L("RND nao recebe argumento (secao 9.2)",
                              "RND does not take an argument (section 9.2)"),
                            t.line, t.column);
            }
        } else if (!hasArgs) {
            reportError(L("a funcao ", "function ") + t.lexeme +
                            L(" precisa de um argumento entre parenteses "
                              "(secao 9.4)",
                              " needs an argument in parentheses "
                              "(section 9.4)"),
                        t.line, t.column);
        }
        return e;
    }

    reportError(L("expressao invalida", "invalid expression"), t.line, t.column);
    if (t.type != TokenType::EndOfLine && t.type != TokenType::EndOfFile) {
        advance(); // evita loop infinito; nao avanca em fim de linha/arquivo
    }
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::NumberLiteral;
    e->numberValue = 0.0;
    e->line = t.line;
    e->column = t.column;
    return e;
}

ExprPtr Parser::parseVariableRef() {
    const Token& t = expect(TokenType::Identifier,
                             L("esperada uma variavel", "expected a variable"));
    auto e = std::make_unique<Expr>();
    e->line = t.line;
    e->column = t.column;
    e->name = t.lexeme;
    bool isString = isStringIdentifier(t.lexeme);

    if (check(TokenType::LParen)) {
        if (isString) {
            reportError(L("uma variavel de string nao pode ser indexada (a "
                          "norma so define arrays numericos - secao 18)",
                          "a string variable cannot be indexed (the "
                          "standard only defines numeric arrays - "
                          "section 18)"),
                        t.line, t.column);
        }
        if (t.lexeme.size() != 1) {
            // secao 7.2: numeric-array-name = letter (so uma letra,
            // sem digito). Diferente de simple-numeric-variable, que
            // aceita letra+digito. "A9(...)" nao e um array valido.
            reportError(L("nome de array so pode ter uma letra, sem digito "
                          "(secao 7.2)",
                          "an array name can only have one letter, no "
                          "digit (section 7.2)"),
                        t.line, t.column);
        }
        advance();
        e->kind = ExprKind::ArrayElement;
        e->subscripts.push_back(parseNumericExpression());
        if (check(TokenType::Comma)) {
            advance();
            e->subscripts.push_back(parseNumericExpression());
        }
        expect(TokenType::RParen, L("esperado ')' fechando os indices (secao 7.2)",
                                     "expected ')' closing the indices (section 7.2)"));
    } else {
        e->kind = isString ? ExprKind::StringVariable : ExprKind::SimpleVariable;
    }
    return e;
}

} // namespace ecma55
