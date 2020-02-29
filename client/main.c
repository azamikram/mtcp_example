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

uint8_t junk_bytes[1024] = {
    22,  35,  220, 105, 151, 35,  14,  95,  204, 126, 156, 191, 158, 182, 109,
    170, 140, 180, 242, 211, 180, 85,  51,  212, 81,  234, 209, 140, 25,  175,
    25,  59,  152, 0,   64,  151, 8,   13,  62,  69,  114, 233, 170, 138, 12,
    150, 52,  214, 130, 245, 209, 254, 15,  119, 202, 240, 248, 205, 132, 182,
    191, 114, 135, 231, 139, 4,   40,  65,  32,  114, 11,  243, 251, 101, 27,
    73,  225, 152, 227, 160, 219, 192, 72,  130, 78,  99,  139, 161, 38,  235,
    210, 79,  133, 191, 62,  197, 213, 94,  238, 217, 244, 153, 165, 199, 154,
    73,  40,  193, 254, 100, 63,  153, 207, 79,  102, 250, 80,  234, 14,  49,
    101, 58,  49,  254, 197, 35,  69,  128, 76,  59,  197, 166, 235, 185, 183,
    60,  12,  206, 49,  209, 190, 125, 175, 82,  209, 180, 147, 127, 221, 166,
    122, 186, 251, 155, 184, 167, 100, 11,  171, 137, 58,  12,  160, 52,  173,
    26,  104, 192, 123, 153, 181, 176, 73,  134, 109, 191, 87,  162, 124, 232,
    174, 101, 40,  254, 112, 234, 110, 149, 164, 243, 84,  7,   87,  156, 141,
    196, 196, 175, 168, 32,  72,  206, 105, 90,  72,  102, 41,  83,  125, 171,
    43,  218, 136, 97,  28,  236, 209, 162, 125, 88,  228, 144, 118, 176, 241,
    122, 209, 205, 33,  224, 171, 119, 54,  206, 12,  227, 67,  29,  242, 146,
    200, 141, 47,  191, 7,   29,  152, 69,  207, 1,   159, 128, 240, 184, 224,
    102, 216, 83,  167, 172, 239, 78,  113, 45,  77,  225, 176, 7,   114, 49,
    248, 38,  15,  169, 161, 36,  53,  218, 135, 149, 13,  143, 121, 220, 153,
    204, 173, 13,  13,  38,  233, 107, 133, 153, 18,  204, 28,  3,   89,  157,
    64,  140, 201, 239, 180, 187, 158, 121, 157, 27,  57,  175, 78,  168, 182,
    74,  6,   36,  211, 231, 31,  114, 86,  134, 129, 78,  203, 58,  78,  232,
    252, 54,  236, 118, 207, 112, 203, 187, 128, 24,  221, 164, 174, 52,  201,
    252, 28,  224, 221, 242, 239, 86,  109, 198, 68,  30,  205, 152, 126, 220,
    206, 202, 133, 153, 232, 158, 105, 229, 23,  189, 18,  193, 255, 92,  225,
    85,  199, 23,  43,  23,  38,  115, 245, 156, 18,  143, 98,  254, 163, 253,
    207, 218, 139, 174, 24,  149, 241, 145, 225, 128, 193, 159, 96,  115, 253,
    208, 38,  195, 205, 94,  145, 204, 183, 16,  204, 240, 1,   80,  97,  215,
    228, 130, 208, 212, 112, 209, 233, 100, 67,  90,  252, 164, 7,   175, 49,
    139, 60,  221, 32,  14,  139, 46,  229, 172, 193, 44,  29,  136, 128, 146,
    148, 31,  210, 96,  164, 67,  143, 247, 198, 16,  96,  49,  28,  74,  142,
    36,  32,  143, 40,  84,  51,  110, 168, 219, 85,  98,  190, 208, 143, 48,
    162, 245, 86,  74,  60,  82,  95,  137, 134, 124, 117, 123, 197, 26,  144,
    136, 45,  1,   237, 108, 4,   248, 39,  181, 116, 37,  236, 111, 206, 52,
    86,  40,  109, 57,  197, 136, 124, 205, 218, 244, 182, 115, 96,  233, 233,
    144, 205, 115, 27,  6,   68,  5,   254, 225, 138, 158, 46,  31,  141, 243,
    85,  8,   55,  4,   108, 111, 51,  40,  40,  243, 136, 203, 194, 151, 238,
    69,  252, 176, 86,  46,  225, 216, 230, 1,   28,  185, 17,  158, 135, 138,
    164, 16,  153, 139, 158, 62,  19,  85,  9,   140, 21,  246, 243, 20,  166,
    117, 89,  101, 18,  97,  79,  232, 146, 239, 37,  74,  217, 234, 18,  152,
    183, 75,  217, 150, 20,  189, 203, 133, 135, 7,   6,   160, 8,   77,  34,
    220, 43,  92,  211, 215, 69,  195, 113, 213, 148, 107, 50,  88,  99,  40,
    128, 161, 144, 80,  164, 252, 93,  191, 98,  145, 40,  214, 240, 12,  148,
    208, 28,  3,   74,  140, 55,  173, 17,  11,  223, 170, 202, 182, 30,  119,
    225, 101, 136, 115, 79,  216, 151, 86,  206, 157, 239, 9,   231, 164, 96,
    38,  246, 43,  130, 242, 16,  84,  35,  50,  186, 7,   179, 99,  17,  115,
    161, 86,  209, 18,  74,  44,  92,  140, 109, 54,  12,  16,  143, 235, 49,
    140, 121, 241, 111, 253, 12,  59,  140, 63,  37,  0,   169, 77,  73,  22,
    27,  98,  250, 168, 106, 240, 86,  211, 44,  10,  23,  116, 76,  71,  247,
    148, 239, 14,  80,  38,  249, 245, 236, 245, 180, 20,  27,  92,  179, 30,
    158, 186, 207, 199, 248, 174, 212, 176, 219, 192, 146, 94,  48,  176, 121,
    146, 241, 159, 26,  4,   97,  66,  216, 73,  196, 170, 197, 186, 223, 61,
    44,  53,  161, 102, 46,  175, 146, 191, 90,  74,  44,  68,  245, 79,  3,
    156, 71,  179, 73,  11,  65,  141, 118, 225, 192, 237, 147, 206, 109, 97,
    15,  103, 50,  170, 152, 117, 84,  19,  22,  61,  235, 151, 193, 236, 49,
    244, 202, 147, 113, 177, 197, 211, 100, 131, 40,  217, 159, 206, 66,  4,
    141, 37,  99,  75,  234, 145, 61,  102, 130, 230, 232, 170, 244, 192, 192,
    50,  4,   231, 79,  90,  63,  185, 161, 18,  55,  121, 22,  36,  136, 41,
    33,  169, 223, 217, 86,  239, 199, 165, 6,   146, 18,  91,  97,  150, 134,
    95,  227, 82,  116, 24,  47,  88,  11,  205, 29,  224, 186, 136, 189, 70,
    138, 227, 129, 185, 208, 217, 16,  221, 24,  118, 71,  147, 8,   41,  87,
    226, 136, 215, 0,   97,  161, 239, 237, 179, 142, 229, 155, 122, 119, 232,
    240, 96,  254, 28,  49,  91,  29,  213, 67,  82,  204, 160, 188, 196, 217,
    74,  223, 151, 63,  56,  157, 136, 142, 22,  55,  114, 137, 126, 157, 185,
    120, 60,  121, 212, 139, 202, 189, 126, 74,  254, 175, 86,  16,  57,  54,
    34,  237, 102, 157, 206, 210, 48,  149, 51,  168, 55,  29,  131, 254, 23,
    49,  11,  66,  253, 227, 236, 209, 125, 219, 231, 227, 175, 133, 221, 131,
    61,  75,  12,  224, 215, 24,  3,   173, 30,  140, 245, 134, 163, 154, 242,
    103, 144, 225, 56};
