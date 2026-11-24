/* ============================================================================
 * kernel.c — O coração da simulação
 * ----------------------------------------------------------------------------
 * ORDEM DOS ACONTECIMENTOS EM UM TICK
 *
 * O estado guardado em `s->atual` está SEMPRE "pronto para ser desenhado":
 * as ativações do instante já ocorreram e o escalonador já decidiu quem ocupa
 * cada CPU. Avançar um tick imita um SO de verdade, em que o TIMER gera uma
 * interrupção e o núcleo trata essa interrupção:
 *
 *    sim_avancar()
 *      tratar_tick()           a "interrupção do relógio":
 *          tick++              o ÚNICO lugar em que o tempo anda
 *          executar()          quem está na CPU trabalha 1 unidade
 *          verificar_prazos()  quem passou do deadline é marcado (req. 3.3)
 *          ativar()            chegadas do tick viram PRONTAS (periódicas na
 *                              fila de prontos; aperiódicas e esporádicas na
 *                              fila de aperiódicas, req. B 1.1)
 *          escalonar()         SÓ SE houve evento (senão só conta CPU ociosa)
 *      snapshot()              congela tudo em historico[tick] (req. 1.5.2)
 *
 * VERIFICAR SEMPRE, AGIR SÓ QUANDO PRECISA (req. 1.1). Todo tick o núcleo
 * verifica se algo aconteceu; o escalonador só roda se aconteceu um EVENTO
 * (chegada, término, fim de quantum, edição, troca de algoritmo,
 * retrocesso), marcado em s->precisa_escalonar. Sem evento a ordem não muda:
 * a prioridade não é calculada, os critérios só LEEM campos do TCB (período
 * no RM, deadline_abs no EDF), e esses campos só mudam em evento. Exceção:
 * critério exec_restante (ex. SRTF), que muda a cada tick de execução --
 * para ele todo tick é evento (escalonador_muda_com_a_execucao).
 *
 * Manter essa ordem fixa é o que torna a simulação DETERMINÍSTICA: rodar,
 * retroceder e avançar de novo produz exatamente o mesmo resultado.
 *
 * NOTA: estado_clonar/estado_liberar/estado_nome moram em tcb.c (é lá que
 * `tcb.h` os declara), não duplicar aqui.
 * ==========================================================================*/
#include "kernel.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Acha a POSIÇÃO de uma tarefa no vetor a partir do ID que o usuário vê.
 * São coisas diferentes: o ID é o TEXTO do arquivo ("t06", "7"...), a
 * posição é 0, 1, 2... Dentro do núcleo tudo anda pela posição (CPU.tarefa,
 * candidatos do escalonador); o id só aparece na entrada (arquivo, edição)
 * e na saída (JSON, Gantt). Devolve -1 se o ID não existe.
 * str_igual_ci: "T06" acha "t06" (req. 3.3.2). Busca simples, uma por uma:
 * com ~190 tarefas e uma busca por clique, não vale nada mais elaborado. */
static int sim_indice_por_id(const Estado *e, const char *id)
{
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (str_igual_ci(e->tarefas[i].id, id))
            return i;
    }
    return -1;
}

static int existe_servidor(const Estado *e);

/* Devolve 1 se TODAS as tarefas estão EST_CONCLUIDA (com ntarefas==0 também:
 * nada pra simular). Usada por sim_iniciar/sim_avancar pra decidir
 * s->terminada. Sem servidor, aperiódica e esporádica nunca executam: não
 * entram na conta, senão a simulação só pararia no limite de ticks (o
 * parser já avisou que elas não executam). */
static int todas_concluidas(const Estado *e)
{
    int esperar_aperiodicas = existe_servidor(e);

    for (int i = 0; i < e->ntarefas; i++)
    {
        const TCB *t = &e->tarefas[i];
        if (t->tipo != TIPO_PERIODICA && !esperar_aperiodicas)
            continue;
        if (t->estado != EST_CONCLUIDA)
            return 0;
    }
    return 1;
}

/* Clona 'origem' em 'destino'. Devolve 1 se deu certo, 0 se faltou memória
 * (estado_clonar devolve os vetores vazios nesse caso; aqui já é liberado). */
static int clonar(Estado *destino, const Estado *origem)
{
    *destino = estado_clonar(origem);
    if (destino->ntarefas != origem->ntarefas || destino->ncpus != origem->ncpus)
    {
        estado_liberar(destino);
        return 0;
    }
    return 1;
}

/* Tira a tarefa da CPU dos DOIS lados: a CPU guarda a tarefa e a tarefa
 * guarda a CPU. */
static void soltar_cpu(Estado *e, TCB *t)
{
    e->cpus[t->cpu].tarefa = -1;
    t->cpu = -1;
}

/* Preempção: tira da CPU e devolve à fila de prontas. */
static void devolver_a_fila(Estado *e, TCB *t)
{
    soltar_cpu(e, t);
    t->estado = EST_PRONTA;
}

/* Troca a foto do tick atual pela versão nova de s->atual (edição, CPUs,
 * algoritmo no tick 0): o Gantt e o retroceder leem o histórico. */
static void refazer_foto(Simulacao *s)
{
    estado_liberar(&s->historico[s->n_hist - 1]);
    s->historico[s->n_hist - 1] = estado_clonar(&s->atual);
}

/* ------------------------------- histórico ------------------------------- */

/* Guarda a fotografia do tick corrente em historico[n_hist]. Devolve 1 se
 * guardou, 0 se faltou memória (o histórico continua como estava).
 *
 * guardamos o estado INTEIRO a cada tick, não um diário de
 * diferenças. Custo = ntarefas * ticks * sizeof(TCB) — 20 tarefas e 5000
 * ticks dão uns 30 MB; 190 tarefas, 60 KB por tick. Em troca, retroceder é
 * só restaurar uma cópia. O teto de memória fica em calcular_limite.
 * Trocar por undo-log só se a simulação precisar de milhões de ticks. */
static int snapshot(Simulacao *s)
{
    if (s->n_hist >= s->cap_hist)
    {
        int nova = (s->cap_hist == 0) ? 64 : s->cap_hist * 2;
        /* Ponteiro temporário: se o realloc falhar, devolve NULL e o
         * histórico antigo continua valendo. Com "s->historico = realloc"
         * ele se perderia, e a linha seguinte escreveria em NULL. */
        Estado *maior = realloc(s->historico, nova * sizeof(Estado));
        if (maior == NULL)
            return 0;
        s->historico = maior;
        s->cap_hist = nova;
    }
    if (!clonar(&s->historico[s->n_hist], &s->atual))
        return 0;
    s->n_hist++;
    return 1;
}

/* LIMITE DE TICKS de uma simulação (req. 2.4: a imagem final não pode
 * ficar presa a um tempo máximo). Calculado a partir do arquivo:
 *   - toda tarefa faz 10 execuções, então a última chegada de cada uma é
 *     em ingresso + 9 * período;
 *   - depois da última chegada falta, no máximo, o trabalho TOTAL (10 *
 *     duração de cada tarefa): o escalonador nunca deixa CPU desligada com
 *     tarefa pronta, então cada tick faz pelo menos 1 unidade de trabalho.
 * A soma das duas é um teto para o fim natural. O dobro dá folga para
 * edições (ex.: uma duração aumentada). Só passa disso uma simulação que
 * não termina sozinha, como uma tarefa suspensa à mão e nunca retomada: é
 * para ela que o limite existe.
 * Nunca menos que MAX_TICKS, e nunca mais do que o histórico (uma foto por
 * tick) cabe em MAX_MEMORIA_HIST. long long: duração e período vêm do
 * arquivo, e a conta em int poderia estourar. */
