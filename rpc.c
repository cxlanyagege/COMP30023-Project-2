#define _POSIX_C_SOURCE 200112L
#define RPC_FIND 1
#define RPC_CALL 2

#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "rpc.h"

struct rpc_server {
    /* Add variable(s) for server state */
    int port;                           // Server port
    int socket_fd;                      // Server socket
    int socket_opt;                     // Socket option
    int handler_size;                   // Handler numbers
    char **handler_name;                // Handler names array
    rpc_handler *handler;               // Handler function array
    struct addrinfo hint, *res, *rp;    // Address storing infomation
};

char *rpc_data_compose(int type, rpc_data *data);
rpc_data *rpc_data_decompose(char *data);

rpc_server *rpc_init_server(int port) {
    /* Allocate memory to server */
    rpc_server *server = malloc(sizeof(rpc_server));

    /* Check if server port is valid */
    if (port <= 0) {
        return NULL;
    } else {
        server->port = port;
    }

    /* Get address informations */
    server->socket_fd = 0;
    server->socket_opt = 1;
    memset(&server->hint, 0, sizeof(server->hint));
    server->hint.ai_family = AF_INET6;
    server->hint.ai_socktype = SOCK_STREAM;
    server->hint.ai_flags = AI_PASSIVE;

    /* Create socket for server */
    char port_buff[11];
    sprintf(port_buff, "%d", port);
    int ret = getaddrinfo(NULL, 
                          port_buff, 
                          &server->hint, 
                          &server->res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    }
    server->socket_fd = socket(server->res->ai_family, 
                               server->res->ai_socktype, 
                               server->res->ai_protocol);

    /* Set socket options */
    setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, 
              &server->socket_opt, sizeof(server->socket_opt));

    /* Initialize RPC handler */
    server->handler_size = 0;
    server->handler_name = NULL;
    server->handler = NULL;

    return server;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    /* Check if arguments are valid */
    if (srv == NULL || name == NULL || handler == NULL) {
        return -1;
    }

    /* Check if there are functions with same name */
    for (int i = 0; i < srv->handler_size; i++) {
        if (!strcmp(srv->handler_name[i], name)) {
            srv->handler[i] = handler;
            return i;
        }
    }

    /* Allocate memory to store new handler function */
    srv->handler_size++;
    srv->handler_name = realloc(srv->handler_name, 
                                srv->handler_size * sizeof(char *));
    srv->handler = realloc(srv->handler, 
                           srv->handler_size * sizeof(rpc_handler));
    
    /* Store new handler function */
    int index = srv->handler_size - 1;
    srv->handler_name[index] = strdup(name);
    srv->handler[index] = handler;

    return index;
}

void rpc_serve_all(rpc_server *srv) {
    /* Bind socket to listen client */
    if (bind(srv->socket_fd, 
             srv->res->ai_addr, 
             srv->res->ai_addrlen)) {
                printf("Bind failed: %s\n", strerror(errno));
    }
    listen(srv->socket_fd, 10);

    /* Wait until query coming in */
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int conn_fd = accept(srv->socket_fd, 
                            (struct sockaddr*)&client_addr, 
                            &client_addr_size);

        /* Error query, continue waiting */
        if (conn_fd < 0) {
            continue;
        }

        /* Read request from client */
        char recv_buff[1024];
        int byte_len = read(conn_fd, 
                            recv_buff, 
                            sizeof(recv_buff) - 1);

        /* Check if reading is completed */
        if (byte_len < 0) {
            close(conn_fd);
            continue;
        }

        /* Determine specific RPC call feature */
        /* When client asks for finding function */
        if (recv_buff[0] == RPC_FIND) {

            /* Traverse every function names */
            int i;
            for (i = 0; i < srv->handler_size; i++) {

                /* Do char by char comparison */
                int j;
                for (j = 1; j < byte_len; j++) {

                    /* Mismatch detected, compare next one */
                    if (srv->handler_name[i][j - 1] != recv_buff[j]) {
                        break;
                    }
                }

                /* No matched handler function */
                if (j == byte_len) {
                    char send_buff[1024];
                    send_buff[0] = RPC_FIND;
                    send_buff[1] = 1;
                    write(conn_fd, send_buff, strlen(send_buff)); 
                    close(conn_fd);
                    break;
                }
            }

            /* Matched result, reform client */
            if (i == srv->handler_size) {
                char send_buff[1024];
                send_buff[0] = RPC_FIND;
                send_buff[1] = 0;
                write(conn_fd, send_buff, strlen(send_buff)); 
                close(conn_fd);
            }
        } 
        
        /* When client asks to run function */
        else if (recv_buff[0] == RPC_CALL) {

        }
    }
}

struct rpc_client {
    /* Add variable(s) for client state */
    int port;                           // Server port
    int socket_fd;                      // Server socket
    int handler_size;                   // Handler numbers
    char **handler_name;                // Handler names array
    rpc_handler *handler;               // Handler function array
    struct addrinfo hint, *res, *rp;    // Address storing infomation
};

struct rpc_handle {
    /* Add variable(s) for handle */
};

rpc_client *rpc_init_client(char *addr, int port) {
    /* Allocate memory to client */
    rpc_client *client = malloc(sizeof(rpc_client));

    /* Check if client port is valid */
    if (port <= 0) {
        return NULL;
    } else {
        client->port = port;
    }

    /* Get address informations */
    client->socket_fd = 0;
    memset(&client->hint, 0, sizeof(client->hint));
    client->hint.ai_family = AF_INET6;
    client->hint.ai_socktype = SOCK_STREAM;

    /* Specify address and port */
    char port_buffer[11];
    sprintf(port_buffer, "%d", port);
    getaddrinfo(addr, port_buffer, &client->hint, &client->res);

    return client;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    /* Check if arguments are valid */
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    /* Create socket for client connection */
    for (cl->rp = cl->res; 
         cl->rp != NULL; 
         cl->rp = cl->rp->ai_next) {
            cl->socket_fd = socket(cl->rp->ai_family, 
                                   cl->rp->ai_socktype, 
                                   cl->rp->ai_protocol);
            if (cl->socket_fd == -1) {
                continue;
            }
            if (connect(cl->socket_fd, 
                        cl->rp->ai_addr, 
                        cl->rp->ai_addrlen) != -1) {
                            break;
            }
            close(cl->socket_fd);
    }

    /* Send compose name data to server */
    int comp_len = strlen(name) + 2;
    char *comp_name = malloc(comp_len);
    comp_name[0] = RPC_FIND;
    strcpy(comp_name + 1, name);
    write(cl->socket_fd, comp_name, strlen(comp_name));

    /* Read result sending from server */
    char recv_buff[1024];
    while (read(cl->socket_fd, recv_buff, 
                sizeof(recv_buff) - 1) > 0) {

    }

    /* Check if result is valid */
    if (recv_buff[0] == 0) {
        return NULL;
    } else if (recv_buff[0] == RPC_FIND) {
        if (recv_buff[1] == 0) {
            return NULL;
        }
    }

    close(cl->socket_fd);

    /* Return handle for further reference */
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    return handle;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    return NULL;
}

void rpc_close_client(rpc_client *cl) {

}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}

char *rpc_data_compose(int type, rpc_data *data) {
    
}

rpc_data *rpc_data_decompose(char *data) {

}
