/* ============================================================================
 * escalonador.c — algoritmos de escalonamento e critérios de desempate
 *
 * RESUMO
 *  - Um algoritmo é só UMA função de prioridade. Valor MAIOR ganha.
 *      RM  = -(período)        prioridade fixa
 *      EDF = -(deadline_abs)   prioridade dinâmica
 *  - O kernel não conhece RM nem EDF. Ele só chama escalonador_buscar (ao
 *    carregar o arquivo) e escalonador_ordenar (a cada tick).     [req. 4.2]
 *  - Algoritmo novo: escrever a função de prioridade e registrar em
 *    escalonador_registrar_padroes. Nenhuma linha do kernel muda.
 *  - Empate: 5 critérios em cascata, em comparar_sem_sorteio.      [req. 4.3]
 * ==========================================================================*/
#include "escalonador.h"
#include "config.h" /* str_igual_ci */

#include <stddef.h>

/* --------------------------- tabela de algoritmos ------------------------ */

/* Algoritmos registrados. Vetor de tamanho fixo: são poucos, e assim não
 * precisa de malloc. */
#define MAX_ALGORITMOS 8
static Escalonador tabela[MAX_ALGORITMOS];
static int n_algoritmos = 0;

/* Guarda um algoritmo na tabela.
 * A atribuição copia a struct INTEIRA, por isso quem chama pode passar uma
 * variável local. Os textos não são copiados, só os ponteiros: use strings
 * literais. Ponteiro nulo ou tabela cheia: ignora. */
void escalonador_registrar(const Escalonador *esc)
{
    if (esc == NULL || n_algoritmos >= MAX_ALGORITMOS)
        return;
    tabela[n_algoritmos] = *esc;
    n_algoritmos++;
}

/* Para listar os algoritmos disponíveis (mensagens de erro, interface). */
int escalonador_qtde(void) { return n_algoritmos; }
const Escalonador *escalonador_em(int i) { return &tabela[i]; }

/* Procura um algoritmo pelo nome, sem diferenciar maiúsculas: "rm" acha
 * "RM" (req. 3.3.2). Devolve NULL se não existir.
 * O ponteiro devolvido aponta para a própria tabela: vale o programa
 * inteiro e não precisa de free. */
const Escalonador *escalonador_buscar(const char *nome)
{
    int i;
    for (i = 0; i < n_algoritmos; i++)
    {
        if (str_igual_ci(tabela[i].nome, nome))
        {
            return &tabela[i];
        }
    }

    return NULL;
}

/* ------------------------ algoritmos nativos (req. 4) -------------------- */

/* RATE MONOTONIC: menor período = mais prioritária.
 * Prioridade FIXA: o período nunca muda, então a ordem entre duas tarefas é
 * sempre a mesma.
 * O tick não é usado. O parâmetro existe porque todos os algoritmos têm a
 * mesma assinatura (é o que permite trocar um pelo outro). */
static long prio_rm(const TCB *t, int tick)
{
    (void)tick;
    /* Sinal trocado: no projeto o valor MAIOR ganha, e no RM ganha o MENOR
     * período. */
    return -(long)t->periodo;
}

/* EARLIEST DEADLINE FIRST: deadline absoluto mais próximo = mais prioritária.
 * Prioridade DINÂMICA: o deadline_abs é recalculado pela ativar() do kernel
 * a cada período, e por isso a ordem entre duas tarefas pode virar.
 * Também não usa o tick: o prazo já está pronto no TCB. */
static long prio_edf(const TCB *t, int tick)
{
    (void)tick;
    /* Sinal trocado: o valor MAIOR ganha, e no EDF ganha o MENOR deadline. */
    return -(long)t->deadline_abs;
}

/* Registra RM e EDF. O main chama antes de qualquer outra coisa.
 * Campos: nome usado no arquivo de configuração, descrição, função.
 * Para incluir um algoritmo novo: mais uma struct e mais uma chamada aqui. */
void escalonador_registrar_padroes(void)
{
    Escalonador rm = {"RM", "Rate Monotonic", prio_rm};
    Escalonador edf = {"EDF", "Earliest Deadline First", prio_edf};

    escalonador_registrar(&rm);
    escalonador_registrar(&edf);
}

/* ----------------------------- desempate (4.3) --------------------------- */

/* Critério 5: sorteio entre duas tarefas empatadas em tudo.
 * Devolve -1 se ida ganha, +1 se idb ganha.
 *
 * NÃO usa rand(): o simulador retrocede e avança (req. 1.5.2), então o
 * sorteio tem que dar SEMPRE a mesma resposta para a mesma situação. O
 * resultado é uma conta (hash) feita com o tick e os dois ids.
 *
 * A conta usa (menor id, maior id), nessa ordem, para que sorteio(a,b) e
 * sorteio(b,a) apontem o mesmo vencedor. */