static int calcular_limite(const Estado *e)
{
    long long ultima_chegada = 0, trabalho = 0, limite, teto, instantes = 0;

    for (int i = 0; i < e->ntarefas; i++)
    {
        const TCB *t = &e->tarefas[i];
        instantes += t->n_instantes;
        /* Última chegada e número de execuções dependem do tipo: a
         * aperiódica chega uma vez, no ingresso; a esporádica, uma vez por
         * instante da lista (instantes[] está em ordem crescente). */
        long long chegada = t->ingresso;
        int execucoes = 1;
        if (t->tipo == TIPO_PERIODICA)
        {
            chegada += (long long)(MAX_ATIVACOES - 1) * t->periodo;
            execucoes = MAX_ATIVACOES;
        }
        else if (t->tipo == TIPO_ESPORADICA && t->n_instantes > 0)
        {
            chegada = t->instantes[t->n_instantes - 1];
            execucoes = t->n_instantes;
        }
        if (chegada > ultima_chegada)
            ultima_chegada = chegada;
        trabalho += (long long)execucoes * t->duracao;
    }
    limite = 2 * (ultima_chegada + trabalho);
    if (limite < MAX_TICKS)
        limite = MAX_TICKS;

    /* Bytes de UMA foto: a struct Estado mais os vetores de tarefas, de CPUs
     * e de instantes das esporádicas (cada foto tem a sua cópia). */
    teto = MAX_MEMORIA_HIST / ((long long)sizeof(Estado) +
                               (long long)e->ntarefas * sizeof(TCB) +
                               (long long)e->ncpus * sizeof(CPU) +
                               instantes * (long long)sizeof(int));
    if (limite > teto)
        limite = teto;
    return (int)limite; /* cabe: teto <= 1 GB / sizeof(Estado) */
}

/* Uma CHEGADA: a tarefa recebe uma nova execução para fazer.
 * Usada por ativar() e pela edição INATIVA -> PRONTA. */
static void ativar_tarefa(TCB *t, int tick)
{
    t->estado = EST_PRONTA;
    t->exec_restante = t->duracao;  /* uma execução inteira pela frente */
    t->quantum_restante = 0;        /* definido em escalonar(), ao ganhar CPU */
    t->ativacao = tick;             /* quando ESTA execução começou       */
    t->deadline_abs = tick + t->prazo; /* o prazo conta a partir da chegada */
    t->perdeu_prazo = 0;
    t->cpu = -1;
    t->sorteada = 0;
}

/* ------------------- Projeto B: aperiódicas e esporádicas ----------------- */

/* [6.4] Agenda a chegada SEGUINTE logo depois de uma chegada (automática em
 * ativar() ou manual na edição). Os três tipos usam o mesmo
 * proxima_ativacao; só muda a regra:
 *   PERIODICA   um período depois
 *   APERIODICA  nunca mais (chega uma vez só)
 *   ESPORADICA  o próximo instante da lista, se ainda houver */
static void agendar_proxima(TCB *t)
{
    switch (t->tipo)
    {
    case TIPO_PERIODICA:
        t->proxima_ativacao += t->periodo;
        break;
    case TIPO_APERIODICA:
        t->proxima_ativacao = SEM_CHEGADA;
        break;
    case TIPO_ESPORADICA:
        /* O teto protege a chegada MANUAL de quem já não tem ocorrência
         * pela frente: prox_evento nunca passa de n_instantes. */
        if (t->prox_evento < t->n_instantes)
            t->prox_evento++;
        t->proxima_ativacao = t->prox_evento < t->n_instantes
                                  ? t->instantes[t->prox_evento]
                                  : SEM_CHEGADA;
        break;
    }
}

/* [6.5] A execução atual tem prazo? Só a aperiódica pode não ter: prazo 0
 * nela = soft (o parser já trocou o 0 da esporádica por |periodo|). */
static int tem_prazo(const TCB *t)
{
    return t->tipo != TIPO_APERIODICA || t->prazo > 0;
}

/* [6.5] Depois de terminar uma execução (ativacoes já contou esta): era a
 * última? As 10 execuções (req. 4.4) são só da periódica; aperiódica e
 * esporádica acabam quando não chega mais nada e nada está esperando. */
static int foi_a_ultima_execucao(const TCB *t)
{
    if (t->tipo == TIPO_PERIODICA)
        return t->ativacoes >= MAX_ATIVACOES;
    return t->proxima_ativacao == SEM_CHEGADA && t->pendentes == 0;
}

/* [6.4] Req. B 1.1: só periódicas disputam a CPU pela fila de prontos.
 * Aperiódicas e esporádicas ficam de fora, senão roubariam CPU das
 * periódicas e invalidariam o teste de escalonabilidade delas.
 * TODO [7]: TIPO_SERVIDOR também entra (o servidor É uma periódica). */
static int vai_para_fila_de_prontos(const TCB *t)
{
    return t->tipo == TIPO_PERIODICA;
}

/* Há algum servidor para executar aperiódicas? Sem servidor elas nunca
 * concluem, e por isso não seguram o fim da simulação (todas_concluidas).
 * TODO [7]: procurar TIPO_SERVIDOR. */
static int existe_servidor(const Estado *e)
{
    (void)e;
    return 0;
}

/* [6.4] Fila de aperiódicas IMPLÍCITA: não é um vetor guardado, é montada
 * na hora (aperiódica/esporádica em PRONTA, por ordem de chegada). Entra no
 * histórico de graça e não há nada para manter sincronizado. Quem o
 * servidor pegar vira EXECUTANDO e sai da fila sozinho: nenhum outro
 * servidor o vê (req. B 2.2). Empate na chegada: id_comparar, para o
 * resultado não depender da ordem do arquivo. */
int sim_fila_aperiodicas(const Estado *e, int *fila)
{
    int n = 0;

    for (int i = 0; i < e->ntarefas; i++)
    {
        const TCB *t = &e->tarefas[i];
        if (t->tipo == TIPO_PERIODICA || t->estado != EST_PRONTA)
            continue;
        /* Inserção ordenada: abre espaço empurrando os que chegaram depois. */
        int k = n++;
        while (k > 0)
        {
            const TCB *antes = &e->tarefas[fila[k - 1]];
            if (antes->ativacao < t->ativacao ||
                (antes->ativacao == t->ativacao && id_comparar(antes->id, t->id) <= 0))
                break;
            fila[k] = fila[k - 1];
            k--;
        }
        fila[k] = i;
    }
    return n;
}

