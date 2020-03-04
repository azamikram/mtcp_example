#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include "cpu.h"
#include "debug.h"
#include "netlib.h"
#include "rss.h"

#include "local_debug.h"

#define BUF_SIZE (1024)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#ifndef MAX_CPUS
#define MAX_CPUS 14
#endif

uint8_t junk_bytes[8] = {22,  35,  220, 105, 151, 35, 22, 25};

static pthread_t app_thread[MAX_CPUS];
static mctx_t g_mctx[MAX_CPUS];
volatile static int done[MAX_CPUS];

static int max_fds;

static in_addr_t daddr;
static in_port_t dport;
static in_addr_t saddr;

struct thread_context {
  int core;

  mctx_t mctx;
  int ep;
};
typedef struct thread_context *thread_context_t;

static struct thread_context *g_ctx[MAX_CPUS] = {0};

struct master_conf {
  char *conf_file;
  int nb_cores;
};

static struct master_conf *m_conf;

thread_context_t create_context(int core) {
  thread_context_t ctx;

  TRACE_DBG("Hello from other world!");

  ctx = (thread_context_t)calloc(1, sizeof(struct thread_context));
  if (!ctx) {
    perror("malloc");
    TRACE_ERROR("Failed to allocate memory for thread context.\n");
    return NULL;
  }
  ctx->core = core;

  ctx->mctx = mtcp_create_context(core);
  if (!ctx->mctx) {
    TRACE_ERROR("Failed to create mtcp context.\n");
    free(ctx);
    return NULL;
  }
  g_mctx[core] = ctx->mctx;

  return ctx;
}

void destroy_context(thread_context_t ctx) {
  mtcp_destroy_context(ctx->mctx);
  free(ctx);
}

static inline int create_conn(thread_context_t ctx) {
  mctx_t mctx = ctx->mctx;
  struct mtcp_epoll_event ev;
  struct sockaddr_in addr;
  int sockid;
  int ret;

  sockid = mtcp_socket(mctx, AF_INET, SOCK_STREAM, 0);
  if (sockid < 0) {
    TRACE_INFO("Failed to create socket!\n");
    return -1;
  }
  ret = mtcp_setsock_nonblock(mctx, sockid);
  if (ret < 0) {
    TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
    exit(-1);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = daddr;
  addr.sin_port = dport;

  ret = mtcp_connect(mctx, sockid, (struct sockaddr *)&addr,
                     sizeof(struct sockaddr_in));
  if (ret < 0) {
    if (errno != EINPROGRESS) {
      perror("mtcp_connect");
      mtcp_close(mctx, sockid);
      return -1;
    }
  }

  ev.events = MTCP_EPOLLOUT;
  ev.data.sockid = sockid;
  mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, sockid, &ev);

  return sockid;
}

static inline void close_conn(thread_context_t ctx, int sockid) {
  mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
  mtcp_close(ctx->mctx, sockid);
}

static inline int send_msg(thread_context_t ctx, int sockid) {
  int wr = 0;
  int len = sizeof(junk_bytes) / sizeof(uint8_t);

  wr = mtcp_write(ctx->mctx, sockid, (const char *)junk_bytes, len);
  if (wr < len) {
    TRACE_ERROR(
        "Socket %d: Sending MSG failed. "
        "try: %d, sent: %d\n", sockid, len, wr);
  }

  return wr;
}

static inline int read_event(thread_context_t ctx, int sockid) {
  mctx_t mctx = ctx->mctx;
  char buf[BUF_SIZE];
  int rd;

  rd = 1;
  while (rd > 0) {
    rd = mtcp_read(mctx, sockid, buf, BUF_SIZE);
    if (rd <= 0) break;
  }

  if (rd == 0) {
    /* connection closed by remote host */
    TRACE_DBG("Socket %d connection closed with server.\n", sockid);
    close_conn(ctx, sockid);
  } else if (rd < 0) {
    if (errno != EAGAIN) {
      TRACE_DBG("Socket %d: mtcp_read() error %s\n", sockid, strerror(errno));
      close_conn(ctx, sockid);
    }
  }
  return 0;
}

