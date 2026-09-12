#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Estado estado_clonar(const Estado *e)
{

}

void estado_liberar(Estado *e)
{

}

const char *estado_nome(EstadoTarefa e)
{

}

int sim_indice_por_id(const Estado *e, int id)
{

}


static void snapshot(Simulacao *s)
{

}


static void truncar_historico(Simulacao *s, int a_partir_de)
{
}


static void ativar(Simulacao *s)
{

}


static void escalonar(Simulacao *s)
{
}


static void executar(Simulacao *s)
{

}


static void verificar_prazos(Simulacao *s)
{

}

static int tudo_concluido(const Estado *e)
{

}


int sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d)
{

}

int sim_avancar(Simulacao *s)
{

}

int sim_retroceder(Simulacao *s)
{

}

void sim_executar_tudo(Simulacao *s)
{

}

int sim_editar_tarefa(Simulacao *s, int id_tarefa, const char *campo,
                      const char *valor, char *motivo, int motivo_tam)
{

}

void sim_liberar(Simulacao *s)
{

}
