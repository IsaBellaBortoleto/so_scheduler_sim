/* ============================================================================
 * gantt.c — O ÚNICO lugar onde o gráfico de Gantt é desenhado (req. 2)
 * ----------------------------------------------------------------------------
 * Decisão de projeto (opção A, ver gantt.h): a página web NÃO desenha. Ela
 * pede o SVG ao servidor e exibe. Esta mesma função gera:
 *   - a tela ao vivo (req. 2.3), só com os últimos GANTT_JANELA_TELA ticks;
 *   - o arquivo exportado (req. 2.4), com a simulação inteira.
 * Assim cada regra de desenho abaixo existe uma vez só, e a imagem exportada
 * é idêntica à da tela -- e, gerada no C, indiscutivelmente não é print.
 *
 * A legenda também é desenhada AQUI, dentro do SVG (e não no HTML): assim
 * ela aparece igual na tela e no arquivo exportado.
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


/* Tipos de marcador de evento. A prioridade de exibição fica no marca_em. */
typedef enum {
    MARCA_NENHUMA = 0,
    MARCA_SORTEIO,     /* a tarefa venceu um desempate por sorteio (4.3-5)   */
    MARCA_CHEGADA,     /* nova ativação da tarefa entrou no sistema (2.2)    */
    MARCA_TERMINO,     /* a ativação terminou (2.2)                          */
    MARCA_PRAZO        /* deadline perdido: é um ERRO e deve aparecer (3.3)  */
} Marca;

/* Descobre o evento ocorrido na tarefa `i` no instante `t`, comparando
 * historico[t] com historico[t-1]. Uma célula mostra UM marcador; quando
 * mais de um se aplica: prazo > término > chegada > sorteio. */
static int marca_em(const Simulacao *s, int t, int i)
{
    const TCB *agora = &s->historico[t].tarefas[i];
    const TCB *antes = (t > 0) ? &s->historico[t - 1].tarefas[i] : NULL;
    int marcas = 0;
    int ativa = agora->estado == EST_PRONTA || agora->estado == EST_EXECUTANDO ||
                agora->estado == EST_SUSPENSA;

    /* PRAZO (a): a execução em andamento. O X vai no tick em que o prazo
     * dela vence. A passagem de 0 para 1 cobre os outros casos (ex.: uma
     * edição que diminuiu o prazo). */
    if (agora->perdeu_prazo &&
        (agora->deadline_abs == t || antes == NULL || !antes->perdeu_prazo))
        marcas |= 1 << MARCA_PRAZO;

    /* PRAZO (b): execuções ainda na fila. A j-ésima chegou j períodos
     * depois da atual, e vence 'prazo' ticks depois disso. Sem esta regra,
     * uma tarefa atrasada mostraria um X só, por mais execuções que
     * perdesse. */
    for (int j = 1; j <= agora->pendentes; j++)
        if (agora->ativacao + j * agora->periodo + agora->prazo == t)
            marcas |= 1 << MARCA_PRAZO;

    if (antes != NULL && agora->ativacoes > antes->ativacoes)
        marcas |= 1 << MARCA_TERMINO;

    /* CHEGADA: uma execução nova começou agora, ou chegou e foi para a
     * fila porque a anterior ainda não tinha terminado (pendentes subiu). */
    if ((ativa && agora->ativacao == t) ||
        (antes != NULL && agora->pendentes > antes->pendentes))
        marcas |= 1 << MARCA_CHEGADA;

    if (agora->sorteada && agora->estado == EST_EXECUTANDO &&
        (antes == NULL || antes->estado != EST_EXECUTANDO))
        marcas |= 1 << MARCA_SORTEIO;
    return marcas;
}

/* Ordem das linhas: índices das tarefas ordenados por id CRESCENTE.
 * linha 0 = menor id = mais próxima do eixo X (requisito 2.5). */
static void ordem_por_id(const Estado *e, int *ordem)
{
    /* Insertion sort que já preenche: o índice i entra no fim e anda para
     * trás enquanto o id da vizinha for maior. Não usa qsort porque a
     * comparação precisa de e->tarefas, e o comparador do qsort não recebe
     * contexto (só por variável global). Os ids são únicos (config.c recusa
     * repetidos), então não há empate.*/
    for (int i = 0; i < e->ntarefas; i++)
    {
        int j = i;
        while (j > 0 && e->tarefas[ordem[j - 1]].id > e->tarefas[i].id)
        {
            ordem[j] = ordem[j - 1];
            j--;
        }
        ordem[j] = i;
    }
}

/* ==========================================================================
 *  DESENHO — SVG da tela ao vivo e do arquivo exportado
 * ========================================================================*/

