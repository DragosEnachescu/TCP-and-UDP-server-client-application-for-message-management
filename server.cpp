#include <iostream>
#include "utils.h"

using namespace std;

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <port>\n";
        exit(0);
    }

    int socket_udp, socket_tcp, socket_new;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    int ret, rc;

    /* se initializeaza socketii udp si tcp */
    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket_tcp < 0, "socket_tcp\n");

    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(socket_udp < 0, "socket_udp\n");

    /* se retin datele serverului */
    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* dezactivarea algoritmului lui Nagle */
    int enable = 1;
    ret = setsockopt(socket_tcp, IPPROTO_TCP, TCP_NODELAY, (char *) &enable, sizeof(int));
    DIE(ret < 0, "Nagle\n");

    /* bind pe cele 2 porturi udp si tcp */
    ret = bind(socket_udp, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind udp\n");

    ret = bind(socket_tcp, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind tcp\n");

    /* listen la portul tcp */
    ret = listen(socket_tcp, MAX_CLIENTS);
    DIE(ret < 0, "listen tcp\n");

    /* initializam vectorul de file descriptori */
    vector<struct pollfd> fds;
    struct pollfd fd_copy;

    fd_copy.fd = STDIN_FILENO;
    fd_copy.events = POLLIN;
    fds.push_back(fd_copy);

    fd_copy.fd = socket_tcp;
    fd_copy.events = POLLIN;
    fds.push_back(fd_copy);

    fd_copy.fd = socket_udp;
    fd_copy.events = POLLIN;
    fds.push_back(fd_copy);

    map<string, vector<pair<string, uint8_t>>> client_subscriptions; /* pt cand clientii dau subscribe la anumite topicuri */
    vector<pair<string, int>> subscribed_clients; /* folosit pentru a retine socketul pt clientii conectati */
    map<string, queue<UDP_message>> sf_messages; /* folosit ca sa trimitem mesajele pt clientii cu sf = 1 */

    while (1) {
        rc = poll(&fds[0], fds.size(), -1);
        DIE(rc < 0, "poll\n");

        for (long unsigned int i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == socket_tcp) {
                    /* primim mesaj de la client TCP */

                    char client_id[CLIENT_LENGTH] = {0};  

                    socket_new = accept(socket_tcp, (struct sockaddr *) &client_addr, &client_len);
                    DIE(socket_new < 0, "accept");

                    ret = recv(socket_new, client_id, sizeof(client_id), 0);
                    DIE(ret < 0, "receive message\n");

                    client_id[strlen(client_id)] = '\0';

                    if (subscribed_clients.empty()) {
                        subscribed_clients.push_back(make_pair(string(client_id), socket_new));

                        fd_copy.fd = socket_new;
                        fd_copy.events = POLLIN;
                        fds.push_back(fd_copy);

                        cout << "New client " << client_id << " connected from " << 
                            inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << "\n";

                        char close_client[CLIENT_LENGTH] = "ok";
                            ret = send(socket_new, close_client, sizeof(close_client), 0);

                        /* se verifica daca sunt mesaje in coada si se trimit la client */

                        while (!sf_messages[string(client_id)].empty()) {
                            UDP_message message_sf = sf_messages[string(client_id)].front();

                            ret = send(socket_new, (char *) &message_sf, sizeof(UDP_message), 0);
                            DIE(ret < 0, "sent\n");

                            sf_messages[string(client_id)].pop();
                        }

                    } else {
                        int OK = 1;
                        for (auto& [id, socket] : subscribed_clients) {
                            if (id == string(client_id)) {
                                cout << "Client " << id << " already connected.\n";

                                char close_client[10] = "new";
                                ret = send(socket_new, close_client, sizeof(close_client), 0);
                                DIE(ret < 0, "failed\n");
                                OK = 0;
                            }
                        }
                        if (OK == 1) {
                            subscribed_clients.push_back(make_pair(string(client_id), socket_new));

                            cout << "New client " << client_id << " connected from " << 
                                inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << "\n";

                            fd_copy.fd = socket_new;
                            fd_copy.events = POLLIN;
                            fds.push_back(fd_copy);

                            char close_client[10] = "ok";
                            ret = send(socket_new, close_client, sizeof(close_client), 0);

                            /* se verifica daca sunt mesaje in coada si se trimit la client */

                            while (!sf_messages[string(client_id)].empty()) {
                                UDP_message message_sf = sf_messages[string(client_id)].front();

                                ret = send(socket_new, (char *) &message_sf, sizeof(UDP_message), 0);
                                DIE(ret < 0, "send\n");

                                sf_messages[string(client_id)].pop();
                            }
                        }
                    }
                } 
                else if (fds[i].fd == socket_udp) {
                    /* se primeste mesaj de la socketul UDP */

                    UDP_message message_udp;

                    char buffer[MAX_BUFFER + 1];
                    ret = recvfrom(socket_udp, buffer, 1551, 0, (struct sockaddr *) &client_addr, &client_len);
                    DIE(ret < 0, "not udp receive\n");

                    memcpy(&message_udp.topic, buffer, TOPIC_LENGTH);
                    memcpy(&message_udp.tip_date, buffer + TOPIC_LENGTH, 1);
                    memcpy(&message_udp.continut, buffer + TOPIC_LENGTH + 1, MAX_BUFFER);

                    for (const auto& [id_client, topics] : client_subscriptions) {

                        for (const auto& [topic, sf] : topics) {
                            if (topic == string(message_udp.topic)) {
                                int client_socket_udp = -1;
                                for (const auto& [id, socket] : subscribed_clients) {
                                    if (id == id_client) {
                                        client_socket_udp = socket;
                                    }
                                }

                                if (client_socket_udp != -1) {
                                    ret = send(client_socket_udp, (char *) &message_udp, sizeof(UDP_message), 0);
                                    DIE(ret < 0, "couldn't sent upd message to client\n");

                                } else if (sf == 1) {
                                        sf_messages[id_client].push(message_udp);
                                    }
                            }
                        }
                    }
                } else {
                    /* se primeste comanda de la clientul TCP */

                    CLIENT_message message_tcp;
                    memset(&message_tcp, 0, sizeof(CLIENT_message));

                    /* pentru comenzile clientului */
                    ret = recv(fds[i].fd, (char *) &message_tcp, sizeof(CLIENT_message), 0);
                    DIE(ret < 0, "failed receive command from client\n");

                    message_tcp.command[strlen(message_tcp.command)] = '\0';

                    if (ret >= 0) {
                        if (strncmp(message_tcp.command, "exit", 4) == 0) {
                            ret = recv(fds[i].fd, (char *) &message_tcp, sizeof(CLIENT_message), 0);
                            DIE(ret < 0, "recv\n");

                            subscribed_clients.erase(find(subscribed_clients.begin(), 
                                    subscribed_clients.end(), make_pair(string(message_tcp.client_id), fds[i].fd)));

                            cout << "Client " << message_tcp.client_id << " disconnected.\n";
                            
                            close(fds[i].fd);

                            fds.erase(fds.begin() + i);
                            
                        } else if (strncmp(message_tcp.command, "subscribe", 9) == 0) {
                            auto subscriptions = client_subscriptions[message_tcp.client_id];

                            int OK = 0;
                            for (auto& [topic, sf] : subscriptions) {
                                /* se verifica daca clientul e deja abonat la topic */
                                if (string(message_tcp.topic) == topic) {
                                    OK = 1;
                                    char response1[25] = "Already subscribed";
                                
                                    ret = send(fds[i].fd, response1, sizeof(response1), 0);
                                    DIE(ret < 0, "couldn't sent message to client\n");
                                }
                            }

                            if (OK == 0) {
                                /* daca nu este abonat si il adaugam in lista */
                                client_subscriptions[string(message_tcp.client_id)].push_back(make_pair(message_tcp.topic, message_tcp.sf));
                                char response2[25] = {0};
                                strcpy(response2, "subscribed");
                                
                                ret = send(fds[i].fd, response2, sizeof(response2), 0);
                                DIE(ret < 0, "couldn't sent message to client\n");
                            }
                        } else if(strncmp(message_tcp.command, "unsubscribe", 11) == 0) {
                            /* trebuie sa se verifice ca exista topicul la care clientul vrea sa se dezaboneze */
                            int K = 0;
                            auto it = client_subscriptions.find(message_tcp.client_id);
                            if (it != client_subscriptions.end()) {
                                auto& topics = it->second;
                                for (auto topic_it = topics.begin(); topic_it != topics.end(); ) {
                                    if (topic_it->first == message_tcp.topic) {
                                        topic_it = topics.erase(topic_it);
                                        K++;
                                    } else {
                                        ++topic_it;
                                    }
                                }
                                char response4[20] = "Unsubscribed from";

                                ret = send(fds[i].fd, response4, sizeof(response4), 0);
                                DIE(ret < 0, "couldn't sent the message to client\n");
                            } else {
                                char response5[20] = "Client not added\n";

                                ret = send(fds[i].fd, response5, sizeof(response5), 0);
                                DIE(ret < 0, "client not in the list\n");
                            }

                            if (K == 0) {
                                char response6[20] = "Client not subs\n";

                                ret = send(fds[i].fd, response6, sizeof(response6), 0);
                                DIE(ret < 0, "client not subscribed to topic\n");
                            }
                        } else cout << "Unrecognized command.\n";
                    }
                }
            } 
        }

        if (fds[0].revents & POLLIN) {
            /* se da comanda de la server */
            char comanda[CLIENT_LENGTH];

            memset(&comanda, 0, sizeof(comanda));
            fgets(comanda, CLIENT_LENGTH, stdin);

            if (strncmp(comanda, "exit", 4) == 0) {
                for (auto& it : subscribed_clients) {
                    UDP_message message_udp;
                    strcpy(message_udp.topic, "exit");
                    strcpy(message_udp.continut, "continut");
                    message_udp.tip_date = 3;

                    ret = send(it.second, (char *) &message_udp, sizeof(UDP_message), 0);
                    DIE(ret < 0, "sent exit message\n");

                    cout << "Client " << it.first << " disconnected.\n";
                }

                for (auto& it : subscribed_clients) {
                    close(it.second);
                }

                close(socket_udp);
                close(socket_new);
                break;
            } else cout << "Unrecognized command.\n";
        }
    }

    close(socket_udp);
    close(socket_tcp);

    fds.clear();
    client_subscriptions.clear();
    subscribed_clients.clear();
    sf_messages.clear();
    
    return 0;
}