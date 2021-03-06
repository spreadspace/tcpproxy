/*
 *  tcpproxy
 *
 *  tcpproxy is a simple tcp connection proxy which combines the
 *  features of rinetd and 6tunnel. tcpproxy supports IPv4 and
 *  IPv6 and also supports connections from IPv6 to IPv4
 *  endpoints and vice versa.
 *
 *
 *  Copyright (C) 2010-2015 Christian Pointner <equinox@spreadspace.org>
 *
 *  This file is part of tcpproxy.
 *
 *  tcpproxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  tcpproxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with tcpproxy. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  if(argc < 3) {
    fprintf(stderr, "Usage: %s <port> (A|B)\n", argv[0]);
    return -1;
  }

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(!s) {
    perror("socket()");
    return -1;
  }

  int on = 1;
  int ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if(ret) {
    perror("setsockopt(SO_REUSEADDR)");
    return -1;
  }

  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  laddr.sin_port = htons(atoi(argv[1]));
  laddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ret = bind(s, (struct sockaddr*)(&laddr), sizeof(struct sockaddr_in));
  if(ret) {
    perror("bind()");
    return -1;
  }

  ret = listen(s, 1);
  if(ret) {
    perror("listen()");
    return -1;
  }

  printf("MODE: %c, listening...\n", toupper(argv[2][0]));

  struct sockaddr_in caddr;
  socklen_t len = sizeof(struct sockaddr_in);
  int c = accept(s, (struct sockaddr*)(&caddr), &len);
  if(c < 0) {
    perror("accept()");
    return -1;
  }
  char addr_str[64];
  if(!inet_ntop(AF_INET, (&caddr.sin_addr), addr_str, sizeof(addr_str))) {
    perror("inet_ntop");
    return -1;
  }
  printf("connection from %s:%d\n", addr_str, caddr.sin_port);


  switch(toupper(argv[2][0])) {
/*************** MODE A ***************/
  case 'A': {
    char buf[10000];
    int rtot = 0;
    for(;;) {
      int nbread = recv(c, buf, sizeof(buf), 0);
      if(nbread <= 0) {
        if(!nbread) {
          fprintf(stderr, "connection closed\n");
          sleep(1);
          return 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
          perror("recv()");
          return -1;
        }
        continue;
      }

      rtot += nbread;
      printf("%d bytes received, total = %d\n", nbread, rtot);

      int len = 0;
      for(;;) {
        int nbwritten = send(c, &(buf[len]), nbread - len, 0);
        if(nbwritten <= 0) {
          if(nbwritten < 0 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            perror("send()");
            return -1;
          }
          continue;
        }
        printf("%d bytes sent\n", nbwritten);
        len += nbwritten;
        if(len >= nbread) {
          break;
        }
      }
    }
    break;
  }
/*************** MODE A ***************/
  case 'B': {
    char buf[1234567];
    int rtot = 0;
    for(;;) {
      int nbread = recv(c, &(buf[rtot]), sizeof(buf) - rtot, 0);
      if(nbread <= 0) {
        if(!nbread) {
          fprintf(stderr, "connection closed\n");
          return 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
          perror("recv()");
          return -1;
        }
        continue;
      }

      rtot += nbread;
      printf("%d bytes received, total = %d\n", nbread, rtot);
      unsigned char* end = memchr(buf, '\n', rtot);
      if(!end) {
        if(rtot >= sizeof(buf)) {
          printf("Request too big...\n");
          return -2;
        }
      } else {
        *end = 0;
        printf("Got Request: '%s'\n", buf);
        break;
      }
    }

    unsigned int i;
    for(i = 0; i<sizeof(buf); ++i) {
      buf[i] = 'A' + i%62;
    }
    buf[sizeof(buf)-1] = '\n';

    int wtot = 0;
    for(;;) {
      int nbwritten = send(c, &(buf[wtot]), sizeof(buf) - wtot, 0);
      if(nbwritten <= 0) {
        if(nbwritten < 0)
          perror("send()");
        else
          fprintf(stderr, "nothing sent... aborting\n");
        return -1;
      }
      wtot += nbwritten;
      printf("%d bytes sent, total=%d\n", nbwritten, wtot);
      if(wtot >= sizeof(buf)) {
        break;
      }
    }
    shutdown(c, SHUT_WR);
    while(recv(c, buf, 1, 0) > 0);

    break;
  }
/**************************************/
  }

  return 0;
}