/* ATIVAÇÃO — uma tarefa chega quando o relógio alcança o tick guardado em
 * proxima_ativacao; nesse momento a chegada seguinte já fica agendada
 * (agendar_proxima). Vale para os três tipos: a periódica vai para a fila
 * de prontos e a aperiódica/esporádica para a fila de aperiódicas (as duas
 * são "PRONTA"; quem separa é vai_para_fila_de_prontos).
 *
 * Com o período fixo isso dá os mesmos ticks de antes (ingresso + k*periodo).
 * A diferença aparece quando o usuário EDITA o período: a conta antiga,
 * (tick - ingresso) % periodo == 0, supunha o período novo desde o ingresso
 * e mudava a fase de todas as chegadas. Guardando a próxima chegada, o
 * período novo só muda o intervalo dali em diante. */
static void ativar(Simulacao *s)
{
    Estado *executando = &s->atual;
    for (int i = 0; i < executando->ntarefas; i++)
    {
        TCB *tarefa = &executando->tarefas[i];
        int chegou = executando->tick == tarefa->proxima_ativacao;
        if (!chegou)
            continue;
        agendar_proxima(tarefa); /* agenda a seguinte */

        if (tarefa->estado == EST_INATIVA)
        {
            ativar_tarefa(tarefa, executando->tick); /* livre: começa já */
            s->precisa_escalonar = 1; /* EVENTO: uma PRONTA nova na fila (na
                                       * seção 7, um servidor reage a ela) */
        }
        else if (tarefa->estado != EST_CONCLUIDA)
            tarefa->pendentes++; /* ocupada: espera a vez. NÃO é evento: a
                                  * tarefa já estava na disputa e a fila de
                                  * prontos não mudou. */
    }
}

/* ESCALONAMENTO — requisito 1.2.
 * A fila de prontos é GLOBAL (um único vetor de candidatas para todas as
 * CPUs, não uma fila por processador) e a ociosidade é minimizada: só sobra
 * CPU desligada se realmente não houver tarefa pronta.
 *
 * Roda só em evento (ver o topo do arquivo). Quando roda, reavalia do zero quem deve ocupar cada CPU (é isso que torna
 * o escalonamento PREEMPTIVO): uma tarefa só continua na mesma CPU se ainda
 * estiver entre as mais prioritárias segundo escalonador_ordenar (que já
 * aplica os critérios de desempate do requisito 4.3, dando vantagem a quem
 * já estava executando só como critério de desempate, não como prioridade
 * absoluta). */

/* OCIOSIDADE (req. 1.2) — mais um tick desligada para cada CPU vazia.
 * Passo 6 de escalonar(), e chamada direto em tick sem evento. */
static void contar_cpus_desligadas(Estado *e)
{
    for (int c = 0; c < e->ncpus; c++)
        if (e->cpus[c].tarefa == -1)
            e->cpus[c].ticks_desligada++;
}

static void escalonar(Simulacao *s)
{
    /* Os seis passos estão numerados abaixo. A ideia central: TODAS as
     * prontas e as que estão executando disputam as CPUs de novo. Por isso
     * uma tarefa mais prioritária que chega toma a CPU de quem está rodando
     * (preempção). Quem continua rodando só ganha vantagem no desempate
     * (critério 1 do req. 4.3), não passe livre. */
    Estado *e = &s->atual;

    /* 1) Montar a lista de candidatos */
    int candidatos[e->ntarefas];
    int n_candidatos = 0;
    
    for (int i = 0; i < e->ntarefas; i++)
    {
        if ((e->tarefas[i].estado == EST_PRONTA || e->tarefas[i].estado == EST_EXECUTANDO) &&
            vai_para_fila_de_prontos(&e->tarefas[i]))
        {
            candidatos[n_candidatos] = i;
            n_candidatos++;
        }
    }

    /* 2) Ordenar a lista usando o escalonador atual */
    if (n_candidatos > 0)
    {
        escalonador_ordenar(s->esc, e, candidatos, n_candidatos);
    }
    
    /* 3) Determinar quantos vencedores teremos (menor entre candidatos e cpus) */
    int vencedores = (n_candidatos < e->ncpus) ? n_candidatos : e->ncpus;

    /* 4) Preempção: tirar da CPU quem estava EXECUTANDO mas perdeu a vaga.
     * Toda tarefa EXECUTANDO é candidata, então quem perdeu a vaga está
     * em candidatos[vencedores..n_candidatos). */
    for (int j = vencedores; j < n_candidatos; j++)
    {
        TCB *t = &e->tarefas[candidatos[j]];
        if (t->estado == EST_EXECUTANDO)
            devolver_a_fila(e, t);
    }

    /* 5) Acomodar os vencedores que ainda não têm CPU */
    for (int k = 0; k < vencedores; k++)
    {
        int id = candidatos[k];
        if (e->tarefas[id].cpu == -1)
        {
            // Procura a primeira CPU livre
            for (int c = 0; c < e->ncpus; c++)
            {
                if (e->cpus[c].tarefa == -1) 
                {
                    e->tarefas[id].cpu = c;
                    e->cpus[c].tarefa = id;    
                    e->tarefas[id].estado = EST_EXECUTANDO; 
                    e->tarefas[id].quantum_restante = e->quantum;
                    break;
                }
            }
        }
    }

    /* 6) Contabilizar a ociosidade das CPUs que sobraram sem ninguém */
    contar_cpus_desligadas(e);
}

/* EXECUÇÃO — consome uma unidade de tempo de cada tarefa que está na CPU,
 * e trata tanto o término da ativação quanto o estouro do quantum
 * (requisito 3.3: "quantum indica o período máximo de tempo que uma tarefa
 * pode executar" antes de precisar ceder a CPU). */
static void executar(Simulacao *s)
{
    /* Para cada tarefa na CPU: gasta 1 unidade de trabalho e 1 de quantum,
     * e então decide se ela (a) terminou esta execução, (b) esgotou o
     * quantum e devolve a CPU, ou (c) segue executando. (a) e (b) são
     * EVENTOS (precisa_escalonar). Roda depois do tick++ sem problema: não
     * lê o tick, só contabiliza o intervalo [tick-1, tick) que acabou. */
    Estado *executando = &s->atual;
    for(int i = 0; i < executando->ntarefas; i++)
    {
         TCB *tarefa = &executando->tarefas[i];
         if (tarefa->estado == EST_EXECUTANDO)
         {
             tarefa->exec_restante--;
             if (executando->quantum > 0 && tarefa->quantum_restante > 0)
             {
                 tarefa->quantum_restante--;
             }
             /* (a) Terminou esta execução: solta a CPU e conta mais uma. Na
              * 10a vira CONCLUIDA (req. 4.4); antes disso volta a INATIVA e
              * espera a próxima chegada do período. */
             if (tarefa->exec_restante <= 0)
             {
                 soltar_cpu(executando, tarefa);
                 tarefa->quantum_restante = 0;
                 tarefa->ativacoes++;
                 if (foi_a_ultima_execucao(tarefa)) /* [6.5] */
                 {
                     tarefa->estado = EST_CONCLUIDA;
                     tarefa->pendentes = 0; /* fez as 10: acabou */
                 }
                 else if (tarefa->pendentes > 0)
                 {
                     /* Já havia outra execução esperando: o prazo dela conta
                      * da chegada DELA (periódica: um período depois da que
                      * acabou de terminar). */
                     int chegada = tcb_chegada_pendente(tarefa, 1); /* [6.5] */
                     tarefa->pendentes--;
                     ativar_tarefa(tarefa, chegada);
                 }
                 else
                 {
                     tarefa->estado = EST_INATIVA;
                 }
                 s->precisa_escalonar = 1; /* EVENTO: término, CPU livre */
             }
             /* (b) Quantum esgotado com trabalho por fazer: devolve a CPU e
              * volta para a fila. quantum 0 no arquivo = sem limite, por
              * isso o "quantum > 0". */
             else if (executando->quantum > 0 && tarefa->quantum_restante <= 0)
             {
                 soltar_cpu(executando, tarefa);
                 tarefa->estado = EST_PRONTA;
                 s->precisa_escalonar = 1; /* EVENTO: fim de quantum */
             }
         }
    }
}