void *run_client(void *arg) {
  thread_context_t ctx;
  mctx_t mctx;
  int core = *(int *)arg;
  struct in_addr daddr_in;
  int maxevents;
  int ep;
  struct mtcp_epoll_event *events;
  int nevents;
  int i;

  mtcp_core_affinitize(core);

  ctx = create_context(core);
  if (!ctx) {
    return NULL;
  }
  mctx = ctx->mctx;
  g_ctx[core] = ctx;

  mtcp_init_rss(mctx, saddr, 1, daddr, dport);

  daddr_in.s_addr = daddr;
  fprintf(stderr, "Thread %d connecting to %s:%u\n", core, inet_ntoa(daddr_in),
          ntohs(dport));
  /* Initialization */
  maxevents = max_fds * 3;
  ep = mtcp_epoll_create(mctx, maxevents);
  if (ep < 0) {
    TRACE_ERROR("Failed to create epoll struct!\n");
    exit(EXIT_FAILURE);
  }
  events = (struct mtcp_epoll_event *)calloc(maxevents,
                                             sizeof(struct mtcp_epoll_event));
  if (!events) {
    TRACE_ERROR("Failed to allocate events!\n");
    exit(EXIT_FAILURE);
  }
  ctx->ep = ep;
  i = 1;

  printf("Going to create %d connection...\n", i);
  while (i > 0) {
    create_conn(ctx);
    i--;
  }

  printf("Starting the main loop\n");

  while (!done[core]) {
    nevents = mtcp_epoll_wait(mctx, ep, events, maxevents, -1);
    if (nevents < 0) {
      if (errno != EINTR) {
        TRACE_ERROR("mtcp_epoll_wait failed! ret: %d\n", nevents);
      }
      done[core] = TRUE;
      break;
    }

    for (i = 0; i < nevents; i++) {
      if (events[i].events & MTCP_EPOLLERR) {
        printf("[CPU %d] Error on socket %d\n", core, events[i].data.sockid);
        printf("Closing a connection\n");
        close_conn(ctx, events[i].data.sockid);
      } else if (events[i].events & MTCP_EPOLLIN) {
        read_event(ctx, events[i].data.sockid);
      } else if (events[i].events & MTCP_EPOLLOUT) {
        send_msg(ctx, events[i].data.sockid);
      }
    }
  }
  TRACE_INFO("Client thread %d waiting for mtcp to be destroyed.\n", core);
  destroy_context(ctx);
  TRACE_DBG("Client thread %d finished.\n", core);
  pthread_exit(NULL);
  return NULL;
}

void signal_handler(int signum) {
  int i;

  for (i = 0; i < m_conf->nb_cores; i++) {
    done[i] = TRUE;
  }
}

static int parse_args(int argc, char **argv) {
  struct mtcp_conf mcfg;
  int max_cores, o;

  max_cores = GetNumCPUs();
  m_conf->nb_cores = max_cores;

  while (-1 != (o = getopt(argc, argv, "N:f:"))) {
    switch (o) {
      case 'N':
        m_conf->nb_cores = mystrtol(optarg, 10);
        if (m_conf->nb_cores > max_cores) {
          TRACE_ERROR(
              "CPU limit should be smaller than the "
              "number of CPUS: %d\n",
              max_cores);
          return ERROR;
        } else if (m_conf->nb_cores < 1) {
          TRACE_ERROR("CPU limit should be greater than 0\n");
          return ERROR;
        }

        mtcp_getconf(&mcfg);
        mcfg.num_cores = m_conf->nb_cores;
        max_fds = mcfg.max_concurrency;
        mtcp_setconf(&mcfg);
        break;
      case 'f':
        m_conf->conf_file = optarg;
        break;
    }
  }

  printf("Sucessful return\n");
  return TRUE;
}

int main(int argc, char **argv) {
  int ret, i;

  m_conf = malloc(sizeof(struct master_conf));
  memset(m_conf, 0, sizeof(struct master_conf));

  daddr = inet_addr("192.168.0.10");
  dport = htons(8877);
  saddr = INADDR_ANY;

  ret = parse_args(argc, argv);
  if (ret == ERROR) {
    TRACE_ERROR("Unable to parse arguments!\n");
    exit(EXIT_FAILURE);
  }
  printf("Successful parsed arguments!\n");

  if (m_conf->conf_file == NULL) {
    TRACE_ERROR("mTCP configuration file is not set!\n");
    exit(EXIT_FAILURE);
  }

  ret = mtcp_init(m_conf->conf_file);
  if (ret) {
    TRACE_ERROR("Failed to initialize mtcp.\n");
    exit(EXIT_FAILURE);
  }
  mtcp_register_signal(SIGINT, signal_handler);

  for (i = 0; i < m_conf->nb_cores; i++) {
    done[i] = FALSE;

    if (pthread_create(&app_thread[i], NULL, run_client, (void *)&i)) {
      perror("pthread_create");
      TRACE_ERROR("Failed to create client thread.\n");
      exit(-1);
    }
  }

  for (i = 0; i < m_conf->nb_cores; i++) {
    pthread_join(app_thread[i], NULL);
    TRACE_INFO("client thread %d joined.\n", i);
  }

  mtcp_destroy();
  return 0;
}
