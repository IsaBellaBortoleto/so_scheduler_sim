# Build 100% com toolchain padrao (gcc + make), sem nenhuma lib externa.
# "make" gera o binario de desenvolvimento; "make release" gera um
# executavel estatico (sem depender de .so/.dll na maquina de destino
# -- ver requisito 5: nada pode precisar ser instalado para rodar).

CC = gcc
INCLUDE_DIR = include
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -I$(INCLUDE_DIR)
LDFLAGS =
TARGET = simulador
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

.PHONY: all release clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Executavel estatico -- roda em qualquer maquina Linux sem precisar
# instalar nada (glibc estatica embutida no binario).
release: CFLAGS += -O2 -static
release: clean $(TARGET)

clean:
	rm -f src/*.o $(TARGET)