/* PRAZOS — marca a execução que chegou ao prazo absoluto (chegada + Di)
 * sem terminar. Roda logo DEPOIS do tick++, então "tick >= deadline_abs"
 * pega o instante exato do prazo (req. 2.2): com ">" uma execução que ainda
 * roda no tick do prazo e termina no seguinte nunca seria marcada. Quem
 * termina exatamente no prazo já está INATIVA e não é marcada (cumpriu). */
static void verificar_prazos(Simulacao *s)
{
    /* A tarefa continua executando normalmente depois de perder o prazo;
     * só fica marcada pro gantt mostrar o erro (requisito 3.3). */
    Estado *executando = &s->atual;
    for (int i = 0; i < executando->ntarefas; i++)
    {
        TCB *tarefa = &executando->tarefas[i];
        if (tarefa->estado != EST_INATIVA && tarefa->estado != EST_CONCLUIDA &&
            tem_prazo(tarefa) && /* [6.5] */
            executando->tick >= tarefa->deadline_abs)
            tarefa->perdeu_prazo = 1;
    }
}

/* ------------------------------ ciclo de vida ----------------------------- */

/* "RM, EDF": os nomes dos algoritmos disponíveis, para as mensagens de erro. */
static void listar_algoritmos(char *lista, size_t tam)
{
    lista[0] = '\0';
    for (int i = 0; i < escalonador_qtde(); i++)
    {
        size_t usado = strlen(lista);
        snprintf(lista + usado, tam - usado, "%s%s",
                 i ? ", " : "", escalonador_em(i)->nome);
    }
}

int sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d)
{
    /* A busca vem ANTES de mexer em 's': se o algoritmo não existe, a
     * simulação que já estava carregada continua intacta. */
    const Escalonador *esc = escalonador_buscar(inicial->algoritmo);
    if (esc == NULL)
    {
        /* Erro claro (requisito 5): diz o que veio, o que é aceito e o que
         * fazer -- o algoritmo pode ser criado pelo usuário (req. 4.2). */
        char lista[128];
        listar_algoritmos(lista, sizeof lista);
        diag_erro(d, "algoritmo \"%s\" nao existe (disponiveis: %s). "
                     "Para usa-lo, adicione-o em \"novo algoritmo\" "
                     "e carregue o arquivo de novo.",
                  inicial->algoritmo, lista);
        return 0;
    }

    /* Começa limpo. Se já havia uma simulação (o usuário carregou um
     * segundo arquivo), o histórico dela é liberado aqui; se 's' estava
     * zerada, sim_liberar não faz nada. Só chega aqui quem passou na
     * validação acima: um arquivo recusado não apaga a simulação anterior.
     * CONTRATO: 's' tem que estar zerada (memset) ou já iniciada, nunca
     * com lixo de memória. */
    sim_liberar(s);
    s->esc = esc;

    /* Cópia independente: quem chamou pode liberar 'inicial' depois. Se o
     * malloc falhar, o clone volta com ntarefas/ncpus zerados. */
    if (!clonar(&s->atual, inicial))
    {
        diag_erro(d, "falta de memoria ao iniciar a simulacao");
        return 0;
    }

    s->limite_ticks = calcular_limite(&s->atual);
    ativar(s);    /* ativações que já valem no tick 0 */
    escalonar(s); /* primeira atribuição de CPUs: no tick 0 sempre escalona */
    s->precisa_escalonar = 0;  /* os eventos do tick 0 já foram atendidos */
    s->terminada = todas_concluidas(&s->atual);
    if (!snapshot(s)) /* historico[0] = tick 0 (requisito 1.5.2) */
    {
        sim_liberar(s);
        diag_erro(d, "falta de memoria ao iniciar a simulacao");
        return 0;
    }
    return 1;
}

/* TRATADOR DO TICK — a "interrupção" do relógio: VERIFICA sempre (executar,
 * prazos e chegadas levantam s->precisa_escalonar) e AGE só se preciso.
 * RELÓGIO GLOBAL (req. 1.1): o tick++ daqui é o ÚNICO do código. Como o
 * timer de um computador, o tempo anda e "interrompe" o núcleo. */
static void tratar_tick(Simulacao *s)
{
    s->atual.tick++;      /* o tempo anda...                             */
    executar(s);         /* contabiliza o intervalo que acabou          */
    verificar_prazos(s);  /* marca quem estourou o prazo                 */
    ativar(s);            /* chegadas deste tick                         */

    /* A estrela de sorteio no Gantt só vale no tick em que a tarefa GANHOU
     * a CPU: apaga sempre; escalonador_ordenar a acende de novo se rodar. */
    for (int i = 0; i < s->atual.ntarefas; i++)
        s->atual.tarefas[i].sorteada = 0;

    if (s->precisa_escalonar || escalonador_muda_com_a_execucao(s->esc))
    {
        escalonar(s);
        s->precisa_escalonar = 0;  /* evento atendido */
    }
    else
        contar_cpus_desligadas(&s->atual);
}

/* AVANÇAR (req. 1.5, modo passo a passo) — executa UM tick do relógio.
 * Devolve 1 se executou, 0 se não havia o que fazer (já terminada ou no
 * limite de ticks). */
int sim_avancar(Simulacao *s)
{
    /* Simulação já encerrada: não há o que fazer. O 0 avisa quem chamou
     * (a página não muda nada; o laço de sim_executar_tudo para). */
    if (s->terminada)
        return 0;

    /* Rede de segurança contra simulação que nunca termina (ex.: tarefa
     * suspensa que ninguém retoma). Corta no limite e AVISA o usuário, em
     * vez de deixar o programa rodando sem fim. O limite vem do arquivo
     * (calcular_limite) e fica depois do fim natural: uma simulação normal
     * nunca chega nele. Vale igual para os dois modos (passo a passo e
     * sim_executar_tudo), que passam por aqui. */
    if (s->atual.tick >= s->limite_ticks)
    {
        s->terminada = 1;
        snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
                 "simulacao interrompida no limite de %d ticks", s->limite_ticks);
        return 0;
    }

    /* Um tick: o relógio anda e o tratador do tick faz o resto (ordem
     * explicada no topo do arquivo). */
    tratar_tick(s);

    /* Fim natural (req. 4.4): todas as tarefas completaram as 10 execuções.
     * São duas mensagens diferentes de propósito: "concluída" aqui e
     * "interrompida" no limite, para o usuário saber qual dos dois houve. */
    s->terminada = todas_concluidas(&s->atual);
    if (s->terminada)
        snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
                 "simulacao concluida no tick %d", s->atual.tick);

    /* Guarda a foto deste tick (req. 1.5.2). Sem memória para ela, a
     * simulação para aqui, com aviso, em vez de derrubar o programa. */
    if (!snapshot(s))
    {
        s->terminada = 1;
        snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
                 "memoria insuficiente: simulacao interrompida no tick %d", s->atual.tick);
    }
    return 1;
}

