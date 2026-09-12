/* ============================================================================
 * escalonador.c — Algoritmos de escalonamento e critérios de desempate
 * ==========================================================================*/
#include "escalonador.h"
#include "config.h"    /* str_igual_ci */

#include <stddef.h>
#include <stdlib.h>   /* qsort */

/* --------------------------- tabela de algoritmos ------------------------ */
/* Vetor estático simples: a quantidade de algoritmos é pequena e conhecida.
 * Uma lista encadeada ou hash aqui seria complexidade sem benefício algum. */
#define MAX_ALGORITMOS 8
static Escalonador tabela[MAX_ALGORITMOS];
static int         n_algoritmos = 0;

void escalonador_registrar(const Escalonador *esc)
{
    /* TODO: se esc==NULL ou n_algoritmos>=MAX_ALGORITMOS, ignore. Senão,
     * tabela[n_algoritmos] = *esc (copia o struct inteiro, incluindo o
     * ponteiro de função) e incremente n_algoritmos. */
    (void)esc;
}

int                escalonador_qtde(void)      { return n_algoritmos; }
const Escalonador *escalonador_em(int i)       { return &tabela[i];   }

const Escalonador *escalonador_buscar(const char *nome)
{
    /* TODO: percorrer 'tabela' comparando tabela[i].nome com 'nome' via
     * str_igual_ci (requisito 3.3.2: case-insensitive). Devolver o
     * ponteiro pro primeiro que bater, ou NULL se nenhum bater. */
    (void)nome;
    return NULL;
}

/* ------------------------ algoritmos nativos (req. 4) -------------------- */

/* RATE MONOTONIC (preemptivo)
 * Prioridade FIXA, inversamente proporcional ao período: quanto menor o
 * período, mais frequentemente a tarefa precisa rodar, logo mais urgente ela
 * é. Como o período não muda, a prioridade de uma tarefa é sempre a mesma. */
static long prio_rm(const TCB *t, int tick)
{
    /* TODO: devolver um valor onde "maior = mais prioritário" (convenção
     * usada em TODO escalonador deste projeto). Uma forma simples:
     * retornar -(long)t->periodo (período menor -> valor menos negativo
     * -> maior -> mais prioritário). 'tick' não é usado aqui, porque a
     * prioridade do RM é fixa. */
    (void)t;
    (void)tick;
    return 0;
}

/* EARLIEST DEADLINE FIRST (preemptivo)
 * Prioridade DINÂMICA: vence quem tem o deadline absoluto mais próximo.
 * O deadline absoluto é recalculado a cada ativação (ver kernel.c). */
static long prio_edf(const TCB *t, int tick)
{
    /* TODO: mesma convenção do RM, mas usando t->deadline_abs em vez do
     * período (retornar -(long)t->deadline_abs, por exemplo). */
    (void)t;
    (void)tick;
    return 0;
}

void escalonador_registrar_padroes(void)
{
    /* TODO: montar um Escalonador {"RM", "...", prio_rm} e outro {"EDF",
     * "...", prio_edf} e chamar escalonador_registrar() pra cada um. */
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
    /* TODO:
     * 1) calcular menor = min(ida,idb), maior = max(ida,idb) -- é
     *    essencial usar o PAR ORDENADO no hash, senão a antissimetria
     *    quebra.
     * 2) combinar (tick, menor, maior) numa função hash simples (ex. uma
     *    variante de FNV-1a: h = 2166136261; h = (h ^ v) * 16777619; para
     *    cada v na sequência).
     * 3) usar 1 bit do hash pra decidir se 'menor' ou 'maior' "ganha".
     * 4) devolver o resultado NA PERSPECTIVA de (ida, idb): se ida==menor
     *    e menor ganhou, devolve -1 (ida vem antes); se ida==menor e
     *    menor perdeu, devolve +1; e o espelho se ida==maior. Confira que
     *    sorteio(tick,a,b) == -sorteio(tick,b,a) escrevendo um teste. */
    (void)tick;
    (void)ida;
    (void)idb;
    return 0;
}

/* Verdadeiro se a e b empatam em TODOS os critérios anteriores ao sorteio.
 * Serve para marcar, no Gantt, que a escolha foi decidida na sorte (4.3-5). */
static int empate_ate_sorteio(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    /* TODO: devolver 1 somente se: (estado==EXECUTANDO) for igual nos
     * dois, esc->prioridade(a,tick)==esc->prioridade(b,tick),
     * deadline_abs igual, ingresso igual e duracao igual. Qualquer
     * diferença nesses campos -> devolver 0 (o desempate já foi decidido
     * antes do sorteio). */
    (void)esc;
    (void)a;
    (void)b;
    (void)tick;
    return 0;
}

/* Comparação completa: <0 se `a` deve ser escolhida antes de `b`. */
static int comparar(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    /* TODO, na ordem EXATA do requisito 4.3 (critério de desempate só
     * entra quando o anterior empata):
     *   0) [antes de tudo] esc->prioridade(a,tick) vs esc->prioridade(b,tick)
     *      -- é a prioridade do PRÓPRIO algoritmo (RM ou EDF); quem tem
     *      valor maior deveria vir primeiro (return -1).
     *   1) a que já estava EST_EXECUTANDO evita troca de contexto à toa.
     *   2) menor a->deadline_abs.
     *   3) menor a->ingresso.
     *   4) menor a->duracao.
     *   5) sorteio(tick, a->id, b->id) -- último recurso, sempre decide.
     * Repare que a ordem certa começa pela prioridade do algoritmo e só
     * DEPOIS aplica o critério (1); é isso que o enunciado quer dizer com
     * "em caso de empate na escolha da próxima tarefa". */
    (void)esc;
    (void)a;
    (void)b;
    (void)tick;
    return 0; /* (5) */
}

void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n)
{
    /* TODO:
     * 1) qsort não aceita parâmetros extras no comparador -- ou usar
     *    variáveis estáticas de arquivo (contexto: esc, e, tick) lidas
     *    por uma função de comparação `int cmp(const void*, const
     *    void*)` que busca as TCBs por índice e chama comparar(), ou
     *    implementar sua própria ordenação (insertion sort é suficiente
     *    pra n pequeno, e evita esse truque).
     * 2) depois de ordenar 'indices' (mais prioritária primeiro), marcar
     *    e->tarefas[idx].sorteada = 0 pra todo mundo em 'indices', e
     *    então, pra cada par ADJACENTE no resultado ordenado, se
     *    empate_ate_sorteio(...) for verdadeiro, marcar sorteada = 1 na
     *    tarefa que ficou na frente do par (é ela quem "venceu" o
     *    sorteio) -- é o gancho usado no gantt pra desenhar o marcador do
     *    requisito 4.3-5. */
    (void)esc;
    (void)e;
    (void)indices;
    (void)n;
}