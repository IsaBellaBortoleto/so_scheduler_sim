/* ============================================================================
 * gerar_recursos.c — ferramenta de BUILD (não vai pro executável final)
 * ----------------------------------------------------------------------------
 * Le cada arquivo passado na linha de comando e escreve um .h com o
 * conteudo embutido como array de bytes, pra servidor.c poder servir a
 * pagina web sem depender de uma pasta web/ do lado do executavel --
 * requisito do professor: "um unico executavel pra clicar e rodar".
 *
 * Uso: gerar_recursos <saida.h> <arquivo1> [arquivo2 ...]
 *
 * O nome de cada recurso na tabela RECURSOS_WEB e derivado do nome do
 * arquivo: "index.html" vira o caminho "/", qualquer outro nome vira
 * "/nome.ext" (ex. "app.js" -> "/app.js").
 *
 * Roda uma vez por build (chamado pelo Makefile ANTES de compilar
 * servidor.c), gera build/recursos_web.h, e esse .h é #include'd
 * normalmente pelo resto do projeto. Não faz parte do binário final.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Deriva um identificador C valido a partir do nome do arquivo, em
 * MAIUSCULAS (ex. "estilo.css" -> "ESTILO_CSS"). */
static void identificador_de(const char *nome_arquivo, char *out, size_t out_tam)
{
    /* TODO: percorrer nome_arquivo copiando pra 'out' (respeitando
     * out_tam - 1, sempre deixando espaço pro '\0' final): cada caractere
     * alfanumérico vira toupper(c); qualquer outro caractere (ponto,
     * hífen, etc.) vira '_'. Terminar com '\0'. */
    (void)nome_arquivo;
    (void)out_tam;
    if (out_tam > 0) out[0] = '\0';
}

/* Descobre o tipo MIME a partir da extensao do arquivo. */
static const char *mime_de(const char *nome_arquivo)
{
    /* TODO: strrchr(nome_arquivo, '.') pra achar a extensão; se não achar,
     * "application/octet-stream". Comparar (strcmp) contra ".html",
     * ".css", ".js", ".svg" e devolver o MIME correspondente (ver tabela
     * abaixo); qualquer outra extensão cai no genérico.
     *   .html -> "text/html; charset=utf-8"
     *   .css  -> "text/css; charset=utf-8"
     *   .js   -> "application/javascript; charset=utf-8"
     *   .svg  -> "image/svg+xml"
     */
    (void)nome_arquivo;
    return "application/octet-stream";
}

/* Extrai só o nome do arquivo de um caminho (ignora diretorios). */
static const char *nome_base(const char *caminho)
{
    /* TODO: strrchr(caminho, '/'); se achou, devolver barra+1; senão
     * devolver o próprio 'caminho' (já era só um nome, sem diretório). */
    return caminho;
}

/* strdup não é C11 padrão (é POSIX) -- com -std=c11 estrito o compilador
 * não declara o protótipo e o retorno vira int truncado, causando
 * comportamento indefinido (foi exatamente isso que gerou um SEGFAULT
 * aqui ao testar a versão completa deste arquivo -- os warnings eram
 * "implicit declaration of function 'strdup'" e "assignment to 'char *'
 * from 'int' makes pointer from integer without a cast"). Uma cópia
 * própria evita depender de uma extensão POSIX não declarada. */
static char *duplicar_string(const char *s)
{
    /* TODO: tam = strlen(s) + 1; malloc(tam); se não for NULL,
     * memcpy(copia, s, tam). Devolver a cópia (ou NULL se malloc falhou). */
    (void)s;
    return NULL;
}

/* Le o arquivo inteiro em memoria. Devolve o tamanho em *tam, ou NULL em erro. */
static unsigned char *ler_arquivo(const char *caminho, long *tam)
{
    /* TODO:
     * 1) fopen(caminho, "rb"); se falhar, devolver NULL.
     * 2) fseek(f, 0, SEEK_END) + ftell(f) pra descobrir o tamanho;
     *    checar erro (ftell < 0) e fechar o arquivo se der problema.
     * 3) rewind(f).
     * 4) malloc(tamanho > 0 ? tamanho : 1) (evita malloc(0) em arquivo
     *    vazio, que tem comportamento implementação-definido).
     * 5) fread(buf, 1, tamanho, f); conferir que leu exatamente
     *    'tamanho' bytes, senão free+fclose e devolver NULL.
     * 6) fclose(f), *tam = tamanho, devolver buf. */
    (void)caminho;
    (void)tam;
    return NULL;
}

