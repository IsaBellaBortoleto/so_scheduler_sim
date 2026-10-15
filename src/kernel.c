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

#include <errno.h>
#include <limits.h>
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

/* ------------------------------- histórico ------------------------------- */

/* Guarda a fotografia do tick corrente em historico[n_hist].
 *
 * guardamos o estado INTEIRO a cada tick, não um diário de
 * diferenças. Custo = ntarefas * ticks * sizeof(TCB) — para 20 tarefas e
 * 5000 ticks dá poucos megabytes, e em troca retroceder é um memcpy.
 * Trocar por undo-log só se a simulação precisar de milhões de ticks. */
static void snapshot(Simulacao *s)
{
    if (s->n_hist >= s->cap_hist)
    {
        s->cap_hist = (s->cap_hist == 0) ? 64 : s->cap_hist*2;
        s->historico = realloc(s->historico,s->cap_hist* sizeof(Estado));
    }
    s->historico[s->n_hist] = estado_clonar(&s->atual);
    s->n_hist++;
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
        if (tarefa->estado == EST_INATIVA && tarefa->periodo > 0 &&
            executando->tick >= tarefa->ingresso &&
            (executando->tick - tarefa->ingresso) % tarefa->periodo == 0)
        {
            ativar_tarefa(tarefa, executando->tick);
        }
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

    /* 4) Preempção: tirar da CPU quem estava EXECUTANDO mas perdeu a vaga */
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (e->tarefas[i].estado == EST_EXECUTANDO)
        {
            int continua = 0;
            for (int j = 0; j < vencedores; j++)
            {
                if (candidatos[j] == i)
                {
                    continua = 1;
                    break;
                }
            }
            
            if (continua == 0)
            {
                // Libera a CPU e volta a tarefa para a fila
                e->cpus[e->tarefas[i].cpu].tarefa = -1;
                e->tarefas[i].cpu = -1;
                e->tarefas[i].estado = EST_PRONTA;
            }
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
        if(tarefa->estado == EST_EXECUTANDO)
        {
            tarefa->exec_restante--;
            if(executando->quantum > 0 && tarefa->quantum_restante > 0)
            {
                tarefa->quantum_restante--;

            }
            /* (a) Terminou esta execução: solta a CPU e conta mais uma. Na
             * 10a vira CONCLUIDA (req. 4.4); antes disso volta a INATIVA e
             * espera a próxima chegada do período. */
            if(tarefa->exec_restante<=0)
            {
                executando->cpus[tarefa->cpu].tarefa=-1;
                tarefa->cpu=-1;
                tarefa->quantum_restante=0;
                tarefa->ativacoes++;
                
                if(tarefa->ativacoes>=MAX_ATIVACOES)
                {
                    tarefa->estado=EST_CONCLUIDA;
                

                }else
                {
                    tarefa->estado = EST_INATIVA;
                }
            }
            /* (b) Quantum esgotado com trabalho por fazer: devolve a CPU e
             * volta para a fila. quantum 0 no arquivo = sem limite, por
             * isso o "quantum > 0". */
            else if(executando->quantum >0 && tarefa->quantum_restante<=0)
            {
                executando->cpus[tarefa->cpu].tarefa=-1;
                tarefa->cpu=-1;
                tarefa->estado = EST_PRONTA;

            }

        }


    }
}

/*
* REQUISITO: 2.6: Implementar a verificação de estouro de prazo (deadline) por tarefa
*/
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

int sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d)
{
    /* A busca vem ANTES de mexer em 's': se o algoritmo não existe, a
     * simulação que já estava carregada continua intacta. */
    const Escalonador *esc = escalonador_buscar(inicial->algoritmo);
    if (esc == NULL)
    {
        /* Erro claro (requisito 5): diz o que veio e o que é aceito. */
        char lista[128] = "";
        for (int i = 0; i < escalonador_qtde(); i++)
        {
            size_t usado = strlen(lista);
            snprintf(lista + usado, sizeof lista - usado, "%s%s",
                     i ? ", " : "", escalonador_em(i)->nome);
        }
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
    s->atual = estado_clonar(inicial);
    if (s->atual.ntarefas != inicial->ntarefas || s->atual.ncpus != inicial->ncpus)
    {
        estado_liberar(&s->atual);
        diag_erro(d, "falta de memoria ao iniciar a simulacao");
        return 0;
    }

    ativar(s);    /* ativações que já valem no tick 0 */
    escalonar(s); /* primeira atribuição de CPUs */
    s->terminada = todas_concluidas(&s->atual);
    snapshot(s);  /* historico[0] = tick 0 (requisito 1.5.2) */
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
     * vez de deixar o programa rodando sem fim. A mensagem é a mesma de
     * sim_executar_tudo, para os dois modos avisarem igual. */
    if (s->atual.tick >= MAX_TICKS)
    {
        s->terminada = 1;
        snprintf(s->ultimo_evento, sizeof s->ultimo_evento,
                 "simulacao interrompida no limite de %d ticks", MAX_TICKS);
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

    snapshot(s);         /* guarda a foto deste tick (req. 1.5.2)       */
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
     * poderia acontecer com "while (!s->terminada)". O limite de MAX_TICKS
     * (e o aviso de que a simulação foi CORTADA) fica no próprio sim_avancar. */
    while (sim_avancar(s))
        ;
}

/* Converte 'txt' em inteiro. Devolve 1 se o texto INTEIRO é um número
 * ("12"), 0 se não é ("abc", "", "12x") ou não cabe num int
 * ("99999999999"). O strtol diz onde parou de ler: se não andou nada, não
 * havia número; se sobrou texto, não era só número. Número grande demais
 * para um long o strtol avisa pelo errno; para um int, o teste é à mão
 * (no Linux 64 bits o long é maior que o int). */
static int ler_inteiro(const char *txt, int *saida)
{
    char *fim;
    long v;
    errno = 0;
    v = strtol(txt, &fim, 10);
    if (fim == txt || *fim != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX)
        return 0;
    *saida = (int)v;
    return 1;
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

    /* ---- campos numéricos: exec_restante e prazo ---- */
    if (strcmp(campo, "exec_restante") != 0 && strcmp(campo, "prazo") != 0)
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
        if (numero < 1 || numero > tarefa->duracao)
        {
            snprintf(motivo, motivo_tam,
                     "O valor %d nao cumpre os pre requisitos (deve ser entre 1 e %d).",
                     numero, tarefa->duracao);
            return 0;
        }
        tarefa->exec_restante = numero;
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
     * 190 tarefas e 5000 ticks, são centenas de MB que voltam aqui. */
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