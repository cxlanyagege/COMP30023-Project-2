#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    server->hint.ai_family = AF_INET;
    server->hint.ai_socktype = SOCK_STREAM;
    server->hint.ai_flags = AI_PASSIVE;

    /* Create socket for server */
    char port_buffer[11];
    sprintf(port_buffer, "%d", port);
    getaddrinfo(NULL, port_buffer, &server->hint, &server->res);
    server->socket_fd = socket(server->res->ai_family, 
                               server->res->ai_socktype, 
                               server->res->ai_flags);

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
    bind(srv->socket_fd, srv->res->ai_addr, srv->res->ai_addrlen);
    listen(srv->socket_fd, 10);
    int is_working = 1;

    /* Wait until query coming in */
    while (is_working) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int conn_fd = accept(srv->socket_fd, 
                            (struct sockaddr*)&client_addr, 
                             &client_addr_size);

        /* No query, continue waiting */
        if (conn_fd < 0) {
            continue;
        }

        /* Send result back to client */
        char send_buff[1024];
        snprintf(send_buff, sizeof(send_buff), "");
        write(conn_fd, send_buff, strlen(send_buff)); 
        close(conn_fd);
        is_working = 0;
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
    client->hint.ai_family = AF_INET;
    client->hint.ai_socktype = SOCK_STREAM;

    /* Create socket for server */
    char port_buffer[11];
    sprintf(port_buffer, "%d", port);
    getaddrinfo(addr, port_buffer, &client->hint, &client->res);
    client->socket_fd = socket(client->res->ai_family, 
                               client->res->ai_socktype, 
                               client->res->ai_protocol);

    return client;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    return NULL;
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