/* Escreve os bytes de 'dados' como uma lista "0x1a,0x2b,..." em C,
 * quebrando linha a cada 16 valores (só por legibilidade do .h gerado). */
static void escrever_bytes(FILE *saida, const unsigned char *dados, long tam)
{
    /* TODO: for (i = 0; i < tam; i++) { a cada 16 valores (i % 16 == 0)
     * escrever "\n    "; depois fprintf(saida, "0x%02x,", dados[i]); }
     * terminar com um '\n'. */
    (void)saida;
    (void)dados;
    (void)tam;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "uso: %s <saida.h> <arquivo1> [arquivo2 ...]\n", argv[0]);
        return 1;
    }

    const char *caminho_saida = argv[1];
    int n_arquivos = argc - 2;

    FILE *saida = fopen(caminho_saida, "w");
    if (saida == NULL) {
        fprintf(stderr, "erro: não consegui abrir '%s' para escrita\n", caminho_saida);
        return 1;
    }

    /* TODO (cabeçalho do .h gerado):
     * 1) escrever um comentário "gerado automaticamente, não editar" e
     *    listar os arquivos de origem (argv[2..argc-1]), pra rastrear de
     *    onde veio o conteúdo.
     * 2) escrever o guard #ifndef/#define RECURSOS_WEB_H.
     * 3) escrever a definição do struct RecursoWeb, com os campos:
     *      const char *caminho;      // ex. "/" ou "/app.js"
     *      const char *tipo_mime;
     *      const unsigned char *dados;
     *      unsigned long tamanho;
     */

    /* TODO (um vetor de bytes por arquivo de entrada):
     * Alocar (malloc) três arrays paralelos de tamanho n_arquivos:
     *   ids[i]          -> identificador C do recurso i (ex. "APP_JS")
     *   caminhos_web[i] -> caminho HTTP do recurso i (ex. "/app.js")
     *   mimes[i]        -> tipo MIME do recurso i
     * Checar falha de malloc antes de continuar.
     *
     * Para cada i em 0..n_arquivos-1:
     *   a) ler_arquivo(argv[2+i], &tamanho); se der NULL, erro e sair
     *      (lembrar de fclose(saida) antes do 'return 1').
     *   b) identificador_de(nome_base(argv[2+i]), id, sizeof(id));
     *      ids[i] = duplicar_string(id).
     *   c) caminho web: se nome_base(...) == "index.html", vira "/";
     *      senão vira "/" + nome_base(...) (snprintf). caminhos_web[i] =
     *      duplicar_string(disso).
     *   d) mimes[i] = mime_de(nome_base(...)).
     *   e) escrever no .h: "static const unsigned char RECURSO_<id>[] =
     *      {" + escrever_bytes(...) + "};" e "static const unsigned long
     *      RECURSO_<id>_LEN = <tamanho>UL;".
     *   f) free(dados) (já foi copiado pro arquivo de saída, não precisa
     *      mais em memória).
     */

    /* TODO (tabela final RECURSOS_WEB[]):
     * escrever "static const RecursoWeb RECURSOS_WEB[] = {" e, para cada
     * i, uma linha "{ caminhos_web[i], mimes[i], RECURSO_<id>,
     * RECURSO_<id>_LEN }," -- depois "};" e "static const int
     * RECURSOS_WEB_QTDE = n_arquivos;".
     * Fechar com a linha de guard do include (#endif) e um comentário
     * identificando RECURSOS_WEB_H, igual ao #ifndef do topo.
     * fclose(saida).
     * Liberar ids[i]/caminhos_web[i] de cada i, depois os três arrays.
     * fprintf(stderr, "gerado: %s (%d recurso(s))\n", ...) pra feedback
     * no terminal durante o build.
     * return 0.
     */
    (void)n_arquivos;
    fclose(saida);
    return 0;
}