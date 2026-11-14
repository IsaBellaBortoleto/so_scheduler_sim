/* ============================================================================
 * escalonador.c — algoritmos de escalonamento e critérios de desempate
 *
 * RESUMO
 *  - Um algoritmo é DADO, não código: uma lista de critérios (campo do TCB +
 *    "menor" ou "maior" ganha) e a flag preemptivo. Exemplos:
 *      RM   = periodo;menor        prioridade fixa
 *      EDF  = deadline_abs;menor   prioridade dinâmica
 *      FIFO = ativacao;menor       não preemptivo
 *  - O usuário cria algoritmos sem recompilar: por um .txt ou pelo
 *    formulário da página, ambos lidos por escalonador_carregar_texto.
 *    RM e EDF são registrados pelo mesmo caminho.               [req. 4.2]
 *  - O kernel não conhece nenhum algoritmo. Ele só chama escalonador_buscar
 *    (ao carregar o arquivo) e escalonador_ordenar (a cada evento).
 *  - Empate: 5 critérios em cascata, em comparar_sem_sorteio.    [req. 4.3]
 * ==========================================================================*/
#include "escalonador.h"
#include "config.h" /* str_igual_ci */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* --------------------------- tabela de algoritmos ------------------------ */

/* Algoritmos registrados. Vetor de tamanho fixo: são poucos, e assim não
 * precisa de malloc. Valem até o programa fechar. */
#define MAX_ALGORITMOS 32
static Escalonador tabela[MAX_ALGORITMOS];
static int n_algoritmos = 0;

/* Retorna a quantidade exata de algoritmos atualmente registados no sistema. */
int escalonador_qtde(void) { return n_algoritmos; }
//Retorna um algoritmo específico da tabela com base no seu índice (0 até n-1).
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

/* --------------------------- campos e critérios -------------------------- */

/* Campos do TCB que um critério pode usar, pelo nome que o usuário escreve.
 * Todos são int. 'desloc' é a posição do campo dentro do TCB (offsetof):
 * assim um critério guarda só o índice desta tabela, e valor_campo lê o
 * campo certo sem um switch por algoritmo. Campo novo = uma linha aqui. */
static const struct { const char *nome; size_t desloc; } campos[] = {
    {"periodo",       offsetof(TCB, periodo)},
    {"prazo",         offsetof(TCB, prazo)},
    {"deadline_abs",  offsetof(TCB, deadline_abs)},
    {"ativacao",      offsetof(TCB, ativacao)},
    {"ingresso",      offsetof(TCB, ingresso)},
    {"duracao",       offsetof(TCB, duracao)},
    {"exec_restante", offsetof(TCB, exec_restante)},
};
#define N_CAMPOS ((int)(sizeof campos / sizeof campos[0]))

/* Valor do campo de índice 'campo' na tarefa 't'. */
static long valor_campo(const TCB *t, int campo)
{
    return *(const int *)((const char *)t + campos[campo].desloc);
}

/* ------------------------ algoritmos nativos (req. 4) -------------------- */

/* Registra RM e EDF. O main chama antes de qualquer outra coisa.
 * Eles passam pelo MESMO caminho de um algoritmo criado pelo usuário
 * (escalonador_carregar_texto), com o mesmo formato do .txt:
 *   RM  = menor período ganha. Prioridade FIXA: o período não muda.
 *   EDF = menor deadline absoluto ganha. Prioridade DINÂMICA: o
 *         deadline_abs é recalculado pela ativar() do kernel a cada período.
 * Os dois são preemptivos (req. 4). O motivo de recusa é ignorado porque
 * estes textos são fixos e válidos. */
void escalonador_registrar_padroes(void)
{
    char m[128];
    escalonador_carregar_texto("RM;Rate Monotonic;sim\nperiodo;menor", m, sizeof m);
    escalonador_carregar_texto("EDF;Earliest Deadline First;sim\ndeadline_abs;menor", m, sizeof m);
}

/* 1 se o algoritmo usa exec_restante (ex. SRTF), que muda a cada tick na
 * CPU. Fica aqui porque o kernel não conhece nomes de campos (req. 4.2). */
int escalonador_muda_com_a_execucao(const Escalonador *esc)
{
    for (int i = 0; i < esc->n_criterios; i++)
        if (strcmp(campos[esc->criterios[i].campo].nome, "exec_restante") == 0)
            return 1;
    return 0;
}

/* ----------------------------- desempate (4.3) --------------------------- */

/* Critério 5: sorteio entre duas tarefas empatadas em tudo.
 * Devolve -1 se ida ganha, +1 se idb ganha.
 *
 * NÃO usa rand(): o simulador retrocede e avança (req. 1.5.2), então o
 * sorteio tem que dar SEMPRE a mesma resposta para a mesma situação. O
 * resultado é uma conta (hash FNV-1a) feita com o tick e os dois ids.
 *
 * Os ids são TEXTO: o hash mistura as letras de cada um. Elas entram
 * sempre na ordem (menor id, maior id), dada por id_comparar, para que
 * sorteio(a,b) e sorteio(b,a) apontem o mesmo vencedor. */
