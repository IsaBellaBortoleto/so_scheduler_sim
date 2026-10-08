/* ============================================================================
 * estado_json.c — transforma dados da simulação em texto JSON para a página
 *
 * RESUMO
 *  - A página web não enxerga as structs do C. Ela recebe TEXTO, no formato
 *    JSON, e é este arquivo que escreve esse texto.
 *  - O JSON é montado à mão, com snprintf: usar uma biblioteca de JSON seria
 *    instalar dependência, o que o requisito 5 proíbe.
 *  - Tudo é escrito num buffer de tamanho fixo, dado por quem chama. Se não
 *    couber, a função devolve -1 e não entrega nada pela metade: um JSON
 *    cortado quebra a página sem mostrar erro nenhum.
 *  - Textos passam por sai_texto, que "escapa" aspas e barras. Sem isso, uma
 *    mensagem como   quantum invalido "x"   estragaria o JSON.
 * ==========================================================================*/
#include "estado_json.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------- escritor -------------------------------- */

/* Escritor com limite: vai acrescentando texto no buffer e lembra se algo
 * não coube. Assim as funções abaixo escrevem sem conferir o tamanho a cada
 * linha, e a conferência é feita uma vez só, no fim. */
typedef struct {
    char  *buf;       /* onde o texto é escrito                      */
    size_t tam;       /* tamanho total de buf                        */
    size_t pos;       /* quantos caracteres já foram escritos        */
    int    estourou;  /* 1 se alguma escrita não coube               */
} Saida;

/* Acrescenta texto formatado, como um printf que escreve no fim do buffer. */
static void sai(Saida *s, const char *fmt, ...)
{
    va_list args;
    size_t resta = s->tam - s->pos; /* espaço livre, contando o '\0' */
    int n;

    /* Depois que algo não coube, nada mais é escrito: o resultado já está
     * perdido e quem chamou vai devolver -1. */
    if (s->estourou)
        return;

    /* Escreve no FIM do que já existe. O vsnprintf é chamado UMA vez só e o
     * retorno fica guardado em n: uma va_list só pode ser percorrida uma
     * vez, usá-la numa segunda chamada é comportamento indefinido. */
    va_start(args, fmt);
    n = vsnprintf(s->buf + s->pos, resta, fmt, args);
    va_end(args);

    /* n = tamanho que a mensagem TEM (não o que foi escrito). Negativo é
     * erro de formato; n >= resta quer dizer que não coube com o '\0'. */
    if (n < 0 || (size_t)n >= resta)
        s->estourou = 1;
    else
        s->pos += (size_t)n;
}

/* Escreve um texto como string JSON: entre aspas e com os caracteres
 * especiais "escapados". */
static void sai_texto(Saida *s, const char *txt)
{
    sai(s, "\""); /* aspa de abertura */

    /* txt NULL vale como texto vazio: o laço não roda e sai só "". */
    for (const char *p = txt; p != NULL && *p != '\0'; p++)
    {
        /* unsigned: byte de letra acentuada (UTF-8) é negativo em char, e
         * cairia por engano no teste "c < 0x20" lá embaixo. */
        unsigned char c = (unsigned char)*p;
        switch (c)
        {
        /* Os dois que quebrariam o JSON: a aspa fecharia a string antes da
         * hora, e a barra iniciaria um escape. */
        case '"':  sai(s, "\\\""); break;
        case '\\': sai(s, "\\\\"); break;
        /* JSON não aceita quebra de linha nem tab "crus" dentro de string. */
        case '\n': sai(s, "\\n");  break;
        case '\r': sai(s, "\\r");  break;
        case '\t': sai(s, "\\t");  break;
        default:
            if (c < 0x20)
                sai(s, "\\u%04x", c); /* outro caractere de controle */
            else
                sai(s, "%c", c);      /* caractere comum: ele mesmo */
        }
    }

    sai(s, "\""); /* aspa de fechamento */
}

/* ------------------------------ diagnóstico ------------------------------ */

/* Escreve uma lista JSON de textos:  ["msg 1","msg 2"]
 * 'lista' é a matriz do Diagnostico (erros ou avisos) e n, quantas linhas
 * dela estão em uso. */
static void sai_lista(Saida *s, const char lista[][MAX_DIAG_TXT], int n)
{
    sai(s, "[");
    for (int i = 0; i < n; i++)
    {
        /* Vírgula ENTRE os itens: antes de cada um, menos do primeiro. JSON
         * não aceita vírgula sobrando depois do último. */
        if (i > 0)
            sai(s, ",");
        sai_texto(s, lista[i]);
    }
    sai(s, "]");
}

/* Monta   {"erro":true,"erros":["..."],"avisos":["..."]}
 * Devolve quantos caracteres escreveu, ou -1 se não coube em buf. */
