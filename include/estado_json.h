#ifndef ESTADO_JSON_H
#define ESTADO_JSON_H

#include <stddef.h>
#include "kernel.h"
//SUPONDO INTERFACE EM HTTP/JSON
int estado_json_atual(const Simulacao *s, char *buf, size_t tam);

int estado_json_historico(const Simulacao *s, int indice, char *buf, size_t tam);

int diagnostico_json(const Diagnostico *d, char *buf, size_t tam);

#endif