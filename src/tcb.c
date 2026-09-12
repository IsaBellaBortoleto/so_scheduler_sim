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

}

void estado_liberar(Estado *e)
{

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

}