/* RETROCEDER (req. 1.5.2) — volta a simulação um tick.
 *
 * O histórico guarda uma FOTO do estado inteiro por tick, e a última foto é
 * sempre a do tick atual:
 *     historico: [tick 0] [tick 1] [tick 2] [tick 3]    n_hist = 4
 * Retroceder = jogar fora a última foto e restaurar a anterior. O relógio
 * volta junto, porque o número do tick faz parte do estado copiado.
 *
 * Devolve 1 se voltou, 0 se já estava no tick 0. */
int sim_retroceder(Simulacao *s)
{
    /* Com uma foto só (a do tick 0) não há para onde voltar. É isto que
     * impede a simulação de ir para antes do início. */
    if (s->n_hist < 2)
        return 0;

    /* Descarta a foto do tick atual. Se o usuário avançar de novo, ela é
     * recalculada; assim nunca sobra no histórico um "futuro" que não vale
     * mais (ex.: depois de editar uma tarefa). */
    estado_liberar(&s->historico[s->n_hist - 1]);
    s->n_hist--;

    /* Libera o estado atual antes de trocá-lo: cada Estado tem vetores
     * alocados (tarefas e CPUs), e sem isto a memória vazaria a cada clique. */
    estado_liberar(&s->atual);

    /* CLONAR, e não só atribuir: a atribuição copiaria os ponteiros, e o
     * estado atual passaria a dividir os vetores com a foto guardada. O
     * próximo avanço estragaria o histórico. */
    s->atual = estado_clonar(&s->historico[s->n_hist - 1]);

    /* A marca de evento não faz parte da foto (é controle do núcleo, não
     * estado do sistema). Por segurança o primeiro tick depois de voltar
     * no tempo reescalona. */
    s->precisa_escalonar = 1;

    /* O algoritmo vem da foto: retroceder para antes de uma troca de
     * algoritmo desfaz a troca (o ponteiro é só um atalho para o nome). */
    s->esc = escalonador_buscar(s->atual.algoritmo);

    /* Voltou um tick: mesmo que a simulação tivesse acabado, não acabou mais. */
    s->terminada = 0;
    snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
             "retrocedeu para o tick %d", s->atual.tick);
    return 1;
}

/* EXECUÇÃO COMPLETA (req. 1.5, modo b) — roda até o fim, sem parar a cada
 * passo. É só repetir o passo a passo: cada volta do laço é um sim_avancar.
 *
 * Os ticks intermediários continuam indo para o histórico, mesmo sem
 * aparecer na tela (req. 1.5.3): o Gantt final é desenhado a partir dele, e
 * o usuário pode retroceder depois de executar tudo. */
void sim_executar_tudo(Simulacao *s)
{
    /* A condição do laço é o RETORNO de sim_avancar: ela devolve 0 quando
     * não executou nenhum tick. Assim o laço nunca gira parado, o que
     * poderia acontecer com "while (!s->terminada)". O limite de ticks
     * (e o aviso de que a simulação foi CORTADA) fica no próprio sim_avancar. */
    while (sim_avancar(s))
        ;
}

/* CÓPIA ATÉ O FIM (req. 2.4) — a imagem exportada tem de ser a simulação
 * INTEIRA, mesmo que o usuário exporte no meio. Em vez de avançar a
 * simulação da tela, copia e avança a cópia.
 *
 * A cópia reaproveita as fotos que já existem (só são lidas, não precisam
 * ser duplicadas) e só calcula os ticks que faltam. */
int sim_copiar_ate_o_fim(const Simulacao *s, Simulacao *copia)
{
    *copia = *s; /* escalonador, limite, terminada... */

    /* Vetor de fotos PRÓPRIO: ao avançar, o snapshot da cópia faz realloc
     * nele, e um realloc no vetor de 's' poderia mudá-lo de lugar e
     * estragar a simulação da tela. As fotos dentro dele são as mesmas. */
    copia->historico = malloc(s->n_hist * sizeof(Estado));
    if (copia->historico == NULL)
        return 0;
    memcpy(copia->historico, s->historico, s->n_hist * sizeof(Estado));
    copia->cap_hist = s->n_hist;

    /* O estado atual muda a cada tick: este sim precisa ser uma cópia de
     * verdade (clonar, como no retroceder). */
    if (!clonar(&copia->atual, &s->atual))
    {
        free(copia->historico);
        return 0;
    }

    sim_executar_tudo(copia);
    return 1;
}
/* Libera SÓ o que a cópia criou: as fotos do n_hist de 's' em diante e o
 * estado atual dela. As fotos anteriores são de 's' e continuam em uso:
 * por isso NÃO se usa sim_liberar aqui. */
void sim_liberar_copia(const Simulacao *s, Simulacao *copia)
{
    for (int i = s->n_hist; i < copia->n_hist; i++)
        estado_liberar(&copia->historico[i]);
    free(copia->historico);
    estado_liberar(&copia->atual);
}
/* [6.6] Edição do |periodo| da esporádica: o intervalo mínimo novo vale para
 * as ocorrências que AINDA VÃO chegar, cada uma a >= 'minimo' da anterior.
 * A primeira delas é comparada com a última chegada que já aconteceu (as
 * passadas não se desfazem): com pendentes, a mais nova é da lista; sem
 * pendentes, é 'ativacao' (inclui a chegada manual). */
static int intervalo_novo_ok(const TCB *t, int minimo, char *motivo, int tam)
{
    int tem_anterior = t->prox_evento > 0;
    int anterior = 0;

    if (tem_anterior)
        anterior = t->pendentes > 0 ? t->instantes[t->prox_evento - 1] : t->ativacao;
    for (int k = t->prox_evento; k < t->n_instantes; k++)
    {
        if (tem_anterior && t->instantes[k] - anterior < minimo)
        {
            snprintf(motivo, tam,
                     "Com intervalo minimo %d, as chegadas dos ticks %d e %d ficariam "
                     "a %d ticks.",
                     minimo, anterior, t->instantes[k], t->instantes[k] - anterior);
            return 0;
        }
        anterior = t->instantes[k];
        tem_anterior = 1;
    }
    return 1;
}

