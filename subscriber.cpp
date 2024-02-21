#include <iostream>
#include "utils.h"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 4) {
        cout << "Usage: " << argv[0] << " <ip> <port>\n";
        return 0;
    }

    int ret, rc;

    int socket_tcp;

    /* initializam socketul tcp */
    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket_tcp < 0, "socket tcp\n");

    /* serverul */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));

    ret = inet_aton(argv[2], &server_addr.sin_addr);
    DIE(ret < 0, "server address\n");

    int enable = 1;
    ret = setsockopt(socket_tcp, IPPROTO_TCP, TCP_NODELAY, (char *) &enable, sizeof(int));
    DIE(ret < 0, "Nagle\n");

    /* se conecteaza clientul la server */
    ret = connect(socket_tcp, (struct sockaddr*) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "connect tcp\n");

    /* se trimite id-ul clientului */
    char id[CLIENT_LENGTH] = {0};
    strcpy(id, argv[1]);
    id[strlen(id)] = '\0';
    ret = send(socket_tcp, id, sizeof(id), 0);
    DIE(ret < 0, "send\n");

    char connection[10];
    ret = recv(socket_tcp, connection, sizeof(connection), 0);
    DIE(ret < 0, "failed connection\n");

    if (strncmp(connection, "new", 5) != 0) {
        vector<struct pollfd> fds;
        struct pollfd fd_copy;

        fd_copy.fd = STDIN_FILENO;
        fd_copy.events = POLLIN;
        fds.push_back(fd_copy);

        fd_copy.fd = socket_tcp;
        fd_copy.events = POLLIN;
        fds.push_back(fd_copy);

        while (1) {
            rc = poll(&fds[0], fds.size(), -1);
            DIE(rc < 0, "poll\n");

            if (fds[0].revents & POLLIN) {
                /* clientul da o comanda */

                char command[COMMAND_SIZE];

                CLIENT_message message_client;
                memset(&message_client, 0, sizeof(CLIENT_message));

                memset(&command, 0, sizeof(command));

                fgets(command, COMMAND_SIZE, stdin);

                char* token = strtok(command, " "); /* comanda */

                if (strncmp(command, "exit", 4) == 0) {
                    strcpy(message_client.client_id, argv[1]);
                    strcpy(message_client.topic, "orice topic");
                    message_client.sf = 2; /* orice in afara de 0 sau 1 */
                    strcpy(message_client.command, "exit");

                    ret = send(socket_tcp, (char *) &message_client, sizeof(CLIENT_message), 0);
                    DIE(ret < 0, "sent exit command\n");

                    close(socket_tcp);
                    return 0;
                } else if (strncmp(command, "subscribe", 9) == 0) {
                    strcpy(message_client.command, "subscribe");
                    strcpy(message_client.client_id, argv[1]);
                    
                    token = strtok(NULL, " ");

                    strcpy(message_client.topic, token);

                    token = strtok(NULL, " ");
                    message_client.sf = atoi(token);
                    
                    ret = send(socket_tcp, (char *) &message_client, sizeof(CLIENT_message), 0);
                    DIE(ret < 0, "cannot send data to server\n");

                    char response_from_server[25] = {0};

                    ret = recv(socket_tcp, response_from_server, sizeof(response_from_server), 0);
                    DIE(ret < 0, "couldn't receive\n");

                    if (strncmp(response_from_server, "subscribed", 10) == 0) {
                        cout << "Subscribed to topic.\n";
                    } else {
                        cout << "Already subscribed to this topic.\n";
                    }

                } else if (strncmp(command, "unsubscribe", 11) == 0) {
                    strcpy(message_client.command, "unsubscribe");
                    strcpy(message_client.client_id, argv[1]);

                    token = strtok(NULL, " ");
                    strcpy(message_client.topic, token);
                    message_client.sf = 2;

                    ret = send(socket_tcp, (char *) &message_client, sizeof(CLIENT_message), 0);
                    DIE(ret < 0, "cannot send data to server\n");

                    char response_from_server2[20] = {0};

                    ret = recv(socket_tcp, response_from_server2, sizeof(response_from_server2), 0);
                    DIE(ret < 0, "couldn't reveive\n");

                    if (strncmp(response_from_server2, "Unsubscribed from", 17) == 0) {
                        cout << "Unsubscribed from topic.\n";
                    } else {
                        cout << "Client not subscribed to this topic.\n";
                    }
                }
            } 
            else if (fds[1].revents & POLLIN) {
                /* se primeste mesaj de la UDP */

                UDP_message message_udp;
                memset(&message_udp, 0, sizeof(UDP_message));

                ret = recv(socket_tcp, (char *) &message_udp, sizeof(UDP_message), 0);
                DIE(ret < 0, "error\n");

                if (strncmp(message_udp.topic, "exit", 4) == 0) {
                    return 0;
                } else {
                    cout << message_udp.topic;

                    if (message_udp.tip_date == 0) {
                        cout << " - INT - ";
                        uint32_t valoare;

                        memcpy(&valoare, message_udp.continut + 1, sizeof(uint32_t));

                        switch (message_udp.continut[0]) {
                            case 1:
                                cout << "-" << ntohl(valoare) << "\n";
                                break;

                            default:
                                cout << ntohl(valoare) << "\n";
                                break;
                        }
                    } else if (message_udp.tip_date == 1) {

                        cout << " - SHORT_REAL - ";
                        uint16_t short_real;
                        memcpy(&short_real, message_udp.continut, sizeof(uint16_t));

                        cout << fixed << setprecision(2) << float(ntohs(short_real)) / 100 << "\n";
                    } else if (message_udp.tip_date == 2) {

                        cout << " - FLOAT - ";
                        uint32_t number;
                        memcpy(&number, message_udp.continut + 1, sizeof(uint32_t));

                        uint8_t power = message_udp.continut[5];

                        int pow = 1;
                        int power_copy = power;
                        for (int i = 0; i < power; i++) {
                            pow = pow * 10;
                        }

                        if (message_udp.continut[0] == 1) {
                            cout << "-";
                        }

                        cout << fixed << setprecision(power_copy) << float(ntohl(number)) / pow << "\n";
                    } else {
                        cout << " - STRING - " << message_udp.continut << "\n";
                    }
                }
            }
        }
        fds.clear();
    }

    close(socket_tcp);

    return 0;
}