#define CEL_W  14   /* largura de 1 tick, em pixels                          */
#define LIN_H  22   /* altura de uma linha do gráfico                        */
#define MARG_E 90   /* margem esquerda, onde vão os rótulos das linhas       */
#define MARG_T 50   /* margem superior, para o título                        */
#define LEG_W  110  /* largura de um item da legenda                         */

/* Símbolo de cada marcador, na ordem do enum Marca. São caracteres de texto
 * (UTF-8), não desenhos: o SVG vai com charset=utf-8 e o navegador os
 * desenha com a fonte. */
static const char *const SIMBOLO[] = { "", "★", "▲", "▼", "✖" };
static const char *const COR_SIMBOLO[] = { "", "#8a6d00", "black", "black", "red" };

static const int CANTO_DX[] = { 0, CEL_W - 3, 3, 3, CEL_W - 3 };
static const int CANTO_DY[] = { 0, LIN_H - 3, LIN_H - 3, 10, 10 };

/* Itens da legenda (requisito 2.1). 'marca' != MARCA_NENHUMA desenha o
 * símbolo do marcador em vez da amostra de preenchimento. */
static const struct {
    const char *fill;
    Marca marca;
    const char *texto;
} LEGENDA[] = {
    { "#4a90d9",    MARCA_NENHUMA, "executando" },
    { "none",       MARCA_NENHUMA, "pronta" },
    { "url(#susp)", MARCA_NENHUMA, "suspensa" },
    { "url(#desl)", MARCA_NENHUMA, "CPU desligada" },
    { NULL,         MARCA_CHEGADA, "chegada" },
    { NULL,         MARCA_TERMINO, "término" },
    { NULL,         MARCA_PRAZO,   "prazo perdido" },
    { NULL,         MARCA_SORTEIO, "sorteio" },
};
#define N_LEGENDA ((int)(sizeof LEGENDA / sizeof LEGENDA[0]))

/* Uma célula de LIN_H - 4 de altura, com 2 px de folga em cima e embaixo. */
static void celula(FILE *f, int x, int y, const char *fill, const char *contorno)
{
    fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\" "
               "stroke=\"%s\" stroke-width=\"0.5\"/>\n",
            x, y + 2, CEL_W, LIN_H - 4, fill, contorno);
}

/* LAYOUT, de cima para baixo:
 *   título
 *   uma linha por CPU (requisito 1.2: CPU desligada aparece hachurada)
 *   uma linha por tarefa, MAIOR id em cima e menor encostado no eixo X (2.5)
 *   eixo X, com o número REAL do tick a cada 5
 *   legenda
 * Coluna de um tick: x = MARG_E + (t - tick_ini) * CEL_W. Subtrair tick_ini
 * é o que faz a janela da tela ao vivo começar na borda do desenho. */