static int aplicar_edicao(Simulacao *s, const char *id_tarefa, const char *campo,
                          const char *valor, char *motivo, int motivo_tam)
{
    int indice = sim_indice_por_id(&s->atual, id_tarefa);
    TCB *tarefa;
    int ativa;
    int numero; /* o valor dos campos numéricos, já convertido */

    if (indice == -1)
    {
        snprintf(motivo, motivo_tam, "O id '%s' nao esta no registro", id_tarefa);
        return 0;
    }
    tarefa = &s->atual.tarefas[indice];
    /* "Ativa" = tem uma execução em andamento (chegou e ainda não acabou). */
    ativa = tarefa->estado == EST_PRONTA || tarefa->estado == EST_EXECUTANDO ||
            tarefa->estado == EST_SUSPENSA;

    /* ---- campo ESTADO ---- */
    if (strcmp(campo, "estado") == 0)
    {
        int novo_estado = -1;

        /* Procura o nome entre os do enum, com o mesmo estado_nome que a
         * página mostra. str_igual_ci: "pronta", "PRONTA" e "Pronta" valem
         * igual (a mesma função que o leitor do arquivo usa para "RM"/"rm"). */
        for (int e = EST_INATIVA; e <= EST_CONCLUIDA; e++)
            if (str_igual_ci(valor, estado_nome((EstadoTarefa)e)))
                novo_estado = e;
        if (novo_estado == -1)
        {
            snprintf(motivo, motivo_tam, "O estado '%s' nao e valido.", valor);
            return 0;
        }
        if (novo_estado == EST_EXECUTANDO)
        {
            /* Quem põe na CPU é o escalonador: forçar aqui deixaria uma
             * tarefa "executando" sem CPU, ou furando a fila de prioridade. */
            snprintf(motivo, motivo_tam,
                     "Nao e possivel forcar EXECUTANDO; mude para PRONTA e o "
                     "escalonador decide.");
            return 0;
        }

        /* As recusas vêm ANTES de mexer em qualquer coisa: edição recusada
         * não pode deixar nada pela metade. */
        if (tarefa->estado == EST_CONCLUIDA)
        {
            /* Reabrir faria a conta passar das 10 execuções (req. 4.4). Não
             * diz "concluiu as 10": ela pode ter sido concluída à mão. */
            snprintf(motivo, motivo_tam,
                     "A tarefa %s ja esta concluida e nao pode ser reaberta.", id_tarefa);
            return 0;
        }
        if (tarefa->estado == EST_INATIVA && novo_estado == EST_SUSPENSA)
        {
            snprintf(motivo, motivo_tam,
                     "A tarefa %s nao esta ativa; nao ha o que suspender.", id_tarefa);
            return 0;
        }
        /* [6.6] Chegada manual da esporádica também respeita o intervalo
         * mínimo, contado da última chegada REAL ('ativacao', que inclui as
         * manuais). Basta olhar a anterior: a ocorrência seguinte da lista
         * já estava a >= |periodo| de uma chegada que ainda nem tinha
         * acontecido, então fica mais longe ainda. */
        if (tarefa->tipo == TIPO_ESPORADICA && tarefa->estado == EST_INATIVA &&
            novo_estado == EST_PRONTA && tarefa->prox_evento > 0 &&
            s->atual.tick - tarefa->ativacao < -tarefa->periodo)
        {
            snprintf(motivo, motivo_tam,
                     "A tarefa %s e esporadica: a ultima chegada foi no tick %d e o "
                     "intervalo minimo e %d (proxima chegada manual a partir do tick %d).",
                     id_tarefa, tarefa->ativacao, -tarefa->periodo,
                     tarefa->ativacao - tarefa->periodo);
            return 0;
        }

        /* Saindo da CPU: solta dos DOIS lados (a CPU guarda a tarefa e a
         * tarefa guarda a CPU), senão a CPU ficaria ocupada por ninguém. */
        if (tarefa->estado == EST_EXECUTANDO)
            soltar_cpu(&s->atual, tarefa);

        /* INATIVA -> PRONTA é uma chegada antecipada: precisa de tempo
         * restante, prazo novo etc. Só trocar o estado faria a tarefa rodar
         * com 0 de trabalho. SUSPENSA -> PRONTA é retomar: mantém o que
         * faltava.
         * [6.6] Aperiódica e esporádica CONSOMEM a chegada que viria
         * (agendar_proxima): senão ativar() a traria de novo no tick dela e
         * a aperiódica chegaria duas vezes. A periódica fica como no A. */
        if (tarefa->estado == EST_INATIVA && novo_estado == EST_PRONTA)
        {
            if (tarefa->tipo != TIPO_PERIODICA)
                agendar_proxima(tarefa);
            ativar_tarefa(tarefa, s->atual.tick);
        }
        else
            tarefa->estado = novo_estado;
        return 1;

    }

    /* ---- campos numéricos: exec_restante, duracao, periodo e prazo ---- */
    if (strcmp(campo, "exec_restante") != 0 && strcmp(campo, "duracao") != 0 &&
        strcmp(campo, "periodo") != 0 && strcmp(campo, "prazo") != 0)
    {
        snprintf(motivo, motivo_tam,
                 "Campo '%s' nao existe ou nao permite edicao manual.", campo);
        return 0;
    }
    /* ler_inteiro e não atoi: o atoi transforma "abc" em 0 sem avisar. */
    if (!ler_inteiro(valor, &numero))
    {
        snprintf(motivo, motivo_tam, "'%s' nao e um numero inteiro valido.", valor);
        return 0;
    }

    /* ---- campo EXEC_RESTANTE (tempo que falta da execução atual) ---- */
    if (strcmp(campo, "exec_restante") == 0)
    {
        /* Limite: a duração da execução EM ANDAMENTO. Se a duração foi
         * editada (vale só para a próxima chegada), a execução atual pode
         * ter mais tempo restante do que a duração nova; até esse valor
         * também é aceito. */
        int maximo = tarefa->duracao > tarefa->exec_restante ? tarefa->duracao
                                                             : tarefa->exec_restante;
        /* Só existe "tempo que falta" durante uma execução. Numa tarefa
         * INATIVA ou CONCLUIDA o valor seria apagado na próxima chegada. */
        if (!ativa)
        {
            snprintf(motivo, motivo_tam,
                     "A tarefa %s nao esta ativa; nao ha execucao em andamento.", id_tarefa);
            return 0;
        }
        /* Mínimo 1, não 0: uma tarefa ativa com 0 restante ainda rodaria
         * mais um tick e ficaria com -1. Para encerrar a execução, o certo
         * é editar o estado. */
        if (numero < 1 || numero > maximo)
        {
            snprintf(motivo, motivo_tam,
                     "O valor %d nao cumpre os pre requisitos (deve ser entre 1 e %d).",
                     numero, maximo);
            return 0;
        }
        tarefa->exec_restante = numero;
        return 1;
    }
    /* ---- campo DURACAO ---- */
    if (strcmp(campo, "duracao") == 0)
    {
        if (numero < 1)
        {
            /* Mesma regra do arquivo de configuração: duração >= 1. */
            snprintf(motivo, motivo_tam,
                     "O valor %d nao cumpre os pre requisitos (deve ser >= 1).", numero);
            return 0;
        }
        /* Vale a partir da PRÓXIMA chegada (ativar_tarefa lê este campo). A
         * execução em andamento continua com o tempo restante que tinha. */
        tarefa->duracao = numero;
        return 1;
    }
    /* ---- campo PERIODO ---- */
    if (strcmp(campo, "periodo") == 0)
    {
        /* [6.6] O VALOR do período muda (req. 1.5.2: "características de
         * qualquer tarefa"), mas o SINAL não: ele é o tipo da tarefa (req. B
         * 1.2 e 1.3), e a execução em andamento ficaria sem regra de chegada
         * (req. 3.1: recusa justificada). INT_MIN fica de fora: não tem
         * |valor| num int. */
        TipoTarefa novo_tipo = numero > 0 ? TIPO_PERIODICA
                             : numero == 0 ? TIPO_APERIODICA
                             : TIPO_ESPORADICA;
        if (novo_tipo != tarefa->tipo || numero == INT_MIN)
        {
            snprintf(motivo, motivo_tam,
                     "A tarefa %s e %s; o periodo %d a tornaria %s. O tipo da tarefa "
                     "nao muda durante a simulacao.",
                     id_tarefa, tipo_nome(tarefa->tipo), numero, tipo_nome(novo_tipo));
            return 0;
        }
        if (tarefa->tipo == TIPO_APERIODICA) /* 0 -> 0: nada muda */
            return 1;
        if (tarefa->tipo == TIPO_ESPORADICA)
        {
            if (!intervalo_novo_ok(tarefa, -numero, motivo, motivo_tam))
                return 0;
            /* O prazo acompanha se era o padrão (Di = |P|), como na
             * periódica logo abaixo. */
            if (tarefa->prazo == -tarefa->periodo)
                tarefa->prazo = -numero;
            tarefa->periodo = numero;
            return 1;
        }
        /* REGRA: o período novo conta a partir da ÚLTIMA chegada.
         *   última chegada = proxima_ativacao - período antigo
         *   nova próxima   = última chegada + período novo
         * Se a tarefa ainda não chegou nenhuma vez (proxima_ativacao ainda é
         * o ingresso), a 1ª chegada continua no ingresso e só o intervalo
         * entre as seguintes muda. */
        if (tarefa->proxima_ativacao > tarefa->ingresso)
        {
            int ultima = tarefa->proxima_ativacao - tarefa->periodo;
            int nova = ultima + numero;
            /* Período encurtado a ponto de a próxima chegada já ter
             * passado: ela acontece no tick seguinte, nunca no passado
             * (a ativar() só dispara quando tick == proxima_ativacao). */
            if (nova <= s->atual.tick)
                nova = s->atual.tick + 1;
            tarefa->proxima_ativacao = nova;
        }
        /* O PRAZO ACOMPANHA, mas só se era igual ao período (Di = Pi, a
         * premissa do RM e o padrão do arquivo quando o prazo vem vazio).
         * Sem isso, encurtar o período de 10 para 6 deixaria Di = 10 > Pi
         * sem o usuário ter pedido. Se o prazo era DIFERENTE do período
         * (Di < Pi, caso do DM e dos testes 16), foi uma escolha explícita
         * e fica como está.
         * O prazo novo vale a partir da próxima chegada: deadline_abs não é
         * tocado, então a execução em andamento mantém o deadline que tinha
         * (mesma regra da edição da duração). */
        if (tarefa->prazo == tarefa->periodo)
            tarefa->prazo = numero;
        /* No RM a prioridade muda na hora: o critério lê este campo. */
        tarefa->periodo = numero;
        return 1;
    }
    /* ---- campo PRAZO (o único que sobrou) ---- */
    if (numero < 0)
    {
        snprintf(motivo, motivo_tam,
                 "O valor %d nao cumpre os pre requisitos (deve ser >= 0).",
                 numero);
        return 0;
    }
    tarefa->prazo = numero;
    /* O prazo conta a partir da chegada desta execução. */
    tarefa->deadline_abs = tarefa->ativacao + numero;
    /* A marca de prazo perdido acompanha o prazo novo: aumentar o prazo
     * pode tirar a marca, diminuir pode pôr. Só vale durante uma
     * execução; numa tarefa INATIVA o prazo novo passa a valer na
     * próxima chegada (ativar_tarefa), e a marca não muda. */
    if (ativa)
        tarefa->perdeu_prazo = s->atual.tick >= tarefa->deadline_abs;
    return 1;
}

