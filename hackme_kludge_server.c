/* 
 * This file is part of the IT security lesson sample set
 * Copyright (c) 2018 Mario Lombardo.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


/* DO NOT USE THIS SERVER (even not partially) FOR PRODUCTION PURPOSE
 *
 * Its intention is to demostrate weak software only.
 *
 * Compilation:
 * gcc hackme_kludge_server.c -o hackme_kludge_server -pthread
 *
 * ...or add the -g flag for debugging ;-)
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>


#define SERVER_PORT 4711

struct client_worker_data_t
{
  int sock;
  size_t input_buffer_pos;
  char input_buffer[1024];
  int password_accepted;
  unsigned int port;
  char addr[16];
};

char *passkey = NULL;



int mk_server_socket()
{
  struct sockaddr_in server_address;
  int reuse = 1;
  int new_sock = -1;

  memset(&server_address, 0, sizeof(server_address));

  /* prepare socket descriptor */
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(SERVER_PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  /* create listener socket */
  if((new_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "could not create listen socket\n");
    return -1;
  }
  if(setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
  {
    fprintf(stderr, "setsockopt(SO_REUSEADDR) failed\n");
    return -1;
  }
#ifdef SO_REUSEPORT
  if(setsockopt(new_sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
  {
    fprintf(stderr, "setsockopt(SO_REUSEPORT) failed\n");
    return -1;
  }
#endif
  if((bind(new_sock, (struct sockaddr *)&server_address, sizeof(server_address))) < 0)
  {
    fprintf(stderr, "could not bind socket\n");
    return -1;
  }

  if(listen(new_sock, 8) < 0)
  {
    fprintf(stderr, "listen() failed\n");
    return -1;
  }

  return new_sock;
}


void *client_worker(void *data)
{
  struct client_worker_data_t *ctx = (struct client_worker_data_t*)data;
  char tmp_data[1024];
  ssize_t n;

  sprintf(tmp_data, "Hello %s:%d\n\nPlease try to hack my password: ", ctx->addr, ctx->port);
  write(ctx->sock, tmp_data, strlen(tmp_data));

  while(0 < (n = read(ctx->sock, ctx->input_buffer + ctx->input_buffer_pos, 1)))
  {
    if(strcmp(ctx->input_buffer, passkey) == 0)
    {
      ctx->password_accepted = 1;
    }
    if(strlen(ctx->input_buffer) > strlen(passkey) || ctx->input_buffer[ctx->input_buffer_pos] == '\n')
    {
      ctx->input_buffer[ctx->input_buffer_pos] = 0;
      break;
    }
    ctx->input_buffer_pos++;
  }

  if(ctx->password_accepted)
  {
    snprintf(tmp_data, sizeof(tmp_data), "Congratulations. Password '%s' accepted!\n\n", passkey);
  }
  else
  {
    snprintf(tmp_data, sizeof(tmp_data), "'%s' is wrong. Nice try fledgling...\n\n", ctx->input_buffer);
  }
  write(ctx->sock, tmp_data, strlen(tmp_data));

  /* tidy up */
  close(ctx->sock);
  free(ctx);
  return NULL;
}




void set_password(int sock)
{
  char tmp_data[256];
  char tmp_pass[256];
  char *pass_ptr;
  ssize_t n;

  sprintf(tmp_data, "No password set. Please enter a password for this service: ");
  write(sock, tmp_data, strlen(tmp_data));

  memset(tmp_pass, 0, sizeof(tmp_pass));
  n = read(sock, tmp_pass, sizeof(tmp_pass) - 1);
  pass_ptr = tmp_pass;
  /* strip non printable chars */
  while(*pass_ptr)
  {
    if(!isprint(*pass_ptr))
    {
      *pass_ptr = 0;
      break;
    }
    pass_ptr++;
  }

  sprintf(tmp_data, "Thank you. Password set to: '%s'\nHappy hacking!\n\n", tmp_pass);
  write(sock, tmp_data, strlen(tmp_data));
  close(sock);
  passkey = strdup(tmp_pass);
}




int main(int argc, char *argv[]) 
{
  (void)argc;
  (void)argv;
  struct sockaddr_in client_address;
  socklen_t client_address_len = 0;
  int listen_sock = 0, new_client = 0;
  struct client_worker_data_t *client_worker_data = NULL;
  pthread_t tid;
  pthread_attr_t attr;

  signal(SIGPIPE, SIG_IGN);

  if((listen_sock = mk_server_socket()) < 0)
  {
    return 1;
  }
  fprintf(stderr, "Listening with #%d on 0.0.0.0:%d\n", listen_sock, SERVER_PORT);

  while(1)
  {
    client_address_len = sizeof(struct sockaddr_in);
    memset(&client_address, 0, client_address_len);
    new_client = accept(listen_sock, (struct sockaddr *)&client_address, &client_address_len);

    /* check for error */
    if(new_client == -1)
    {
      fprintf(stderr, "accept() failed with error %d: %s\n", errno, strerror(errno));
      return 1;
    }

    fprintf(stderr, "New client on #%d: %s:%d\n", new_client, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

    /* let first client set secret password */
    if(!passkey)
    {
      set_password(new_client);
    }

    /* create structure with all required data */
    client_worker_data = calloc(1, sizeof(struct client_worker_data_t));
    client_worker_data->port = ntohs(client_address.sin_port);
    strcpy(client_worker_data->addr, inet_ntoa(client_address.sin_addr));
    client_worker_data->sock = new_client;

    /* start detached worker thread (pure botch!) */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, client_worker, client_worker_data);
  }

  /* tidy up*/
  if(passkey)
  {
    free(passkey);
  }
  close(listen_sock);
  return 0;
}

