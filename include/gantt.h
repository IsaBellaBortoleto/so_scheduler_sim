#ifndef GANTT_H
#define GANTT_H

#include "kernel.h"


void gantt_preparar_terminal(void);

void gantt_terminal(const Simulacao *s, int janela);

int  gantt_svg(const Simulacao *s, const char *caminho);

#endif 