int gantt_svg(const Simulacao *s, FILE *saida, int tick_ini, int tick_fim)
{
    const Estado *e;
    int nt, nc, n_ticks, largura, altura, y_eixo, *ordem;

    if (s->n_hist <= 0 || tick_ini < 0 || tick_ini > tick_fim ||
        tick_fim >= s->n_hist)
        return 0;

    /* Tarefas e CPUs são as mesmas em todas as fotos do histórico. */
    e = &s->historico[tick_fim];
    nt = e->ntarefas;
    nc = e->ncpus;
    ordem = malloc((nt > 0 ? nt : 1) * sizeof *ordem);
    if (ordem == NULL)
        return 0;
    ordem_por_id(e, ordem);

    n_ticks = tick_fim - tick_ini + 1;
    largura = MARG_E + n_ticks * CEL_W + 10;
    if (largura < 10 + N_LEGENDA * LEG_W) /* a legenda precisa caber */
        largura = 10 + N_LEGENDA * LEG_W;
    y_eixo = MARG_T + (nc + nt) * LIN_H;
    altura = y_eixo + 60; /* números do eixo + legenda */

    /* Cabeçalho, fundo branco e os dois padrões de preenchimento:
     * 'desl' = hachura cinza (CPU desligada), 'susp' = pontilhado preto
     * (SUSPENSA: preta, mas diferente do sólido da execução). */
    fprintf(saida,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
            "font-family=\"sans-serif\" font-size=\"11\">\n"
            "<defs>\n"
            "<pattern id=\"desl\" width=\"4\" height=\"4\" patternUnits=\"userSpaceOnUse\" "
            "patternTransform=\"rotate(45)\"><rect width=\"2\" height=\"4\" fill=\"#999\"/></pattern>\n"
            "<pattern id=\"susp\" width=\"4\" height=\"4\" patternUnits=\"userSpaceOnUse\">"
            "<circle cx=\"2\" cy=\"2\" r=\"1\" fill=\"black\"/></pattern>\n"
            "</defs>\n"
            "<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n"
            "<text x=\"10\" y=\"25\" font-size=\"14\">%s, ticks %d a %d</text>\n",
            largura, altura, e->algoritmo, tick_ini, tick_fim);

    /* Faixa das CPUs: na cor da tarefa que a ocupa, ou hachurada. */
    for (int c = 0; c < nc; c++)
    {
        int y = MARG_T + c * LIN_H;
        fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">CPU %d</text>\n",
                MARG_E - 8, y + LIN_H / 2 + 4, e->cpus[c].id);
        for (int t = tick_ini; t <= tick_fim; t++)
        {
            const Estado *h = &s->historico[t];
            int x = MARG_E + (t - tick_ini) * CEL_W;
            int idx = h->cpus[c].tarefa;
            char cor[8];
            if (idx < 0)
            {
                celula(saida, x, y, "url(#desl)", "#ccc");
                continue;
            }
            snprintf(cor, sizeof cor, "#%02X%02X%02X",
                     h->tarefas[idx].r, h->tarefas[idx].g, h->tarefas[idx].b);
            celula(saida, x, y, cor, "#ccc");
        }
    }

    /* Tarefas: linha k = k-ésimo menor id, contada de baixo para cima. */
    for (int k = 0; k < nt; k++)
    {
        int i = ordem[k];
        int y = MARG_T + (nc + nt - 1 - k) * LIN_H;
        fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">T%d</text>\n",
                MARG_E - 8, y + LIN_H / 2 + 4, e->tarefas[i].id);
        for (int t = tick_ini; t <= tick_fim; t++)
        {
            const Estado *h = &s->historico[t];
            const TCB *tc = &h->tarefas[i];
            int x = MARG_E + (t - tick_ini) * CEL_W;
            int marcas = marca_em(s, t, i);

            if (tc->estado == EST_EXECUTANDO)
            {
                char cor[8];
                /* Texto preto em cor clara, branco em cor escura
                 * (luminância percebida, pesos da ITU-R BT.601). */
                int claro = (tc->r * 299 + tc->g * 587 + tc->b * 114) / 1000 > 150;
                snprintf(cor, sizeof cor, "#%02X%02X%02X", tc->r, tc->g, tc->b);
                celula(saida, x, y, cor, "#555");
                if (tc->cpu >= 0 && tc->cpu < h->ncpus)
                    fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" "
                                   "font-size=\"9\" fill=\"%s\">%d</text>\n",
                            x + CEL_W / 2, y + LIN_H / 2 + 3,
                            claro ? "black" : "white", h->cpus[tc->cpu].id);
            }
            else if (tc->estado == EST_PRONTA)
                celula(saida, x, y, "none", "#555");
            else if (tc->estado == EST_SUSPENSA)
                celula(saida, x, y, "url(#susp)", "black");
            /* INATIVA/CONCLUIDA: nada. */

            for (int m = MARCA_SORTEIO; m <= MARCA_PRAZO; m++)
                if (marcas & (1 << m))
                    fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" "
                                   "font-size=\"8\" fill=\"%s\">%s</text>\n",
                            x + CANTO_DX[m], y + CANTO_DY[m], COR_SIMBOLO[m], SIMBOLO[m]);
        }
    }
    free(ordem);

    /* Eixo X e o número REAL do tick (t, não t - tick_ini) a cada 5. */
    fprintf(saida, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"black\"/>\n",
            MARG_E, y_eixo, MARG_E + n_ticks * CEL_W, y_eixo);
    for (int t = tick_ini; t <= tick_fim; t++)
        if (t % 5 == 0)
            fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\">%d</text>\n",
                    MARG_E + (t - tick_ini) * CEL_W + CEL_W / 2, y_eixo + 14, t);

    /* Legenda (obrigatória), dentro do SVG para sair igual na tela e no
     * arquivo exportado. */
    for (int k = 0; k < N_LEGENDA; k++)
    {
        int x = 10 + k * LEG_W, y = y_eixo + 28;
        if (LEGENDA[k].marca == MARCA_NENHUMA)
            fprintf(saida, "<rect x=\"%d\" y=\"%d\" width=\"12\" height=\"12\" "
                           "fill=\"%s\" stroke=\"#555\"/>\n",
                    x, y, LEGENDA[k].fill);
        else
            fprintf(saida, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" fill=\"%s\">%s</text>\n",
                    x + 6, y + 10, COR_SIMBOLO[LEGENDA[k].marca], SIMBOLO[LEGENDA[k].marca]);
        fprintf(saida, "<text x=\"%d\" y=\"%d\">%s</text>\n", x + 17, y + 10, LEGENDA[k].texto);
    }

    fprintf(saida, "</svg>\n");
    return 1;
}
