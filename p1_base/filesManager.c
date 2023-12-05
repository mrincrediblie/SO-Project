#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

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
  printf("buffer:%s.\n", buffer);
  int fd = open(file_path, O_RDONLY);
  if (fd < 0)
    fprintf(stderr, "Failed to open File\n");

  process_command(fd, buffer);

  show_to_file(buffer, file_name);
  close(fd);
}

void process_dir(char* dir_name)
{

  DIR *jobs_dir = opendir(dir_name);
  if (jobs_dir == NULL)
  {
    fprintf(stderr, "Failed do open Directory");
    // return 1;
  }
  struct dirent *entry;
  while ((entry = readdir(jobs_dir)) != NULL)
  {
    if (strstr(entry->d_name, ".jobs") != NULL)
    {
      // Construir o caminho completo do arquivo
      char file_path[PATH_MAX];
      snprintf(file_path, PATH_MAX, "%s/%s", "jobs", entry->d_name);
      printf("%s\n", entry->d_name);
      // Processar comandos do arquivo
      process_file(file_path, entry->d_name);
    }
  }
}
