/* ============================================================================
 * tcb.c — Operacoes basicas sobre o TCB e sobre o estado do sistema
 * ----------------------------------------------------------------------------
 * Este arquivo implementa o que `tcb.h` declara: a copia profunda do estado,
 * a liberacao da memoria e o nome legivel de cada estado de tarefa.
 *
 * POR QUE A COPIA PROFUNDA VIVE AQUI, E NAO NO KERNEL:
 * O requisito 1.5.2 exige guardar o historico do sistema a cada passo para
 * poder avancar e retroceder a simulacao. Como a struct Estado contem dois
 * vetores alocados dinamicamente (tarefas e cpus), copiar a struct com `=`
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

Estado estado_clonar(const Estado *e)
{
    /* TODO:
     * 1- copia = *e -- copia os campos escalares de uma vez (inclui o
     *    array fixo algoritmo[16]).
     * 2- zera copia.tarefas e copia.cpus antes de mais nada, pra nao
     *    correr risco de "return" cedo demais deixar ponteiro lixo.
     * 3- se e->ntarefas > 0: malloc(ntarefas * sizeof(TCB)) e memcpy do
     *    conteudo de e->tarefas. Cuidado: TCB.eventos e um array fixo
     *    dentro da struct, entao memcpy da struct inteira ja copia o
     *    conteudo dele tambem (nao e um ponteiro separado).
     * 4- mesma ideia pra copia.cpus, com e->ncpus.
     * 5) se algum malloc falhar, deixar o campo correspondente em 0/NULL
     *    de forma consistente (nao deixar ntarefas>0 com tarefas==NULL). */
    Estado copia = {0};
    (void)e;
    return copia;
}

void estado_liberar(Estado *e)
{
    /* TODO: free(e->tarefas), free(e->cpus), e depois zerar os ponteiros
     * e os contadores (ntarefas=0, ncpus=0) pra tornar seguro chamar
     * estado_liberar duas vezes sobre o mesmo Estado (nao deve dar
     * double-free). */
    (void)e;
}

/* Nome legivel do estado, usado no inspetor de tarefas (requisito 1.5.1) e
 * nas mensagens de erro de edicao manual (requisito 3.4). */
const char *estado_nome(EstadoTarefa e)
{
    switch (e) {
        case EST_INATIVA:    return "INATIVA";
        case EST_PRONTA:     return "PRONTA";
        case EST_EXECUTANDO: return "EXECUTANDO";
        case EST_SUSPENSA:   return "SUSPENSA";
        case EST_CONCLUIDA:  return "CONCLUIDA";
    }
    /* TODO: o switch acima já cobre todos os valores do enum, mas o
     * compilador não sabe disso com certeza (um int fora do range
     * poderia, em teoria, ser convertido pra EstadoTarefa) -- acrescente
     * um `return` de segurança aqui embaixo (ex. "DESCONHECIDO") pra
     * -Wall não reclamar de "control reaches end of non-void function". */
}