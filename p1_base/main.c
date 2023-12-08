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

      if (ems_show(event_id, buffer))
      {
        fprintf(stderr, "Failed to show event\n");
      }

      break;

    case CMD_LIST_EVENTS:
      if (ems_list_events())
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

      // ems_terminate(); depois tenho que ver uma maneira dele nao chamar ems terminate sempre que le um ficheiro
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
  size_t len = strlen(buffer);
  int done = 0;
  while (len > 0)
  {
    ssize_t bytes_written = write(output_file, buffer + done, len);

    if (bytes_written < 0)
    {
      fprintf(stderr, "write error: %s\n", strerror(errno));
      return -1;
    }
    len -= (size_t)bytes_written;
    done += bytes_written;
  }

  close(output_file);
  return 0;
}

void process_file(char *file_path, char *file_name)
{
  char buffer[1000];
  buffer[0] = '\0';

  int fd = open(file_path, O_RDONLY);
  if (fd < 0)
    fprintf(stderr, "Failed to open File\n");

  process_command(fd, buffer);

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
    printf("estou dentro de um processo\n");
    pid = fork();
    if (pid < 0)
    {
      fprintf(stderr, "Failed to create process");
      exit(1);
    }
    if (pid == 0)
    {
      printf("[%d] [%d] i=%s\n", getppid(), getpid(), entry->d_name);
      if (strstr(entry->d_name, ".jobs") != NULL)
      {
        // Construir o caminho completo do arquivo
        char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", "jobs", entry->d_name);

        // Processar comandos do arquivo
        process_file(file_path, entry->d_name);
        break;
      }
    }
    else {
            // Processo pai
      current_processes++;
    }
  }
  if (pid > 0)
  {
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
  }
}

int main(int argc, char *argv[])
{

  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  char *dir_name = NULL;
  dir_name = (char *)malloc(strlen(argv[1]) + 1);
  strcpy(dir_name, argv[1]);
  MAX_PROC = atoi(argv[2]);
  if (argc > 3)
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

  free(dir_name);
  /*
  if (ems_terminate())
  {
    fprintf(stderr, "Failed to terminate EMS\n");
    return 1;
  }
  */
}
