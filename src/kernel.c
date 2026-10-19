/* ============================================================================
 * kernel.c — O coração da simulação
 * ----------------------------------------------------------------------------
 * ORDEM DOS ACONTECIMENTOS EM UM TICK
 *
 * O estado guardado em `s->atual` está SEMPRE "pronto para ser desenhado":
 * as ativações do instante já ocorreram e o escalonador já decidiu quem ocupa
 * cada CPU. Avançar um tick é, portanto:
 *
 *    executar()          consome 1 unidade de tempo das tarefas nas CPUs
 *    tick++              o relógio global anda (requisito 1.1)
 *    verificar_prazos()  quem passou do deadline é marcado (requisito 3.3)
 *    ativar()            tarefas periódicas que vencem agora viram PRONTAS
 *    escalonar()         escolhe quem ocupa cada CPU no próximo intervalo
 *    snapshot()          congela tudo em historico[tick] (requisito 1.5.2)
 *
 * Manter essa ordem fixa é o que torna a simulação DETERMINÍSTICA: rodar,
 * retroceder e avançar de novo produz exatamente o mesmo resultado.
 *
 * NOTA: estado_clonar/estado_liberar/estado_nome moram em tcb.c (é lá que
 * `tcb.h` os declara), não duplicar aqui.
 * ==========================================================================*/
#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Acha a POSIÇÃO de uma tarefa no vetor a partir do ID que o usuário vê.
 * São coisas diferentes: o ID vem do arquivo (pode ser 7, 42...), a posição
 * é 0, 1, 2... Devolve -1 se o ID não existe. Busca simples, uma por uma:
 * com ~190 tarefas e uma busca por clique, não vale nada mais elaborado. */
int sim_indice_por_id(const Estado *e, int id)
{
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (e->tarefas[i].id == id)
            return i;
    }
    return -1;
}

/* Devolve 1 se TODAS as tarefas estão EST_CONCLUIDA (com ntarefas==0 também:
 * nada pra simular). Usada por sim_iniciar/sim_avancar pra decidir
 * s->terminada. */
static int todas_concluidas(const Estado *e)
{
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (e->tarefas[i].estado != EST_CONCLUIDA)
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
    long long ultima_chegada = 0, trabalho = 0, limite, teto;

    for (int i = 0; i < e->ntarefas; i++)
    {
        const TCB *t = &e->tarefas[i];
        long long chegada = t->ingresso + (long long)(MAX_ATIVACOES - 1) * t->periodo;
        if (chegada > ultima_chegada)
            ultima_chegada = chegada;
        trabalho += (long long)MAX_ATIVACOES * t->duracao;
    }
    limite = 2 * (ultima_chegada + trabalho);
    if (limite < MAX_TICKS)
        limite = MAX_TICKS;

    /* Bytes de UMA foto: a struct Estado mais os vetores de tarefas e CPUs. */
    teto = MAX_MEMORIA_HIST / ((long long)sizeof(Estado) +
                               (long long)e->ntarefas * sizeof(TCB) +
                               (long long)e->ncpus * sizeof(CPU));
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

/* ATIVAÇÃO — uma tarefa periódica INATIVA vira PRONTA quando o tick atual
 * cai exatamente em ingresso + k*periodo, para algum k >= 0. */
static void ativar(Simulacao *s)
{
    Estado *executando = &s->atual;
    for (int i = 0; i < executando->ntarefas; i++)
    {
        TCB *tarefa = &executando->tarefas[i];
        int chegou = tarefa->periodo > 0 &&
                     executando->tick >= tarefa->ingresso &&
                     (executando->tick - tarefa->ingresso) % tarefa->periodo == 0;

        if (!chegou)
            continue;
        if (tarefa->estado == EST_INATIVA)
            ativar_tarefa(tarefa, executando->tick); /* livre: começa já */
        else if (tarefa->estado != EST_CONCLUIDA)
            tarefa->pendentes++; /* ocupada: espera a vez */
    }
}

/* ESCALONAMENTO — requisito 1.2.
 * A fila de prontos é GLOBAL (um único vetor de candidatas para todas as
 * CPUs, não uma fila por processador) e a ociosidade é minimizada: só sobra
 * CPU desligada se realmente não houver tarefa pronta.
 *
 * Reavalia do zero a cada tick quem deve ocupar cada CPU (é isso que torna o
 * escalonamento PREEMPTIVO): uma tarefa só continua na mesma CPU se ainda
 * estiver entre as mais prioritárias segundo escalonador_ordenar (que já
 * aplica os critérios de desempate do requisito 4.3, dando vantagem a quem
 * já estava executando só como critério de desempate, não como prioridade
 * absoluta). */
static void escalonar(Simulacao *s)
{
    /* Os seis passos estão numerados abaixo. A ideia central: a cada tick
     * TODAS as prontas e as que estão executando disputam as CPUs de novo.
     * Por isso uma tarefa mais prioritária que chega toma a CPU de quem
     * está rodando (preempção). Quem continua rodando só ganha vantagem no
     * desempate (critério 1 do req. 4.3), não passe livre. */
    Estado *e = &s->atual;

    /* 1) Montar a lista de candidatos */
    int candidatos[e->ntarefas];
    int n_candidatos = 0;
    
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (e->tarefas[i].estado == EST_PRONTA || e->tarefas[i].estado == EST_EXECUTANDO)
        {
            candidatos[n_candidatos] = i;
            n_candidatos++;
        }
    }

    /* 2) Ordenar a lista usando o escalonador atual */
    if (n_candidatos > 0 && s->esc != NULL) 
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
        {
            // Libera a CPU e volta a tarefa para a fila
            soltar_cpu(e, t);
            t->estado = EST_PRONTA;
        }
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
    for (int c = 0; c < e->ncpus; c++)
    {
        if (e->cpus[c].tarefa == -1)
        {
            e->cpus[c].ticks_desligada++;
        }
    }
}

