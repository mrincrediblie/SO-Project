#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dirent.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>



void redirecionarParaArquivo(const char *nomeArquivo) {
    
    int arquivo = open(nomeArquivo, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (arquivo == -1) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Redireciona a saída padrão para o arquivo
    if (dup2(arquivo, STDOUT_FILENO) == -1) {
        perror("Erro ao redirecionar a saída padrão");
        exit(EXIT_FAILURE);
    }

    // Fecha o descritor de arquivo original
    
    close(arquivo);
}


int funcao(char*buffer){
    
    memset(buffer,'a',16);
    return 0;
}
void funcao_aux(int n, char*buffer){
    n ++;
    memset(buffer,'b',16);
    funcao(buffer);
}
int main(){
    char buffer[1024];
    printf("buffer:%s.\n", buffer);
    

   
    return 0;
}