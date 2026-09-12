# Build 100% com toolchain padrao (gcc + make), sem nenhuma lib externa.
# "make" gera o binario de desenvolvimento; "make release" gera um
# executavel estatico (sem depender de .so/.dll na maquina de destino
# -- ver requisito 5: nada pode precisar ser instalado para rodar).

CC = gcc
INCLUDE_DIR = include
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -I$(INCLUDE_DIR)
LDFLAGS =
TARGET = simulador
BUILD_DIR = build

# main.c fica na raiz, fora de src/, entao precisa entrar explicitamente --
# so com o wildcard de src/ nenhum objeto definiria main() e o link falharia.
SRC = main.c $(wildcard src/*.c)

# Todos os .o vao para build/, sem espelhar a arvore de diretorios: notdir
# achata os caminhos, entao src/config.c gera build/config.o (e nao
# build/src/config.o). Isso deixa UMA unica pasta a criar, o que evita
# precisar de `mkdir -p` -- que nao existe no cmd.exe do Windows.
# Cuidado: por achatar os nomes, dois fontes NAO podem ter o mesmo nome de
# arquivo em pastas diferentes (ex.: main.c e src/main.c se sobrescreveriam).
OBJ = $(addprefix $(BUILD_DIR)/,$(notdir $(SRC:.c=.o)))

# Com os nomes achatados, o make nao sabe mais de que pasta veio cada .o:
# vpath diz onde procurar os .c correspondentes.
vpath %.c . src

.PHONY: all release clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

# O `|` faz de build/ um pre-requisito "order-only": a pasta precisa existir
# antes de compilar, mas alteracoes nela nao forcam recompilacao (o horario
# de uma pasta muda a cada arquivo gravado dentro dela).
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# `mkdir` sem flags funciona tanto no sh quanto no cmd.exe. Como build/ e
# order-only, esta regra roda uma unica vez, quando a pasta ainda nao existe.
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Executavel estatico -- roda em qualquer maquina Linux sem precisar
# instalar nada (glibc estatica embutida no binario).
release: CFLAGS += -O2 -static
release: clean $(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
