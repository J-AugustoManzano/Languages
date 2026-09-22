// token.cpp

#include "token.hpp"

namespace ecma55 {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Number:          return "Number";
        case TokenType::StringLiteral:   return "StringLiteral";
        case TokenType::Identifier:      return "Identifier";
        case TokenType::DefFunctionName: return "DefFunctionName";

        case TokenType::KwBASE:       return "BASE";
        case TokenType::KwDATA:       return "DATA";
        case TokenType::KwDEF:        return "DEF";
        case TokenType::KwDIM:        return "DIM";
        case TokenType::KwEND:        return "END";
        case TokenType::KwFOR:        return "FOR";
        case TokenType::KwGO:         return "GO";
        case TokenType::KwIF:         return "IF";
        case TokenType::KwINPUT:      return "INPUT";
        case TokenType::KwLET:        return "LET";
        case TokenType::KwNEXT:       return "NEXT";
        case TokenType::KwON:         return "ON";
        case TokenType::KwOPTION:     return "OPTION";
        case TokenType::KwPRINT:      return "PRINT";
        case TokenType::KwRANDOMIZE:  return "RANDOMIZE";
        case TokenType::KwREAD:       return "READ";
        case TokenType::KwREM:        return "REM";
        case TokenType::KwRESTORE:    return "RESTORE";
        case TokenType::KwRETURN:     return "RETURN";
        case TokenType::KwSTEP:       return "STEP";
        case TokenType::KwSTOP:       return "STOP";
        case TokenType::KwSUB:        return "SUB";
        case TokenType::KwTHEN:       return "THEN";
        case TokenType::KwTO:         return "TO";

        case TokenType::FnABS: return "ABS";
        case TokenType::FnATN: return "ATN";
        case TokenType::FnCOS: return "COS";
        case TokenType::FnEXP: return "EXP";
        case TokenType::FnINT: return "INT";
        case TokenType::FnLOG: return "LOG";
        case TokenType::FnRND: return "RND";
        case TokenType::FnSGN: return "SGN";
        case TokenType::FnSIN: return "SIN";
        case TokenType::FnSQR: return "SQR";
        case TokenType::FnTAN: return "TAN";

        case TokenType::KwTAB: return "TAB";

        case TokenType::DataList: return "DataList";

        case TokenType::Plus:          return "+";
        case TokenType::Minus:         return "-";
        case TokenType::Star:          return "*";
        case TokenType::Slash:         return "/";
        case TokenType::Caret:         return "^";
        case TokenType::Equals:        return "=";
        case TokenType::NotEquals:     return "<>";
        case TokenType::Less:          return "<";
        case TokenType::LessEquals:    return "<=";
        case TokenType::Greater:       return ">";
        case TokenType::GreaterEquals: return ">=";

        case TokenType::LParen:    return "(";
        case TokenType::RParen:    return ")";
        case TokenType::Comma:     return ",";
        case TokenType::Semicolon: return ";";

        case TokenType::EndOfLine: return "EndOfLine";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Invalid:   return "Invalid";
    }
    return "?";
}

} // namespace ecma55
