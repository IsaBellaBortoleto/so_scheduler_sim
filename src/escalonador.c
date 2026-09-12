/* ============================================================================
 * escalonador.c — Algoritmos de escalonamento e critérios de desempate
 * ==========================================================================*/
#include "escalonador.h"
#include "config.h"    /* str_igual_ci */

#include <stddef.h>

/* --------------------------- tabela de algoritmos ------------------------ */
/* Vetor estático simples: a quantidade de algoritmos é pequena e conhecida.
 * Uma lista encadeada ou hash aqui seria complexidade sem benefício algum. */
#define MAX_ALGORITMOS 8
static Escalonador tabela[MAX_ALGORITMOS];
static int         n_algoritmos = 0;

void escalonador_registrar(const Escalonador *esc)
{

}

int                escalonador_qtde(void)      { return n_algoritmos; }
const Escalonador *escalonador_em(int i)       { return &tabela[i];   }

const Escalonador *escalonador_buscar(const char *nome)
{

}

/* ------------------------ algoritmos nativos (req. 4) -------------------- */

/* RATE MONOTONIC (preemptivo)
 * Prioridade FIXA, inversamente proporcional ao período: quanto menor o
 * período, mais frequentemente a tarefa precisa rodar, logo mais urgente ela
 * é. Como o período não muda, a prioridade de uma tarefa é sempre a mesma. */
static long prio_rm(const TCB *t, int tick)
{

}

/* EARLIEST DEADLINE FIRST (preemptivo)
 * Prioridade DINÂMICA: vence quem tem o deadline absoluto mais próximo.
 * O deadline absoluto é recalculado a cada ativação (ver kernel.c). */
static long prio_edf(const TCB *t, int tick)
{

}

void escalonador_registrar_padroes(void)
{

}

/* ----------------------------- desempate (4.3) --------------------------- */

/* Sorteio DETERMINÍSTICO.
 *
 * Por que não usar rand()? Porque o requisito 1.5.2 permite retroceder e
 * avançar a simulação. Se o sorteio fosse aleatório de verdade, avançar após
 * retroceder poderia produzir um resultado diferente e o histórico ficaria
 * inconsistente com o gráfico. Aqui o "sorteio" é uma função hash de
 * (tick, menor id, maior id): imprevisível para o usuário, mas sempre igual
 * para a mesma situação — ou seja, reprodutível.
 *
 * O hash é calculado sobre o par ORDENADO para garantir antissimetria:
 * sorteio(a,b) == -sorteio(b,a). Sem isso, a ordenação poderia ficar
 * inconsistente e o resultado dependeria da ordem de comparação.
 */
static int sorteio(int tick, int ida, int idb)
{

}

/* Verdadeiro se a e b empatam em TODOS os critérios anteriores ao sorteio.
 * Serve para marcar, no Gantt, que a escolha foi decidida na sorte (4.3-5). */
static int empate_ate_sorteio(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{

}

/* Comparação completa: <0 se `a` deve ser escolhida antes de `b`. */
static int comparar(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
                            /* (5) */
}

void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n)
{

}
