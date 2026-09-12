/* ============================================================================
 * gantt.c — Desenho do gráfico de Gantt (requisito 2)
 * ----------------------------------------------------------------------------
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

#ifdef _WIN32
#  include <windows.h>   /* API do próprio sistema, não é biblioteca externa */
#endif

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

}

/* Ordem das linhas: índices das tarefas ordenados por id CRESCENTE.
 * linha 0 = menor id = mais próxima do eixo X (requisito 2.5). */
static void ordem_por_id(const Estado *e, int *ordem)
{

}

/* ==========================================================================
 *  SAÍDA 1 — TERMINAL (requisito 2.3: atualizado a cada passo)
 * ========================================================================*/

void gantt_preparar_terminal(void)
{

}

/* Texto preto ou branco conforme o brilho do fundo, para a célula continuar
 * legível seja qual for a cor escolhida pelo usuário no arquivo. */
static void cor_fundo(unsigned char r, unsigned char g, unsigned char b)
{

}
static void cor_reset(void) { printf("\x1b[0m"); }

static char simbolo(Marca m)
{

}

void gantt_terminal(const Simulacao *s, int janela)
{

}

/* ==========================================================================
 *  SAÍDA 2 — ARQUIVO SVG (requisito 2.4)
 * ========================================================================*/

#define CEL_W  14   /* largura de 1 tick, em pixels                          */
#define LIN_H  22   /* altura de uma linha do gráfico                        */
#define MARG_E 90   /* margem esquerda, onde vão os rótulos das linhas       */
#define MARG_T 50   /* margem superior, para o título                        */

int gantt_svg(const Simulacao *s, const char *caminho)
{
   
}