/* EDITAR (req. 1.5.2 e 3.4) — muda o estado ou um campo de uma tarefa em
 * qualquer passo da simulação. Devolve 1 se aceitou; 0 se recusou, com o
 * porquê em 'motivo' (é o texto que a página mostra ao usuário).
 *
 * Dividida em duas: aplicar_edicao (acima) valida e aplica; esta faz o que
 * vale para TODA edição aceita, sem repetir em cada campo. */
int sim_editar_tarefa(Simulacao *s, const char *id_tarefa, const char *campo,
                      const char *valor, char *motivo, int motivo_tam)
{
    if (!aplicar_edicao(s, id_tarefa, campo, valor, motivo, motivo_tam))
        return 0;   /* recusada: nada mudou, nada a atualizar */

    s->precisa_escalonar = 1; /* EVENTO: vale a partir do próximo tick */

    /* A edição pode ter concluído a simulação (ex.: a última tarefa
     * pendente marcada como CONCLUIDA). */
    s->terminada = todas_concluidas(&s->atual);

    /* Troca a foto do tick atual pela versão editada: o Gantt é desenhado
     * a partir do histórico, e sem isto a edição não apareceria nele.
     * Libera a foto velha e guarda uma CÓPIA da nova, como em retroceder. */
    refazer_foto(s);

    /* O limite de ticks foi calculado em sim_iniciar com os períodos e as
     * durações do arquivo. Uma edição que os aumenta empurra o fim natural
     * para mais longe, e a simulação bateria no limite antigo antes de
     * terminar. Só cresce: nunca corta o que já rodou. */
    {
        int novo = calcular_limite(&s->atual);
        if (novo > s->limite_ticks)
            s->limite_ticks = novo;
    }
    return 1;
}

/* ALTERAR A QUANTIDADE DE CPUs com a simulação carregada (req. geral 2 e
 * 3.1). A ordem dos passos importa:
 *   1. recusas, antes de mexer em qualquer coisa;
 *   2. ao REDUZIR: as tarefas das CPUs que vão sair voltam para PRONTA --
 *      antes do realloc, senão ficariam apontando para CPU que não existe;
 *   3. realloc do vetor de CPUs (ponteiro temporário: se falhar, o vetor
 *      antigo continua valendo);
 *   4. ao AUMENTAR: as CPUs novas nascem vazias;
 *   5. reescalonar, para não sobrar tarefa pronta com CPU livre (req. 1.2);
 *   6. trocar a foto do tick atual, como na edição de tarefa.
 * Retroceder desfaz sozinho: cada foto do histórico tem o seu próprio
 * ncpus e o seu próprio vetor de CPUs. */