static int sorteio(int tick, const char *ida, const char *idb)
{
    int c = id_comparar(ida, idb);
    const char *menor = (c <= 0) ? ida : idb;
    const char *maior = (c <= 0) ? idb : ida;
    const char *vencedor;
    unsigned h = 2166136261u;

    if (c == 0)
        return 0; /* uma tarefa não disputa sorteio consigo mesma */

    /* Mistura tudo num valor "bagunçado". unsigned porque a multiplicação
     * estoura de propósito. */
    h = (h ^ (unsigned)tick) * 16777619u;
    for (const char *p = menor; *p; p++)
        h = (h ^ (unsigned char)*p) * 16777619u;
    /* Separador entre os dois ids: sem ele, ("ab","c") e ("a","bc")
     * misturariam as mesmas letras na mesma ordem e dariam o mesmo hash.
     * 0xFF nunca aparece num id (só letras, números, '_' e '-'). */
    h = (h ^ 0xFFu) * 16777619u;
    for (const char *p = maior; *p; p++)
        h = (h ^ (unsigned char)*p) * 16777619u;

    /* Um bit do meio do hash decide se ganha o menor id ou o maior.
     * (O último bit não serve: só alterna entre par e ímpar.) */
    vencedor = ((h >> 16) & 1) ? menor : maior;

    /* A resposta é do ponto de vista de ida: -1 se foi ela que ganhou.
     * Compara os PONTEIROS, e não o texto: 'vencedor' é uma cópia de 'ida'
     * ou de 'idb'. */
    return (ida == vencedor) ? -1 : 1;
}

/* Compara duas tarefas pelos critérios 0 a 4 do requisito 4.3.
 * Devolve negativo se `a` ganha, positivo se `b` ganha, 0 se empataram em
 * todos.
 *
 * Funciona em CASCATA: cada critério só é consultado se o anterior empatou.
 * O primeiro que diferenciar as duas tarefas decide.
 *
 * Critério 0 = o algoritmo: a lista de critérios dele, na ordem em que o
 * usuário escreveu.
 * Critério 2: "prazo" é o valor lido do arquivo (contado a partir da
 * ativação), e não o deadline absoluto. */
