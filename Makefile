# Build 100% com toolchain padrao (gcc + make), sem nenhuma lib externa.
# "make" gera o binario de desenvolvimento; "make release" gera o executavel
# da ENTREGA: um unico arquivo estatico que roda sem instalar nada -- nem
# runtime, nem DLL do MinGW, nem a pasta web/ (ela vai embutida no binario).

CC          = gcc
INCLUDE_DIR = include
BUILD_DIR   = build

# -MMD -MP: ao compilar cada .c, o gcc grava ao lado do .o um arquivo .d com a
# lista dos .h que ele incluiu, e o `-include` no fim deste Makefile le essas
# listas. Sem isso, editar um .h (ex. acrescentar um campo no TCB) NAO
# recompila os .c que o usam: cada .o fica com uma ideia diferente do tamanho
# da struct e a memoria corrompe em silencio, sem erro nenhum de compilacao.
# -I$(BUILD_DIR): o recursos_web.h e GERADO dentro de build/ (ver abaixo).
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -MMD -MP -I$(INCLUDE_DIR) -I$(BUILD_DIR)
LDFLAGS =

# ---------------------------------------------------------------------------
# Diferencas entre Windows e Linux/Mac. A variavel OS so existe no Windows.
#
#   EXE    O gcc do Windows acrescenta .exe ao binario. Se o nome do alvo
#          nao tiver a extensao, o make procura "simulador", nunca acha, e
#          refaz o alvo em TODO build. Com a ferramenta gerar_recursos isso
#          vira uma cascata: refaz a ferramenta -> regera recursos_web.h ->
#          recompila o servidor, sempre.
#
#   LDLIBS No Windows os sockets vem da Winsock (ws2_32), uma DLL do proprio
#          sistema operacional, presente em qualquer Windows -- nao e uma
#          biblioteca a instalar. No Linux/Mac os sockets ja fazem parte da
#          libc e nao precisam de nada.
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    EXE    = .exe
    LDLIBS = -lws2_32
else
    EXE    =
    LDLIBS =
endif

TARGET = simulador$(EXE)

# main.c fica na raiz, fora de src/, entao precisa entrar explicitamente --
# so com o wildcard de src/ nenhum objeto definiria main() e o link falharia.
# Novos modulos (servidor.c, estado_json.c) entram sozinhos pelo wildcard.
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

# ---------------------------------------------------------------------------
# Interface web embutida no executavel
#
# O professor quer UM executavel. Se a pagina ficasse em arquivos soltos na
# pasta web/, o simulador so funcionaria com essa pasta ao lado dele. Por isso
# o build acontece em tres etapas:
#
#   1. compila a ferramenta gerar_recursos (roda so na maquina de quem compila,
#      nao faz parte da entrega);
#   2. roda a ferramenta, que converte cada arquivo de web/ num vetor de bytes
#      C dentro de build/recursos_web.h;
#   3. compila o servidor.c, que da #include nesse .h e serve a pagina direto
#      da memoria.
#
# O .h gerado fica em build/, e nao em include/, de proposito: arquivo gerado
# misturado com arquivo escrito a mao acaba sendo editado por engano (e a
# edicao some no proximo build) ou commitado. Em build/ ele ja e ignorado pelo
# git e apagado pelo `make clean`.
# ---------------------------------------------------------------------------
WEB_ARQUIVOS = $(wildcard web/*)
FERRAMENTA   = $(BUILD_DIR)/gerar_recursos$(EXE)
RECURSOS_H   = $(BUILD_DIR)/recursos_web.h

.PHONY: all release clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

# O `|` faz de build/ um pre-requisito "order-only": a pasta precisa existir
# antes de compilar, mas alteracoes nela nao forcam recompilacao (o horario
# de uma pasta muda a cada arquivo gravado dentro dela).
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Etapa 1. Flags proprias, e nao $(CFLAGS): a ferramenta nao deve herdar o
# -static da release nem gerar .d -- ela so precisa rodar aqui, uma vez.
$(FERRAMENTA): ferramentas/gerar_recursos.c | $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -O2 -o $@ $<

# Etapa 2. Depende da ferramenta E de cada arquivo da pagina: editar o app.js
# regera o .h, que por sua vez recompila o servidor. Sem listar $(WEB_ARQUIVOS)
# aqui, uma mudanca na pagina nunca chegaria ao executavel.
$(RECURSOS_H): $(FERRAMENTA) $(WEB_ARQUIVOS)
	$(FERRAMENTA) $@ $(WEB_ARQUIVOS)

# Etapa 3. Os .d do -MMD so existem DEPOIS da primeira compilacao. Num build
# do zero o make ainda nao sabe que servidor.c inclui recursos_web.h, e
# tentaria compilar o servidor antes de o .h existir. Esta linha declara a
# dependencia na mao para garantir a ordem ja no primeiro build.
$(BUILD_DIR)/servidor.o: $(RECURSOS_H)

# `mkdir` sem flags funciona tanto no sh quanto no cmd.exe. Como build/ e
# order-only, esta regra roda uma unica vez, quando a pasta ainda nao existe.
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Executavel da entrega. -static embute no binario tudo o que viria de DLL/.so
# (libgcc, winpthread no MinGW, glibc no Linux). A ws2_32 continua dinamica,
# mas ela faz parte do Windows, entao o arquivo roda numa maquina limpa.
release: CFLAGS += -O2 -static
release: clean $(TARGET)

# O make do Windows executa as receitas pelo cmd.exe, que nao tem `rm`.
# O `-` na frente ignora o erro quando o arquivo ja nao existe.
clean:
ifeq ($(OS),Windows_NT)
	-if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
	-if exist $(TARGET) del /Q $(TARGET)
else
	rm -rf $(BUILD_DIR) $(TARGET)
endif

# Le as dependencias de cabecalho gravadas pelo -MMD. O `-` evita erro no
# primeiro build, quando nenhum .d existe ainda.
-include $(OBJ:.o=.d)
