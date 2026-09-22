// locale.hpp
// Idioma das mensagens de diagnostico (erros lexicos, de sintaxe e de
// execucao). Compartilhado por Lexer, Parser e Interpreter, e pelo
// ambiente (repl.hpp), para que LOCALIZE PT|EN troque tudo de uma vez.
//
// Isto NAO e a linguagem BASIC em si (que a norma exige ser toda em
// maiusculas, sem nocao de idioma, e so o idioma do texto que este
// interpretador escreve para explicar um erro ao usuario, algo que a
// norma deixa inteiramente a criterio de cada implementacao.

#ifndef ECMA55_LOCALE_HPP
#define ECMA55_LOCALE_HPP

namespace ecma55 {

enum class Locale { Portuguese, English };

} // namespace ecma55

#endif // ECMA55_LOCALE_HPP
