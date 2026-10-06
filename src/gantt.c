/* ============================================================================
 * gantt.c — O ÚNICO lugar onde o gráfico de Gantt é desenhado (req. 2)
 * ----------------------------------------------------------------------------
 * Decisão de projeto (opção A, ver gantt.h): a página web NÃO desenha. Ela
 * pede o SVG ao servidor e exibe. Esta mesma função gera:
 *   - a tela ao vivo (req. 2.3), só com os últimos GANTT_JANELA_TELA ticks;
 *   - o arquivo exportado (req. 2.4), com a simulação inteira.
 * Assim cada regra de desenho abaixo existe uma vez só, e a imagem exportada
 * é idêntica à da tela -- e, gerada no C, indiscutivelmente não é print.
 *
 * A legenda também é desenhada AQUI, dentro do SVG (e não no HTML): assim
 * ela aparece igual na tela e no arquivo exportado.
 *
 * CONVENÇÕES DE DESENHO, todas ditadas pelo requisito 2.1 e 2.5:
 *
 *   EXECUTANDO .......... preenchido com a COR da tarefa; dentro da célula vai
 *                         o número da CPU (requisito geral 3: a visualização
 *                         deve indicar em qual processador a tarefa executa).
 *   PRONTA (na fila) .... AUSÊNCIA de cor — apenas o contorno da célula.
 *   SUSPENSA ............ cor PRETA com preenchimento DIFERENTE do sólido
 *                         usado na execução (aqui: pontilhado).
 *   INATIVA/CONCLUIDA ... nada desenhado.
 *
 * Eixo Y (requisito 2.5): "a ordem em que as tarefas aparecem no eixo Y deve
 * ser decrescente em relação ao ID" — o MENOR id fica encostado no eixo X
 * (embaixo) e os ids maiores empilham por cima.
 *
 * Os marcadores de evento são derivados COMPARANDO historico[t] com
 * historico[t-1]. Não guardamos uma lista separada de eventos: o histórico já
 * contém a informação, e duplicá-la só criaria duas fontes da verdade.
 * ==========================================================================*/
#include "gantt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tipos de marcador de evento, em ordem de prioridade de exibição. */
typedef enum {
    MARCA_NENHUMA = 0,
    MARCA_SORTEIO,     /* a tarefa venceu um desempate por sorteio (4.3-5)   */
    MARCA_CHEGADA,     /* nova ativação da tarefa entrou no sistema (2.2)    */
    MARCA_TERMINO,     /* a ativação terminou (2.2)                          */
    MARCA_PRAZO        /* deadline perdido: é um ERRO e deve aparecer (3.3)  */
} Marca;

/* Descobre o evento ocorrido na tarefa `i` no instante `t`. */
static Marca marca_em(const Simulacao *s, int t, int i)
{
    /* TODO: comparar historico[t].tarefas[i] com historico[t-1].tarefas[i]
     * (se t==0, não há "antes": só cabe checar chegada no próprio tick 0).
     *   - se antes estava EST_INATIVA e agora não está mais -> MARCA_CHEGADA
     *   - se antes não estava EST_INATIVA/EST_CONCLUIDA e agora está
     *     EST_INATIVA ou EST_CONCLUIDA -> MARCA_TERMINO
     *   - se tarefas[i].sorteada nesse tick -> MARCA_SORTEIO
     *   - se tarefas[i].perdeu_prazo virou 1 neste tick (e não estava antes)
     *     -> MARCA_PRAZO
     * Escolher UMA prioridade quando mais de uma se aplica (a ordem do
     * enum acima já sugere qual mostrar primeiro: prazo > término >
     * chegada > sorteio, ou o critério que fizer mais sentido pro seu
     * desenho -- o importante é escolher uma ordem e documentar). */
    (void)s;
    (void)t;
    (void)i;
    return MARCA_NENHUMA;
}

/* Ordem das linhas: índices das tarefas ordenados por id CRESCENTE.
 * linha 0 = menor id = mais próxima do eixo X (requisito 2.5). */
static void ordem_por_id(const Estado *e, int *ordem)
{
    /* TODO: preencher ordem[0..ntarefas-1] com os índices 0..ntarefas-1 e
     * ordená-los pelo campo .id (crescente) -- um insertion sort simples
     * já resolve, não precisa de qsort aqui. */
    (void)e;
    (void)ordem;
}

/* ==========================================================================
 *  DESENHO — SVG da tela ao vivo e do arquivo exportado
 * ========================================================================*/

#define CEL_W  14   /* largura de 1 tick, em pixels                          */
#define LIN_H  22   /* altura de uma linha do gráfico                        */
#define MARG_E 90   /* margem esquerda, onde vão os rótulos das linhas       */
#define MARG_T 50   /* margem superior, para o título                        */

int gantt_svg(const Simulacao *s, FILE *saida, int tick_ini, int tick_fim)
{
    /* TODO:
     * 1) validar: n_hist > 0 e 0 <= tick_ini <= tick_fim < n_hist; senão
     *    devolver 0. NÃO abrir nem fechar 'saida' -- quem chamou é dono
     *    dele (ver gantt.h). Escrever tudo com fprintf(saida, ...).
     * 2) n_ticks = tick_fim - tick_ini + 1. Largura total = MARG_E +
     *    n_ticks * CEL_W; altura = MARG_T + (ncpus + ntarefas) * LIN_H +
     *    espaço da legenda. Com ~190 tarefas (arquivo do professor) a
     *    altura passa de 4000 px -- normal, a página tem rolagem.
     * 3) cabeçalho SVG (<svg width=... height=... ...>) e um <rect> de
     *    fundo branco.
     * 4) faixa das CPUs, ACIMA das tarefas (requisito 1.2: o tempo em que
     *    cada processador fica desligado precisa aparecer no gráfico):
     *    para cada CPU c e tick t, historico[t].cpus[c].tarefa == -1 ->
     *    célula hachurada cinza; senão -> célula na cor da tarefa que
     *    ocupa a CPU.
     * 5) ordem_por_id() pra saber em que linha (Y) cada tarefa vai.
     * 6) para cada tick t em tick_ini..tick_fim e cada tarefa i: x =
     *    MARG_E + (t - tick_ini) * CEL_W -- subtrair tick_ini, senão a
     *    janela da tela ao vivo começa fora do desenho. Desenhar o <rect>
     *    conforme o estado (convenções no topo do arquivo). Se
     *    EXECUTANDO, o número da CPU dentro da célula (<text>).
     * 7) marca_em(s, t, i) e o marcador por cima da célula quando !=
     *    MARCA_NENHUMA. ATENÇÃO na borda esquerda da janela: em t ==
     *    tick_ini > 0, o tick anterior existe no histórico e marca_em
     *    deve compará-lo normalmente. Se tratar a borda como "não há
     *    antes", toda tarefa ativa ganha um ▲ falso de chegada ali.
     * 8) rótulos: id da tarefa e "CPU n" na margem esquerda; no eixo X,
     *    o número REAL do tick (t, não t - tick_ini), a cada 5.
     * 9) legenda (obrigatória): executando, pronta, suspensa, CPU
     *    desligada e os quatro marcadores. Fica dentro do SVG pra
     *    aparecer igual na tela e no arquivo exportado.
     * 10) fechar com </svg>. Devolver 1. */
    (void)s;
    (void)saida;
    (void)tick_ini;
    (void)tick_fim;
    return 0;
}