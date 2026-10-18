#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

char *content_type(char *filename) {
  char *dot = strrchr(filename, '.');
  if (dot == NULL)
    return "text/plain";
  if (strcmp(dot, ".html") == 0)
    return "text/html";
  if (strcmp(dot, ".css") == 0)
    return "text/css";
  if (strcmp(dot, ".png") == 0)
    return "image/png";
  if (strcmp(dot, ".jpg") == 0)
    return "image/jpg";
  if (strcmp(dot, ".jpeg") == 0)
    return "image/jpeg";
  return "text/plain";
}

int main(void) {
  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    exit(1);
  }

  if (listen(server, 10) < 0) {
    perror("listen");
    exit(1);
  }

  printf("listening on port :8080\n");
  while (1) {
    int client = accept(server, NULL, NULL);
    if (client < 0) {
      perror("accept");
      exit(1);
    }

    char request[4096] = {0};
    ssize_t n = read(client, request, sizeof(request) - 1);
    if (n <= 0) {
      close(client);
      continue;
    }

    request[n] = '\0';

    char method[8], path[256];
    if (sscanf(request, "%7s %255s", method, path) != 2) {
      close(client);
      continue;
    }

    if (strstr(path, "..") != NULL) {
      const char *forbidden = "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                              "9\r\nConnection: close\r\n\r\nForbidden";
      write(client, forbidden, strlen(forbidden));
      close(client);
      continue;
    }

    char filepath[512];
    if (strcmp(path, "/") == 0) {
      snprintf(filepath, sizeof(filepath), "www/index.html");
    } else {
      snprintf(filepath, sizeof(filepath), "www%s", path);
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
      const char *not_found = "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 22\r\n"
                              "Connection: close\r\n\r\n"
                              "<h1>404 Not Found</h1>";
      write(client, not_found, strlen(not_found));
    } else {
      fseek(file, 0, SEEK_END);
      long filesize = ftell(file);
      rewind(file);

      char header[512];
      snprintf(header, sizeof(header),
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: %s\r\n"
               "Content-Length: %ld\r\n"
               "Connection: close\r\n\r\n",
               content_type(filepath), filesize);
      write(client, header, strlen(header));

      char file_buf[4096];
      size_t bytes_read;
      while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
        write(client, file_buf, bytes_read);
      }
      fclose(file);
    }

    printf("%s %s\n", method, path);

    const char *body = "<h1>Welcome to my server.</h1>\n";
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             strlen(body), body);

    printf("client fd %d\n", client);
    write(client, response, strlen(response));
    close(client);
  }

  return 0;
}