static int comparar_sem_sorteio(const Escalonador *esc, const TCB *a, const TCB *b)
{
    int ea = (a->estado == EST_EXECUTANDO);
    int eb = (b->estado == EST_EXECUTANDO);

    /* Algoritmo NÃO preemptivo: quem já está na CPU ganha de qualquer um,
     * antes mesmo do critério do algoritmo. Ela só sai ao terminar, no fim
     * do quantum ou se for suspensa: nesses casos o kernel já tirou o estado
     * EXECUTANDO, e ela volta a disputar normalmente. */
    if (!esc->preemptivo && ea != eb)
        return (ea > eb) ? -1 : 1;

    /* (0) o algoritmo: o primeiro campo diferente decide. */
    for (int i = 0; i < esc->n_criterios; i++)
    {
        long va = valor_campo(a, esc->criterios[i].campo);
        long vb = valor_campo(b, esc->criterios[i].campo);
        if (va != vb)
        {
            int a_ganha = esc->criterios[i].maior_ganha ? (va > vb) : (va < vb);
            return a_ganha ? -1 : 1;
        }
    }

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
static int empate_ate_sorteio(const Escalonador *esc, const TCB *a, const TCB *b)
{
    return comparar_sem_sorteio(esc, a, b) == 0;
}

/* Decide qual de duas tarefas vem primeiro.
 * Se os critérios 0 a 4 decidiram (c diferente de 0), devolve c.
 * Se empatou em tudo (c igual a 0), quem decide é o sorteio. */
static int comparar(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{
    /* Invoca a Árvore de Decisão em Cascata (Critérios 0 a 4). 
     * Guarda o resultado matemático (-1, 1 ou 0) na variável local 'c'. */
    int c = comparar_sem_sorteio(esc, a, b);
    /** Lógica do Operador Ternário ( c ? c : sorteio(...) ):
     * - Se 'c' for diferente de zero (-1 ou 1), a expressão avalia como TRUE. 
     *   Devolve imediatamente 'c' e a função 'sorteio' NEM SEQUER É EXECUTADA 
     *   (Short-Circuit Evaluation), poupando ciclos de CPU.
     * - Se 'c' for exatamente 0 (Empate Perfeito), a expressão avalia como FALSE.
     *   Devolve então o resultado da função 'sorteio', forçando o desempate
     *   matemático pelo hash FNV-1a (Critério 5). */
    return c ? c : sorteio(tick, a->id, b->id);
}

/* Ordena 'indices' da tarefa mais prioritária para a menos.
 * 'indices' guarda POSIÇÕES em e->tarefas (não as tarefas, nem os ids).
 * É a única função que o kernel chama ao escalonar: as ncpus primeiras
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
            if (!empate_ate_sorteio(esc, com_vaga, de_fora))
                break;
            com_vaga->sorteada = 1;
        }
    }
}

/* ------------------- algoritmo definido pelo usuário --------------------- */

/* Corta 'p' no próximo ';', tira espaços das pontas do pedaço cortado e
 * devolve o começo do campo seguinte (NULL se não houver). */
static char *cortar(char **p)
{
    char *ini = *p, *fim;
    if (ini == NULL) return "";
    fim = strchr(ini, ';');
    *p = fim ? fim + 1 : NULL;
    if (fim) *fim = '\0';
    while (*ini == ' ' || *ini == '\t') ini++;
    fim = ini + strlen(ini);
    while (fim > ini && (fim[-1] == ' ' || fim[-1] == '\t' || fim[-1] == '\r')) *--fim = '\0';
    return ini;
}

/* Lê a definição de um algoritmo e o registra na tabela. É o único caminho
 * de entrada: o .txt escolhido na página, o formulário (que monta o mesmo
 * texto) e os nativos RM e EDF passam por aqui.
 *
 *   nome;descricao;preemptivo      <- 1ª linha útil; preemptivo = sim | nao
 *   campo;sentido                  <- um critério por linha; menor | maior
 *
 * Vale o mesmo do arquivo de configuração: maiúsculas/minúsculas não
 * importam (3.3.2), o ';' final é opcional (3.3.3), linhas em branco e
 * começadas por '#' são ignoradas (3.3.6).
 * Só registra se TUDO estiver certo: recusado, a tabela fica como estava e
 * 'motivo' diz o porquê (req. 5). */
int escalonador_carregar_texto(const char *texto, char *motivo, int tam)
{
    Escalonador novo = {0};
    char copia[1024], *linha, *prox;
    int cabecalho_lido = 0;

    if (strlen(texto) >= sizeof copia) {
        snprintf(motivo, tam, "Definicao de algoritmo grande demais.");
        return 0;
    }
    strcpy(copia, texto);

    for (linha = copia; linha != NULL; linha = prox) {
        char *resto, *c1, *c2, *c3;
        prox = strchr(linha, '\n');
        if (prox) *prox++ = '\0';
        resto = linha;
        c1 = cortar(&resto); c2 = cortar(&resto); c3 = cortar(&resto);
        if (*c1 == '#' || (*c1 == '\0' && *c2 == '\0' && *c3 == '\0'))
            continue;                                  /* linha em branco ou comentário */

        if (!cabecalho_lido) {                         /* nome;descricao;preemptivo */
            /* A descrição (c2) é aceita e ignorada: nada a mostra. */
            if (*c1 == '\0') {
                snprintf(motivo, tam, "Falta o nome do algoritmo."); return 0;
            }
            if (strlen(c1) >= sizeof novo.nome) {
                snprintf(motivo, tam, "Nome '%s' passa de 15 caracteres.", c1); return 0;
            }
            if (escalonador_buscar(c1) != NULL) {
                snprintf(motivo, tam, "Ja existe um algoritmo chamado '%s'.", c1); return 0;
            }
            if (str_igual_ci(c3, "sim")) novo.preemptivo = 1;
            else if (str_igual_ci(c3, "nao")) novo.preemptivo = 0;
            else { snprintf(motivo, tam, "Preemptivo deve ser 'sim' ou 'nao' (veio '%s').", c3); return 0; }
            for (char *q = c1; *q; q++)                /* guarda em maiúsculas, como "RM" */
                *q = (char)toupper((unsigned char)*q);
            strcpy(novo.nome, c1);                     /* cabe: tamanho conferido acima */
            cabecalho_lido = 1;
            continue;
        }

        /* campo;sentido */
        int campo = -1;
        for (int i = 0; i < N_CAMPOS; i++)
            if (str_igual_ci(campos[i].nome, c1)) campo = i;
        if (campo < 0) {
            snprintf(motivo, tam, "Campo '%s' nao existe (validos: periodo, prazo, deadline_abs, "
                     "ativacao, ingresso, duracao, exec_restante).", c1);
            return 0;
        }
        if (!str_igual_ci(c2, "menor") && !str_igual_ci(c2, "maior")) {
            snprintf(motivo, tam, "Sentido deve ser 'menor' ou 'maior' (veio '%s').", c2); return 0;
        }
        if (novo.n_criterios >= MAX_CRITERIOS) {
            snprintf(motivo, tam, "No maximo %d criterios por algoritmo.", MAX_CRITERIOS); return 0;
        }
        novo.criterios[novo.n_criterios].campo = campo;
        novo.criterios[novo.n_criterios].maior_ganha = str_igual_ci(c2, "maior");
        novo.n_criterios++;
    }

    if (!cabecalho_lido)        { snprintf(motivo, tam, "Definicao vazia."); return 0; }
    if (novo.n_criterios == 0)  { snprintf(motivo, tam, "O algoritmo precisa de pelo menos um criterio."); return 0; }
    if (n_algoritmos >= MAX_ALGORITMOS) {
        snprintf(motivo, tam, "Limite de %d algoritmos atingido.", MAX_ALGORITMOS); return 0;
    }
    tabela[n_algoritmos++] = novo;
    return 1;
}