/* EXECUÇÃO — consome uma unidade de tempo de cada tarefa que está na CPU,
 * e trata tanto o término da ativação quanto o estouro do quantum
 * (requisito 3.3: "quantum indica o período máximo de tempo que uma tarefa
 * pode executar" antes de precisar ceder a CPU). */
static void executar(Simulacao *s)
{
    /* Para cada tarefa na CPU: gasta 1 unidade de trabalho e 1 de quantum,
     * e então decide se ela (a) terminou esta execução, (b) esgotou o
     * quantum e devolve a CPU, ou (c) segue executando. Quem decide QUEM
     * entra nas CPUs livres é escalonar(), logo depois, no mesmo tick. */
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
                 if (tarefa->ativacoes >= MAX_ATIVACOES)
                 {
                     tarefa->estado = EST_CONCLUIDA;
                     tarefa->pendentes = 0; /* fez as 10: acabou */
                 }
                 else if (tarefa->pendentes > 0)
                 {
                     /* Já havia outra execução esperando: ela chegou um período depois
                      * da que acabou de terminar, e o prazo dela conta DE LÁ. */
                     tarefa->pendentes--;
                     ativar_tarefa(tarefa, tarefa->ativacao + tarefa->periodo);
                 }
                 else
                 {
                     tarefa->estado = EST_INATIVA;
                 }
             }
             /* (b) Quantum esgotado com trabalho por fazer: devolve a CPU e
              * volta para a fila. quantum 0 no arquivo = sem limite, por
              * isso o "quantum > 0". */
             else if (executando->quantum > 0 && tarefa->quantum_restante <= 0)
             {
                 soltar_cpu(executando, tarefa);
                 tarefa->estado = EST_PRONTA;
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
        /* Erro claro (requisito 5): diz o que veio e o que é aceito. */
        char lista[128];
        listar_algoritmos(lista, sizeof lista);
        diag_erro(d, "algoritmo \"%s\" nao existe (disponiveis: %s)",
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
    escalonar(s); /* primeira atribuição de CPUs */
    s->terminada = todas_concluidas(&s->atual);
    if (!snapshot(s)) /* historico[0] = tick 0 (requisito 1.5.2) */
    {
        sim_liberar(s);
        diag_erro(d, "falta de memoria ao iniciar a simulacao");
        return 0;
    }
    return 1;
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

    /* Um tick, sempre nesta ordem (explicada no topo do arquivo). A ordem
     * fixa é o que faz retroceder e avançar de novo dar o mesmo resultado. */
    executar(s);         /* quem está nas CPUs trabalha 1 unidade       */
    s->atual.tick++;     /* o relógio global anda (req. 1.1)            */
    verificar_prazos(s); /* marca quem estourou o prazo                 */
    ativar(s);           /* tarefas periódicas que vencem agora chegam  */
    escalonar(s);        /* decide quem ocupa cada CPU a partir daqui   */

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
static int aplicar_edicao(Simulacao *s, int id_tarefa, const char *campo,
                          const char *valor, char *motivo, int motivo_tam)
{
    int indice = sim_indice_por_id(&s->atual, id_tarefa);
    TCB *tarefa;
    int ativa;
    int numero; /* o valor dos campos numéricos, já convertido */

    if (indice == -1)
    {
        snprintf(motivo, motivo_tam, "O id %d nao esta no registro", id_tarefa);
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
                     "A tarefa %d ja esta concluida e nao pode ser reaberta.", id_tarefa);
            return 0;
        }
        if (tarefa->estado == EST_INATIVA && novo_estado == EST_SUSPENSA)
        {
            snprintf(motivo, motivo_tam,
                     "A tarefa %d nao esta ativa; nao ha o que suspender.", id_tarefa);
            return 0;
        }

        /* Saindo da CPU: solta dos DOIS lados (a CPU guarda a tarefa e a
         * tarefa guarda a CPU), senão a CPU ficaria ocupada por ninguém. */
        if (tarefa->estado == EST_EXECUTANDO)
        {
            if (tarefa->cpu >= 0 && tarefa->cpu < s->atual.ncpus)
                s->atual.cpus[tarefa->cpu].tarefa = -1;
            tarefa->cpu = -1;
        }

        /* INATIVA -> PRONTA é uma chegada antecipada: precisa de tempo
         * restante, prazo novo etc. Só trocar o estado faria a tarefa rodar
         * com 0 de trabalho. SUSPENSA -> PRONTA é retomar: mantém o que
         * faltava. */
        if (tarefa->estado == EST_INATIVA && novo_estado == EST_PRONTA)
            ativar_tarefa(tarefa, s->atual.tick);
        else
            tarefa->estado = novo_estado;
        return 1;
    }

    /* ---- campos numéricos: exec_restante, duracao e prazo ---- */
    if (strcmp(campo, "exec_restante") != 0 && strcmp(campo, "duracao") != 0 &&
        strcmp(campo, "prazo") != 0)
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
                     "A tarefa %d nao esta ativa; nao ha execucao em andamento.", id_tarefa);
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
int sim_editar_tarefa(Simulacao *s, int id_tarefa, const char *campo,
                      const char *valor, char *motivo, int motivo_tam)
{
    if (!aplicar_edicao(s, id_tarefa, campo, valor, motivo, motivo_tam))
        return 0;   /* recusada: nada mudou, nada a atualizar */

    /* A edição pode ter concluído a simulação (ex.: a última tarefa
     * pendente marcada como CONCLUIDA). */
    s->terminada = todas_concluidas(&s->atual);

    /* Troca a foto do tick atual pela versão editada: o Gantt é desenhado
     * a partir do histórico, e sem isto a edição não apareceria nele.
     * Libera a foto velha e guarda uma CÓPIA da nova, como em retroceder. */
    estado_liberar(&s->historico[s->n_hist - 1]);
    s->historico[s->n_hist - 1] = estado_clonar(&s->atual);
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
    s->historico = NULL;
    s->n_hist = 0;
    s->cap_hist = 0;
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
 * distribuídas pelo algoritmo antigo. Redistribuir agora exigiria chamar
 * escalonar() de novo, que contaria a ociosidade das CPUs duas vezes.
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
        snprintf(motivo, motivo_tam, "O algoritmo '%s' nao existe (disponiveis: %s).",
                 nome, lista);
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
    return 1;
}
