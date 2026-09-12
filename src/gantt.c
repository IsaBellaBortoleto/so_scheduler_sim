/* ============================================================================
 * gantt.c — Exportação do gráfico de Gantt final (requisito 2.4)
 * ----------------------------------------------------------------------------
 * A visualização "ao vivo" (requisitos 2.1-2.3) virou a página web (ver
 * estado_json.h/servidor.h) -- por isso as funções de desenho no terminal
 * (gantt_preparar_terminal/gantt_terminal) saíram do gantt.h e não têm mais
 * lugar aqui. O que sobra é só a exportação do SVG final, que continua
 * sendo tarefa do C (não pode ser print de tela da página).
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
 *  EXPORTAÇÃO — ARQUIVO SVG (requisito 2.4)
 * ========================================================================*/

#define CEL_W  14   /* largura de 1 tick, em pixels                          */
#define LIN_H  22   /* altura de uma linha do gráfico                        */
#define MARG_E 90   /* margem esquerda, onde vão os rótulos das linhas       */
#define MARG_T 50   /* margem superior, para o título                        */

int gantt_svg(const Simulacao *s, const char *caminho)
{
    /* TODO:
     * 1) fopen(caminho, "w"); se falhar, devolver 0.
     * 2) calcular a largura total (MARG_E + n_hist * CEL_W) e a altura
     *    (MARG_T + ntarefas * LIN_H) do desenho.
     * 3) escrever o cabeçalho SVG (<svg width=... height=... ...>) e um
     *    <rect> de fundo branco.
     * 4) ordem_por_id() pra saber em que linha (Y) cada tarefa vai.
     * 5) para cada tick t em 0..n_hist-1 e cada tarefa i: olhar
     *    historico[t].tarefas[i].estado e desenhar um <rect> na
     *    posição (MARG_E + t*CEL_W, linha_da_tarefa*LIN_H) do jeito
     *    certo pra cada estado (ver as convenções no comentário do topo
     *    do arquivo -- cor da tarefa se EXECUTANDO, sem preenchimento se
     *    PRONTA, preto+hachurado se SUSPENSA, nada se INATIVA/CONCLUIDA).
     *    Se EXECUTANDO, escrever também o número da CPU dentro da célula
     *    (<text>).
     * 6) chamar marca_em(s, t, i) e desenhar o ícone/marcador
     *    correspondente por cima da célula quando != MARCA_NENHUMA.
     * 7) escrever os rótulos das linhas (id da tarefa) na margem
     *    esquerda, e alguma marcação de tempo no eixo X.
     * 8) escrever a legenda (requisito 2.1: obrigatória) como elementos
     *    SVG à parte (retângulos + texto explicando cada convenção).
     * 9) fechar com </svg> e fclose(f). Devolver 1. */
    (void)s;
    (void)caminho;
    return 0;
}