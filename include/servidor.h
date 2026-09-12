#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "kernel.h"

//SUPONDO INTERFACE EM HTTP

int servidor_iniciar(Simulacao *sim, int porta);


void servidor_abrir_navegador(int porta);

#endif