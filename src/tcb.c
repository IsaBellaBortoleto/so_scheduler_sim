/* ============================================================================
 * tcb.c — Operacoes basicas sobre o TCB e sobre o estado do sistema
 * ----------------------------------------------------------------------------
 * Este arquivo implementa o que `tcb.h` declara: a copia profunda do estado,
 * a liberacao da memoria e o nome legivel de cada estado de tarefa.
 *
 * POR QUE A COPIA PROFUNDA VIVE AQUI, E NAO NO KERNEL:
 * O requisito 1.5.2 exige guardar o historico do sistema a cada passo para
 * poder avancar e retroceder a simulacao. Como a struct Estado contem dois
 * vetores alocados dinamicamente(tarefas e cpus), copiar a struct com `=`
 * copiaria apenas os PONTEIROS: dois "estados" diferentes passariam a
 * compartilhar as mesmas tarefas, e alterar um alteraria o outro. Retroceder
 * deixaria de funcionar.
 *
 * estado_clonar() resolve isso duplicando tambem o conteudo dos vetores.
 * Com isso, cada fotografia do historico e totalmente independente das
 * outras, e retroceder se resume a restaurar uma delas.
 * ==========================================================================*/
#include "tcb.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Copia PROFUNDA de um Estado: os campos simples e tambem o conteudo dos
 * vetores de tarefas e de CPUs. Se faltar memoria, o vetor que falhou volta
 * NULL com o contador em 0 (nunca ntarefas > 0 com tarefas == NULL); quem
 * chama confere comparando os contadores com os do original. */
Estado estado_clonar(const Estado *e)
{
     /* '=' copia os campos simples de uma vez (inclusive algoritmo[16]). Os
      * dois ponteiros sao zerados logo em seguida: os vetores serao NOVOS. */
     Estado copia = *e;
     copia.tarefas = NULL;
     copia.cpus = NULL;
     if(e->ntarefas > 0)
     {
      /* memcpy das structs inteiras: TCB.eventos e um vetor DENTRO da
       * struct (nao um ponteiro), entao o texto vem junto. */
        copia.tarefas = malloc(e->ntarefas*sizeof(*e->tarefas));
      if (copia.tarefas != NULL)
      {
        memcpy(copia.tarefas, e->tarefas,e->ntarefas*sizeof(*e->tarefas));
      }else
      {
        copia.ntarefas = 0;
      }
     }
     if(e->ncpus > 0)
     {
        copia.cpus = malloc(e->ncpus*sizeof(*e->cpus));
        if (copia.cpus != NULL)
        {
            memcpy(copia.cpus,e->cpus,e->ncpus*sizeof(*e->cpus));
        }else
        {
            copia.ncpus = 0;
        }
     }
    return copia;
}

/* Libera os vetores de um Estado e o deixa zerado. Pode ser chamada duas
 * vezes sobre o mesmo Estado: na segunda os ponteiros ja sao NULL, e
 * free(NULL) nao faz nada. */
void estado_liberar(Estado *e)
{
    free(e->tarefas);
    free(e->cpus);
    e->tarefas = NULL;
    e->cpus = NULL;
    e->ntarefas = 0;
    e->ncpus = 0;
}

/* Nome legivel do estado: o texto da coluna "estado" da pagina (requisito
 * 1.5.1) e o que se digita na edicao manual (requisito 3.4). */
const char *tipo_nome(TipoTarefa t)
{
    switch (t) {
        case TIPO_PERIODICA:  return "PERIODICA";
        case TIPO_APERIODICA: return "APERIODICA";
        case TIPO_ESPORADICA: return "ESPORADICA";
    }
    return "?";
}

const char *estado_nome(EstadoTarefa e)
{
    switch (e) {
        case EST_INATIVA:
            return "INATIVA";
        case EST_PRONTA:
            return "PRONTA";
        case EST_EXECUTANDO:
            return "EXECUTANDO";
        case EST_SUSPENSA:
            return "SUSPENSA";
        case EST_CONCLUIDA:
            return "CONCLUIDA";

    }
    /* O switch cobre todo o enum, mas um valor invalido (memoria estragada)
     * cairia aqui: devolve um texto em vez de lixo, e o -Wall nao reclama
     * de funcao sem return. */
    return "DESCONHECIDO";
}

/* ORDEM NATURAL entre dois ids (ver tcb.h). Anda nos dois textos ao mesmo
 * tempo e, a cada passo, há dois casos:
 *   - os dois estão num NÚMERO: lê o número inteiro de cada lado (strtol)
 *     e compara os valores -- é o que põe "t2" antes de "t10";
 *   - senão: compara uma letra de cada, em maiúscula (req. 3.3.2).
 * O primeiro trecho diferente decide. */
int id_comparar(const char *a, const char *b)
{
    const char *ia = a, *ib = b;
    while (*ia && *ib)
    {
        if (isdigit((unsigned char)*ia) && isdigit((unsigned char)*ib))
        {
            /* strtol devolve o número e deixa em fa/fb onde ele acabou. */
            char *fa, *fb;
            long na = strtol(ia, &fa, 10), nb = strtol(ib, &fb, 10);
            if (na != nb)
                return (na < nb) ? -1 : 1;
            ia = fa;
            ib = fb;
        }
        else
        {
            /* O cast para unsigned char é exigência do toupper/isdigit:
             * com char negativo (letra acentuada) o comportamento é
             * indefinido. */
            int ca = toupper((unsigned char)*ia), cb = toupper((unsigned char)*ib);
            if (ca != cb)
                return ca - cb;
            ia++;
            ib++;
        }
    }
    if (*ia != *ib)
        return *ia ? 1 : -1; /* um acabou antes: o mais curto vem primeiro */
    /* Iguais na ordem natural mas escritos diferente ("t01" x "t1", "T1" x
     * "t1"): o strcmp só garante que a resposta seja sempre a mesma. No
     * simulador isso quase não acontece, porque o config.c recusa ids que
     * só diferem por maiúsculas. */
    return strcmp(a, b);
}