/*----------------------------------------------------------------------------*/
static pthread_t app_thread[MAX_CPUS];
static mctx_t g_mctx[MAX_CPUS];
static int done[MAX_CPUS];
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
static int max_fds;
/*----------------------------------------------------------------------------*/
static in_addr_t daddr;
static in_port_t dport;
static in_addr_t saddr;
/*----------------------------------------------------------------------------*/
struct thread_context {
  int core;

  mctx_t mctx;
  int ep;
};
typedef struct thread_context *thread_context_t;
/*----------------------------------------------------------------------------*/
static struct thread_context *g_ctx[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
void destroy_context(thread_context_t ctx) {
  mtcp_destroy_context(ctx->mctx);
  free(ctx);
}
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
static inline void close_conn(thread_context_t ctx, int sockid) {
  mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
  mtcp_close(ctx->mctx, sockid);
}
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
void signal_handler(int signum) {
  int i;

  for (i = 0; i < core_limit; i++) {
    done[i] = TRUE;
  }
}
/*----------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  struct mtcp_conf mcfg;
  char *conf_file;
  int cores[MAX_CPUS];
  int ret;
  int i, o;
  int process_cpu;

  conf_file = NULL;
  process_cpu = -1;
  daddr = inet_addr("192.168.0.10");
  dport = htons(8877);
  saddr = INADDR_ANY;

  num_cores = GetNumCPUs();
  core_limit = num_cores;

  while (-1 != (o = getopt(argc, argv, "N:f:"))) {
    switch (o) {
      case 'N':
        core_limit = mystrtol(optarg, 10);
        if (core_limit > num_cores) {
          TRACE_CONFIG(
              "CPU limit should be smaller than the "
              "number of CPUS: %d\n",
              num_cores);
          return FALSE;
        } else if (core_limit < 1) {
          TRACE_CONFIG("CPU limit should be greater than 0\n");
          return FALSE;
        }
        /**
         * it is important that core limit is set
         * before mtcp_init() is called. You can
         * not set core_limit after mtcp_init()
         */
        mtcp_getconf(&mcfg);
        mcfg.num_cores = core_limit;
        max_fds = mcfg.max_concurrency;
        mtcp_setconf(&mcfg);
        break;
      case 'f':
        conf_file = optarg;
        break;
    }
  }

  if (conf_file == NULL) {
    TRACE_ERROR("mTCP configuration file is not set!\n");
    exit(EXIT_FAILURE);
  }

  ret = mtcp_init(conf_file);
  if (ret) {
    TRACE_ERROR("Failed to initialize mtcp.\n");
    exit(EXIT_FAILURE);
  }
  mtcp_register_signal(SIGINT, signal_handler);

  for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
    cores[i] = i;
    done[i] = FALSE;

    if (pthread_create(&app_thread[i], NULL, run_client, (void *)&cores[i])) {
      perror("pthread_create");
      TRACE_ERROR("Failed to create wget thread.\n");
      exit(-1);
    }

    if (process_cpu != -1) break;
  }

  for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
    pthread_join(app_thread[i], NULL);
    TRACE_INFO("Wget thread %d joined.\n", i);

    if (process_cpu != -1) break;
  }

  mtcp_destroy();
  return 0;
}
/*----------------------------------------------------------------------------*/
