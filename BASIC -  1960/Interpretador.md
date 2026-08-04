# AM-BASIC/42

Um interpretador de linguagem BASIC que leva a sério tanto a história da linguagem quanto os limites que ela nunca precisou testar.

## O que é

AM-BASIC/42 é um interpretador dec linguagem BASIC escrito em Rust, construído em três camadas sobrepostas, cada uma com uma origem e um propósito distintos:

- **Núcleo ANSI/ECMA-55**: o "Minimal BASIC", padronizado em 1978. A base formal da linguagem: "PRINT", "LET", "IF/THEN", "FOR/NEXT", "GOTO", "GOSUB", "DIM", "DATA/READ".
- **Compatibilidade GW-BASIC**: as extensões que a Microsoft consolidou nos anos 1980 e que se tornaram, na prática, um segundo padrão de fato: manipulação rica de string, arquivos, som, formatação de tela, "WHILE/WEND".
- **Extensões próprias**: recursos que este interpretador estende além de qualquer BASIC anterior: precisão numérica sem limite artificial, arrays com até oito dimensões, limite de índice customizável.

O resultado não é uma recriação nostálgica nem uma linguagem nova fingindo ser BASIC. É a mesma sintaxe que uma geração inteira aprendeu a programar, com algumas garantias que essa geração nunca teve à disposição.

## O que ele faz de diferente

A característica mais distintiva do projeto é a forma como trata números inteiros. A maioria dos interpretadores de BASIC, como a maioria das linguagens de programação, na verdade guarda todo número no mesmo formato de ponto flutuante, que começa a perder precisão exata acima de aproximadamente 9 quatrilhões. Para a esmagadora maioria dos programas, isso nunca importa. Mas quando importa, o efeito é silencioso: o programa continua rodando, só que com o dígito errado a partir de um certo ponto, sem aviso nenhum.

O AM-BASIC/42 detecta, em tempo de execução, quando um cálculo envolve exclusivamente números inteiros, a partir de um "FOR" que conta até um milhão, uma multiplicação em cadeia tipo fatorial e promove automaticamente esse cálculo para um formato que preserva cada dígito, crescendo tanto quanto a memória permitir. O exemplo clássico é o problema do tabuleiro de xadrez, dobrando grãos de trigo sessenta e quatro vezes: o resultado tem vinte dígitos, todos exatos, obtidos com um "FOR" comum multiplicando por dois. Assim que qualquer parte do cálculo envolve uma fração genuína, o resultado volta a ser ponto flutuante.

Essa precisão exata se estende a todo lugar onde um inteiro circula pelo programa: variáveis, arrays de uma e duas dimensões, parâmetros e retorno de funções definidas pelo usuário, constantes, comparação com ponto flutuante, e a travessia entre programas encadeados via "CHAIN".

## Pontos fortes

**Fidelidade histórica verificável.** Onde o projeto reivindica aderência ao ANSI/ECMA-55, essa aderência foi checada contra o texto normativo. Convenções que parecem arbitrárias à primeira vista, como o índice de um array declarado com "DIM A(5)" ir de 0 a 5, seis posições, não cinco, vêm de decisões de design de 1978, documentadas como tal.

**Álgebra de matriz de verdade.** O conjunto de comandos "MAT" para leitura, escrita, soma, transposta, inversa, matrizes especiais está presente, algo que boa parte dos BASICs de linha de comando nunca ofereceu.

**Estrutura de dados além do clássico.** Arrays de até oito dimensões, e a possibilidade de declarar um limite de índice customizado "DIM ANO(2020 TO 2025)" em vez de sempre começar em 0 ou 1.

**Robustez sob pressão.** A trajetória de desenvolvimento levou por caminhos que a maioria dos interpretadores de hobby nunca percorre: recursão profunda em "DEF FN", "Ctrl+C" durante entrada bloqueante, alocação de array grande o suficiente para derrubar o processo se não for contida, foi amplamente testada.

**Ambiente interativo completo.** Histórico de comando, edição de linha, destaque de sintaxe, um sistema de ajuda embutido ("HELP") com mais de 170 entradas documentadas, bilíngue em português e inglês.

## Pontos fracos e limites conhecidos

**Não é Full BASIC.** Existe uma segunda linhagem formal de BASIC: o "Full BASIC" do ANSI X3.113/ECMA-116, de 1986 que torna o número de linha opcional e introduz "SUB"/"FUNCTION" com passagem de parâmetro real, em vez do "DEF FN" de expressão única deste projeto. O AM-BASIC/42 é, deliberadamente, um BASIC de linha numerada ao estilo da década de 1960.

**A precisão exata tem fronteira, e a fronteira é proposital.** A álgebra de "MAT" (multiplicação e inversão de matriz), a divisão, os operadores lógicos bit a bit, e as funções nativas irracionais ("SQR", "SIN", "LOG", etc.) continuam sempre em ponto flutuante porque a matemática dessas operações quase nunca produziria um resultado inteiro exato de qualquer forma. Array com três ou mais dimensões também fica de fora, por não ter demanda real até agora. Nenhum desses pontos é lacuna esquecida; todos são fruto de decisão ponderada e tomada.

**Sem interface gráfica.** O ambiente é inteiramente de modo texto, sem janelas, sem mouse, sem gráfico vetorial além do que o próprio terminal oferece, seguindo o estilo tradicional que um BASIC padrão anos 1960 deve ter.

**Desempenho não comparado formalmente.** O interpretador é rápido o suficiente para uso educacional e experimentação (concatenação de string em laço, por exemplo, é O(N), não O(N²)), mas não existe benchmark publicado contra outros interpretadores de BASIC ou contra implementações de referência. Isto fica aberto a quem quiser fazer essa contribuição.

## Para quem é

Para quem quer aprender ou ensinar os fundamentos da programação através de uma linguagem que foi desenhada, desde o primeiro dia, para isso. Para quem tem curiosidade histórica genuína sobre como BASIC realmente funcionava, sem os atalhos que a memória costuma tomar. E para quem, ocasionalmente, precisa que um "FOR" multiplicando cresça até vinte dígitos sem perder um único deles.

## Documentação

- HELP <comando>, direto no ambiente interativo, para consulta rápida sem sair da sessão.
- Livro (manual), em desenvolvimento.

## Autor

Augusto Manzano
