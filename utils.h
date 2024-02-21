#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <netinet/tcp.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <queue>

#define MAX_BUFFER 1550
#define MAX_CLIENTS 1500
#define TOPIC_LENGTH 50
#define CLIENT_LENGTH 10
#define COMMAND_SIZE 100

typedef struct {
  char topic[TOPIC_LENGTH];
  uint8_t tip_date;
  char continut[MAX_BUFFER];
} UDP_message;

typedef struct {
  char command[TOPIC_LENGTH];
  char topic[TOPIC_LENGTH];
  uint8_t sf;
  char client_id[CLIENT_LENGTH];
} CLIENT_message;