int sim_alterar_cpus(Simulacao *s, int n, char *motivo, int motivo_tam)
{
    Estado *e = &s->atual;
    CPU *maior;
    int antigas = e->ncpus;

    if (s->esc == NULL) /* esc só existe depois de sim_iniciar dar certo */
    {
        snprintf(motivo, motivo_tam, "Nenhuma simulacao carregada.");
        return 0;
    }
    if (s->terminada)
    {
        snprintf(motivo, motivo_tam,
                 "A simulacao ja terminou; retroceda para alterar as CPUs.");
        return 0;
    }
    if (n < 1 || n > MAX_CPUS)
    {
        snprintf(motivo, motivo_tam,
                 "A quantidade de CPUs deve ficar entre 1 e %d (veio %d).", MAX_CPUS, n);
        return 0;
    }
    if (n == antigas)
    {
        snprintf(motivo, motivo_tam, "O sistema ja tem %d CPU(s).", n);
        return 0;
    }

    /* 2) Reduzindo: quem ocupa uma CPU que vai sair volta para a fila. */
    for (int c = n; c < antigas; c++)
    {
        if (e->cpus[c].tarefa >= 0)
            devolver_a_fila(e, &e->tarefas[e->cpus[c].tarefa]);
    }

    /* 3) O vetor muda de tamanho. */
    maior = realloc(e->cpus, (size_t)n * sizeof(CPU));
    if (maior == NULL)
    {
        snprintf(motivo, motivo_tam, "Falta de memoria para alterar as CPUs.");
        return 0;
    }
    e->cpus = maior;

    /* 4) Aumentando: as novas começam vazias e sem tempo desligada. */
    for (int c = antigas; c < n; c++)
    {
        e->cpus[c].id = c;
        e->cpus[c].tarefa = -1;
        e->cpus[c].ticks_desligada = 0;
    }
    e->ncpus = n;

    /* 5) Reescalonar. A escalonar() soma 1 em ticks_desligada de toda CPU
     * vazia, e as CPUs antigas que estão vazias já foram contadas neste
     * tick: desconta antes, para o tick não valer por dois. */
    for (int c = 0; c < antigas && c < n; c++)
    {
        if (e->cpus[c].tarefa == -1)
            e->cpus[c].ticks_desligada--;
    }
    escalonar(s);

    /* 6) A foto do tick atual passa a ser a nova (Gantt e retroceder). */
    refazer_foto(s);
    snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
             "CPUs alteradas de %d para %d no tick %d", antigas, n, e->tick);
    return 1;
}

/* LIBERAR — devolve ao sistema toda a memória da simulação e deixa 's'
 * ZERADA, que é o mesmo que "nenhum arquivo carregado".
 *
 * É segura em uma Simulacao zerada (não faz nada) e pode ser chamada duas
 * vezes seguidas. Por isso sim_iniciar a chama sempre, sem perguntar se
 * havia simulação antes. */
void sim_liberar(Simulacao *s)
{
    /* Cada foto do histórico tem os SEUS vetores de tarefas e de CPUs: é
     * preciso liberar uma por uma antes de liberar o vetor de fotos. Com
     * ~190 tarefas são uns 60 KB por foto: milhares de ticks dão centenas
     * de MB, que voltam aqui. */
    for (int i = 0; i < s->n_hist; i++)
    {
        estado_liberar(&s->historico[i]);
    }
    free(s->historico); /* free(NULL) é permitido: não faz nada */
    estado_liberar(&s->atual);

    /* Zera o resto (escalonador, terminada, último evento). Sem isto 'esc'
     * continuaria apontando para um algoritmo, e a página acharia que ainda
     * existe uma simulação carregada. */
    memset(s, 0, sizeof *s);
}

/* TROCAR O ALGORITMO no meio da simulação (req. 3.1). Devolve 1 se trocou;
 * 0 se recusou, com o porquê em 'motivo' (o texto que a página mostra).
 *
 * Trocar é só apontar s->esc para outro escalonador: o núcleo chama sempre
 * a mesma função (escalonador_ordenar) e não sabe qual algoritmo está por
 * trás. É o que o req. 4.2 pede: algoritmo plugável.
 *
 * Vale a partir do PRÓXIMO tick: as CPUs do tick atual já foram
 * distribuídas pelo algoritmo antigo. A troca é um EVENTO.
 *
 * Quem manda é o NOME guardado no estado (que vai para cada foto); o
 * ponteiro s->esc é só um atalho para ele. Por isso retroceder para antes
 * da troca desfaz a troca sozinho (ver sim_retroceder). */
int sim_trocar_algoritmo(Simulacao *s, const char *nome,
                         char *motivo, int motivo_tam)
{
    const Escalonador *esc;

    /* As recusas vêm antes de mexer em qualquer coisa: recusada, a
     * simulação fica exatamente como estava. */
    if (s->esc == NULL) /* esc só existe depois de sim_iniciar dar certo */
    {
        snprintf(motivo, motivo_tam, "Nenhuma simulacao carregada.");
        return 0;
    }
    /* Terminada, não há "próximo tick" para o algoritmo novo valer. O
     * req. 3.1 pede o motivo E o que fazer. */
    if (s->terminada)
    {
        snprintf(motivo, motivo_tam,
                 "A simulacao ja terminou; retroceda para trocar o algoritmo.");
        return 0;
    }
    /* escalonador_buscar ignora maiúsculas ("edf" acha o EDF), como o
     * arquivo de configuração (req. 3.3.2). */
    esc = escalonador_buscar(nome);
    if (esc == NULL)
    {
        /* A lista vem dos algoritmos registrados, não de um texto fixo: um
         * algoritmo novo entra na mensagem sozinho (req. 4.2). */
        char lista[128];
        listar_algoritmos(lista, sizeof lista);
        snprintf(motivo, motivo_tam, "O algoritmo '%s' nao existe (disponiveis: %s). "
                 "Adicione-o em \"novo algoritmo\".", nome, lista);
        return 0;
    }

    s->esc = esc;
    /* Nome oficial ("edf" vira "EDF"), no estado e na foto do tick atual:
     * a foto é o que o Gantt e o retroceder leem. Basta copiar o nome, e
     * não clonar a foto inteira, porque ele é a única coisa que mudou. */
    snprintf(s->atual.algoritmo, sizeof s->atual.algoritmo, "%s", esc->nome);
    strcpy(s->historico[s->n_hist - 1].algoritmo, s->atual.algoritmo);
    snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
             "algoritmo trocado para %s no tick %d", esc->nome, s->atual.tick);

    /* TROCA NO TICK 0 = escolher o algoritmo ANTES de a simulação começar
     * (req. 3.2 e 4.1: o do arquivo é só o padrão). Nada executou ainda,
     * mas sim_iniciar já distribuiu as CPUs com o algoritmo do arquivo; sem
     * refazer isso, o primeiro intervalo sairia com o algoritmo antigo.
     * Então a distribuição é desfeita e refeita com o algoritmo novo:
     *   1. quem estava EXECUTANDO volta para PRONTA e solta a CPU;
     *   2. o contador de CPU desligada volta a 0 (a escalonar() vai contar
     *      de novo, e sem zerar o tick 0 seria contado duas vezes);
     *   3. escalonar() escolhe de novo;
     *   4. a foto do tick 0 é trocada, como na edição manual (o Gantt e o
     *      retroceder leem o histórico).
     * Depois do tick 0 NÃO se refaz nada: o intervalo em curso já foi
     * decidido, e a troca vale a partir do próximo tick (req. 3.1). */
    if (s->atual.tick == 0)
    {
        Estado *e = &s->atual;
        for (int i = 0; i < e->ntarefas; i++)
        {
            if (e->tarefas[i].estado == EST_EXECUTANDO)
                devolver_a_fila(e, &e->tarefas[i]);
        }
        for (int c = 0; c < e->ncpus; c++)
            e->cpus[c].ticks_desligada = 0;
        escalonar(s);

        refazer_foto(s); /* no tick 0 a última foto é a única: historico[0] */
        snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
                 "algoritmo trocado para %s antes do inicio: vale desde o tick 0",
                 esc->nome);
    }
    s->precisa_escalonar = 1; /* EVENTO: o algoritmo novo vale no próximo tick */
    return 1;
}
