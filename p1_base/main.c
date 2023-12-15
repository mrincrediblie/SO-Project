#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int MAX_PROC;
int MAX_THREADS;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// Estrutura para passar dados para a thread
struct ThreadData
{
  int file_descriptor;
  char *buffer;
};

void process_command(int file, char *buffer)
{

  while (1)
  {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    switch (get_next(file))
    {
    case CMD_CREATE:

      if (parse_create(file, &event_id, &num_rows, &num_columns) != 0)
      {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_create(event_id, num_rows, num_columns))
      {
        fprintf(stderr, "Failed to create event\n");
      }

      break;

    case CMD_RESERVE:

      num_coords = parse_reserve(file, MAX_RESERVATION_SIZE, &event_id, xs, ys);

      if (num_coords == 0)
      {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_reserve(event_id, num_coords, xs, ys))
      {
        fprintf(stderr, "Failed to reserve seats\n");
      }

      break;

    case CMD_SHOW:
      if (parse_show(file, &event_id) != 0)
      {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_show(event_id, &buffer))
      {
        fprintf(stderr, "Failed to show event\n");
      }

      break;

    case CMD_LIST_EVENTS:
      if (ems_list_events(&buffer))
      {
        fprintf(stderr, "Failed to list events\n");
      }

      break;

    case CMD_WAIT:
      if (parse_wait(file, &delay, NULL) == -1)
      { // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay > 0)
      {
        printf("Waiting...\n");
        ems_wait(delay);
      }

      break;

    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_HELP:
      printf(
          "Available commands:\n"
          "  CREATE <event_id> <num_rows> <num_columns>\n"
          "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
          "  SHOW <event_id>\n"
          "  LIST\n"
          "  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
          "  BARRIER\n"                     // Not implemented
          "  HELP\n");

      break;

    case CMD_BARRIER: // Not implemented
    case CMD_EMPTY:
      break;
    case EOC:
      return;
    }
  }
}

int show_to_file(char *buffer, char *file_name)
{

  char new_name[16];
  strcpy(new_name, file_name);
  char *extension_position = strstr(new_name, ".jobs");
  if (extension_position != NULL)
  {
    *extension_position = '\0';
  }
  strcat(new_name, ".out");

  int output_file = open(new_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
  if (output_file < 0)
  {
    fprintf(stderr, "Failed to create File\n");
  }
  pthread_mutex_lock(&buffer_mutex); // apenas um thread de cada vez pode escrever no file
  size_t len = strlen(buffer);
  int done = 0;
  while (len > 0)
  {
    ssize_t bytes_written = write(output_file, buffer + done, len);

    if (bytes_written < 0)
    {
      fprintf(stderr, "write error: %s\n", strerror(errno));
      pthread_mutex_unlock(&buffer_mutex); // Em caso de erro, libertar o mutex antes de retornar
      return -1;
    }
    len -= (size_t)bytes_written;
    done += bytes_written;
  }
  pthread_mutex_unlock(&buffer_mutex); // Liberta o acesso ao buffer

  close(output_file);
  return 0;
}

// a funcao que as threads vao executar
void *execute_command(void *data)
{
  struct ThreadData *thread_data = (struct ThreadData *)data;

  // Chamada à função process_command
  process_command(thread_data->file_descriptor, thread_data->buffer);

  // Libertar a memória alocada para a estrutura ThreadData
  free(thread_data);
  pthread_exit(NULL);
}

void process_file(char *file_path, char *file_name)
{
  char buffer[1000];
  buffer[0] = '\0';

  int fd = open(file_path, O_RDONLY);
  if (fd < 0)
  {
    fprintf(stderr, "Failed to open File\n");
    return;
  }

  pthread_t threads[MAX_THREADS];
  int thread_id = 0;

  int line_number = 0; // Variável para rastrear o número da linha
  while (1)
  {
    if (thread_id >= MAX_THREADS)
      break;

    enum Command next_command = get_next(fd);
    if (next_command == EOC)
    {
      break;
    }

    int thread_fd = open(file_path, O_RDONLY); // Abre o arquivo novamente, permite que cada thread tenha o seu próprio fd para ler o arquivo independentemente das outras threads.
    for (int i = 0; i < line_number; ++i)
    {
      char dummy[256]; // Buffer para armazenar a linha atual, mas serve apenas de "cursor" para chegar à linha certa
      read(thread_fd, dummy, sizeof(dummy));
    }

    struct ThreadData *thread_data = (struct ThreadData *)malloc(sizeof(struct ThreadData));
    thread_data->file_descriptor = thread_fd;
    thread_data->buffer = buffer;

    pthread_create(&threads[thread_id], NULL, execute_command, (void *)thread_data);

    thread_id++;
    line_number++;
  }

  for (int i = 0; i < thread_id; ++i)
  {
    pthread_join(threads[i], NULL);
  }

  show_to_file(buffer, file_name);
  close(fd);
}

void process_dir(char *dir_name)
{

  DIR *jobs_dir = opendir(dir_name);
  pid_t pid;
  if (jobs_dir == NULL)
  {
    fprintf(stderr, "Failed do open Directory");
    // return 1;
  }
  struct dirent *entry;
  int current_processes = 0;
  while ((entry = readdir(jobs_dir)) != NULL)
  {
    while (current_processes >= MAX_PROC)
    {
      // Aguarda enquanto o número máximo de processos em execução é atingido
      wait(NULL);
      current_processes--;
    }
    // printf("estou dentro de um processo\n");
    pid = fork();
    if (pid < 0)
    {
      fprintf(stderr, "Failed to create process");
      exit(1);
    }
    if (pid == 0)
    {
      if (strstr(entry->d_name, ".jobs") != NULL)
      {
        char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", "jobs", entry->d_name);

        // Processar comandos do arquivo
        process_file(file_path, entry->d_name);
        exit(0); // Certifique-se de sair após o processamento do arquivo
      }
      exit(0); // Sair caso o arquivo não seja .jobs
    }
    // Processo pai
    current_processes++;
  }

  // nao esta a entrar no loop mas esta a criar bem os processos filhos
  int status;
  pid_t child_pid;
  while ((child_pid = wait(&status)) > 0)
  {
    printf("estou no loop\n");
    if (WIFEXITED(status))
    {
      printf("Processo filho com PID %d terminou com código de saída: %d\n", child_pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
      printf("Processo filho com PID %d foi encerrado por um sinal: %d\n", child_pid, WTERMSIG(status));
    }
  }

  // Aguardar todos os processos filhos terminarem
  while (current_processes > 0)
  {
    wait(NULL);
    current_processes--;
  }
}

int main(int argc, char *argv[])
{

  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  char *dir_name = NULL;
  dir_name = (char *)malloc(strlen(argv[1]) + 1);
  strcpy(dir_name, argv[1]);
  MAX_PROC = atoi(argv[2]);
  MAX_THREADS = atoi(argv[3]);

  if (argc > 4)
  {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX)
    {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms))
  {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  process_dir(dir_name);
  ems_terminate();
  free(dir_name);
}