static int sorteio(int tick, int ida, int idb)
{
    int menor = (ida < idb) ? ida : idb;
    int maior = (ida > idb) ? ida : idb;
    unsigned h = 2166136261u;

    if (ida == idb)
        return 0; /* uma tarefa não disputa sorteio consigo mesma */

    /* Mistura os três números num valor "bagunçado" (hash FNV-1a).
     * unsigned porque a multiplicação estoura de propósito. */
    h = (h ^ (unsigned)tick)  * 16777619u;
    h = (h ^ (unsigned)menor) * 16777619u;
    h = (h ^ (unsigned)maior) * 16777619u;

    /* Um bit do meio do hash decide se ganha o menor id ou o maior.
     * (O último bit não serve: só alterna entre par e ímpar.) */
    int menor_ganha = (h >> 16) & 1;
    int vencedor = menor_ganha ? menor : maior;

    /* A resposta é do ponto de vista de ida: -1 se foi ela que ganhou. */
    return (ida == vencedor) ? -1 : 1;
}

/* Compara duas tarefas pelos critérios 0 a 4 do requisito 4.3.
 * Devolve negativo se `a` ganha, positivo se `b` ganha, 0 se empataram em
 * todos.
 *
 * Funciona em CASCATA: cada critério só é consultado se o anterior empatou.
 * O primeiro que diferenciar as duas tarefas decide.
 *
 * Critério 2: "prazo" é o valor lido do arquivo (contado a partir da
 * ativação), e não o deadline absoluto. */
static int comparar_sem_sorteio(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    long pa = esc->prioridade(a, tick);
    long pb = esc->prioridade(b, tick);
    if (pa != pb)
        return (pa > pb) ? -1 : 1; // (0) prioridade do algoritmo: MAIOR ganha

    int ea = (a->estado == EST_EXECUTANDO);
    int eb = (b->estado == EST_EXECUTANDO);
    if (ea != eb)
        return (ea > eb) ? -1 : 1; // (1) quem já executava: evita troca de contexto

    if (a->prazo != b->prazo)
        return (a->prazo < b->prazo) ? -1 : 1; // (2) menor prazo

    if (a->ingresso != b->ingresso)
        return (a->ingresso < b->ingresso) ? -1 : 1; // (3) quem chegou antes

    if (a->duracao != b->duracao)
        return (a->duracao < b->duracao) ? -1 : 1; // (4) menor duração

    return 0; // empate em todos os critérios
}

/* Verdadeiro se as duas empatam em tudo antes do sorteio.
 * Serve para o Gantt marcar que a escolha foi decidida na sorte (4.3-5). */
static int empate_ate_sorteio(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    return comparar_sem_sorteio(esc, a, b, tick) == 0;
}

/* Decide qual de duas tarefas vem primeiro.
 * Se os critérios 0 a 4 decidiram (c diferente de 0), devolve c.
 * Se empatou em tudo (c igual a 0), quem decide é o sorteio. */
static int comparar(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    int c = comparar_sem_sorteio(esc, a, b, tick);
    return c ? c : sorteio(tick, a->id, b->id);
}

/* Ordena 'indices' da tarefa mais prioritária para a menos.
 * 'indices' guarda POSIÇÕES em e->tarefas (não as tarefas, nem os ids).
 * É a única função que o kernel chama a cada tick: as ncpus primeiras
 * posições do resultado ganham processador.
 *
 * Ordenação por inserção, como arrumar cartas na mão: pega uma tarefa e a
 * desliza para a esquerda até achar o lugar dela. Foi escolhida em vez do
 * qsort porque a comparação precisa de esc e do tick, e o qsort não repassa
 * esses dados. */
void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n)
{
    for (int i = 1; i < n; i++)
    {
        int atual = indices[i]; /* a "carta" da vez */
        int j = i - 1;

        /* Enquanto a tarefa em j deve vir DEPOIS da atual (comparar > 0),
         * empurra ela uma casa para a direita. O j >= 0 vem primeiro para
         * nunca ler indices[-1]. */
        while (j >= 0 && comparar(esc, &e->tarefas[indices[j]], &e->tarefas[atual], e->tick) > 0)
        {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = atual; /* encaixa no lugar */
    }

    /* Marca quem ganhou a CPU no sorteio (req. 4.3-5: o Gantt mostra um
     * marcador). Primeiro zera TODAS as tarefas, e não só as de 'indices':
     * quem ganhou no sorteio e terminou neste tick já não é candidata, e a
     * marca ficaria grudada nela até a próxima ativação. */
    for (int i = 0; i < e->ntarefas; i++)
        e->tarefas[i].sorteada = 0;

    /* O sorteio só decide quem executa quando o empate atravessa o corte das
     * CPUs: alguém que ficou com vaga empata em tudo com a primeira tarefa
     * que ficou de fora. Empate entre duas que rodam, ou entre duas que
     * esperam, não muda quem executa. */
    if (e->ncpus > 0 && n > e->ncpus)
    {
        const TCB *de_fora = &e->tarefas[indices[e->ncpus]];
        for (int i = e->ncpus - 1; i >= 0; i--)
        {
            TCB *com_vaga = &e->tarefas[indices[i]];
            if (!empate_ate_sorteio(esc, com_vaga, de_fora, e->tick))
                break;
            com_vaga->sorteada = 1;
        }
    }
}
