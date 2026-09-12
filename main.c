/* ============================================================================
 * main.c — Interface com o usuário 
 * ==========================================================================*/
#include "config.h"
#include "escalonador.h"
#include "kernel.h"
#include "gantt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------- apresentação de mensagens --------------------- */
/* todo erro precisa chegar ao usuário com o motivo. */
static void mostrar_diagnostico(const Diagnostico *d)
{

}

/* ---- inspetor de tarefas: o "debugger" -------- */
static void mostrar_tarefas(const Simulacao *s)
{

}

static void mostrar_ajuda(void)
{

}

/* --------------------- relatório final ----------------- */
static void mostrar_resumo(const Simulacao *s)
{

}

/* ==========================================================================
 *  AUTOTESTE — roda com  ./simulador --teste
 *  Verifica as partes com lógica de verdade (parser e escalonador). Se algo
 *  for quebrado numa refatoração, o assert avisa na hora.
 * ========================================================================*/
static void autoteste(void)
{

}

/* ========================================================================== */

int main(int argc, char **argv)
{
 
}