int diagnostico_json(const Diagnostico *d, char *buf, size_t tam)
{
    Saida s = { buf, tam, 0, 0 };

    /* O JSON não é escrito "de uma vez": cada sai() acrescenta um pedaço. */
    sai(&s, "{\"erro\":true,\"erros\":");
    sai_lista(&s, d->erros, d->n_erros);
    sai(&s, ",\"avisos\":");
    sai_lista(&s, d->avisos, d->n_avisos);
    sai(&s, "}");

    if (s.estourou)
    {
        /* Não entrega JSON pela metade: esvazia o que já tinha escrito. */
        if (tam > 0)
            buf[0] = '\0';
        return -1;
    }
    return (int)s.pos;
}

/* -------------------------------- estado --------------------------------- */

/* Uma CPU:  {"id":0,"tarefa":3,"ticks_desligada":2}
 * Na struct, CPU.tarefa é a POSIÇÃO no vetor de tarefas; a página espera o
 * ID da tarefa (o número que o usuário escreveu no arquivo), ou -1 se a CPU
 * está desligada. */
static void sai_cpu(Saida *s, const Estado *e, const CPU *c)
{
    int id_tarefa = (c->tarefa < 0) ? -1 : e->tarefas[c->tarefa].id;
    sai(s, "{\"id\":%d,\"tarefa\":%d,\"ticks_desligada\":%d}",
        c->id, id_tarefa, c->ticks_desligada);
}

/* Uma tarefa, com todos os campos do TCB que a página mostra. */
static void sai_tarefa(Saida *s, const TCB *t)
{
    /* %02X = dois dígitos hexadecimais com zero à esquerda: 255,0,10 vira
     * "#FF000A", o formato de cor do CSS. */
    sai(s, "{\"id\":%d,\"cor\":\"#%02X%02X%02X\",", t->id, t->r, t->g, t->b);
    sai(s, "\"ingresso\":%d,\"duracao\":%d,\"periodo\":%d,\"prazo\":%d,",
        t->ingresso, t->duracao, t->periodo, t->prazo);
    /* estado_nome só devolve palavras fixas (INATIVA, PRONTA...): não
     * precisa de sai_texto. */
    sai(s, "\"estado\":\"%s\",", estado_nome(t->estado));
    sai(s, "\"exec_restante\":%d,\"quantum_restante\":%d,",
        t->exec_restante, t->quantum_restante);
    sai(s, "\"ativacao\":%d,\"deadline_abs\":%d,\"ativacoes\":%d,",
        t->ativacao, t->deadline_abs, t->ativacoes);
    sai(s, "\"perdeu_prazo\":%d,\"cpu\":%d,\"sorteada\":%d,",
        t->perdeu_prazo, t->cpu, t->sorteada);
    /* 'eventos' veio do arquivo do usuário: pode ter aspas, tem que escapar. */
    sai(s, "\"eventos\":");
    sai_texto(s, t->eventos);
    sai(s, "}");
}

/* Monta o objeto "Estado" descrito no topo de web/app.js, a partir do tick
 * atual. Devolve quantos caracteres escreveu, ou -1 se não coube em buf.
 *
 * Antes de carregar um arquivo a Simulacao está zerada (ver main.c): o
 * resultado é um Estado válido com "carregado":false e as listas vazias, sem
 * precisar de caso especial. */
int estado_json_atual(const Simulacao *sim, char *buf, size_t tam)
{
    Saida s = { buf, tam, 0, 0 };
    const Estado *e = &sim->atual;

    /* sim->esc só deixa de ser NULL quando sim_iniciar deu certo. */
    sai(&s, "{\"carregado\":%s,", sim->esc != NULL ? "true" : "false");
    sai(&s, "\"tick\":%d,", e->tick);
    sai(&s, "\"terminada\":%s,", sim->terminada ? "true" : "false");
    sai(&s, "\"algoritmo\":");
    sai_texto(&s, e->algoritmo);
    sai(&s, ",\"quantum\":%d,\"ncpus\":%d,", e->quantum, e->ncpus);
    sai(&s, "\"ultimo_evento\":");
    sai_texto(&s, sim->ultimo_evento);

    sai(&s, ",\"cpus\":[");
    for (int i = 0; i < e->ncpus; i++)
    {
        if (i > 0)
            sai(&s, ","); /* vírgula ENTRE os itens */
        sai_cpu(&s, e, &e->cpus[i]);
    }

    sai(&s, "],\"tarefas\":[");
    for (int i = 0; i < e->ntarefas; i++)
    {
        if (i > 0)
            sai(&s, ",");
        sai_tarefa(&s, &e->tarefas[i]);
    }
    sai(&s, "]}");

    if (s.estourou)
    {
        /* Não entrega JSON pela metade: esvazia o que já tinha escrito. */
        if (tam > 0)
            buf[0] = '\0';
        return -1;
    }
    return (int)s.pos;
}
