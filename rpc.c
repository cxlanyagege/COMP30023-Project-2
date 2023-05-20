#define RPC_ERROR 0
#define RPC_FIND 1
#define RPC_CALL 2

#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
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

struct rpc_handle {
    /* Add variable(s) for handle */
    size_t name_len;
    char *name;
    int handle_size;
};

char *rpc_data_compose(int type, int *size, rpc_data *data);
rpc_data *rpc_data_decompose(int type, char *data, int offset);
char *rpc_handle_compose(int type, rpc_handle *handle);
rpc_handle *rpc_handle_decompose(char *handle);
uint64_t rpc_htonl(uint64_t value);
uint64_t rpc_ntohl(uint64_t value);

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
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int conn_fd = accept(srv->socket_fd, 
                        (struct sockaddr*)&client_addr, 
                        &client_addr_size);

    /* Wait until query coming in */
    while (1) {
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

                /* Matched result, reform client */
                if (j == byte_len) {
                    char send_buff[1024];
                    send_buff[0] = RPC_FIND;
                    send_buff[1] = 1;
                    write(conn_fd, send_buff, strlen(send_buff)); 
                    break;
                }
            }

            /* No matched handler function */
            if (i == srv->handler_size) {
                char send_buff[1024];
                send_buff[0] = RPC_FIND;
                send_buff[1] = 0;
                write(conn_fd, send_buff, strlen(send_buff)); 
            }
        } 
        
        /* When client asks to call function */
        else if (recv_buff[0] == RPC_CALL) {
            /* Decomposed buffer into rpc_handle */
            rpc_handle *handle = rpc_handle_decompose(recv_buff);

            /* Decomposed buffer into rpc_data */
            rpc_data *data = rpc_data_decompose(RPC_CALL, recv_buff, handle->handle_size);
            rpc_data *result;

            /* Call corresponding function */
            for (int i = 0 ;i < srv->handler_size; i++) {
                if (strcmp(srv->handler_name[i], handle->name) == 0) {
                    result = srv->handler[i](data);
                    break;
                }
            }

            /* Send result back to client */
            int buff_size;

            if (result == NULL) {
                char send_buff[1024];
                send_buff[0] = RPC_ERROR;
                write(conn_fd, send_buff, 1);
            } else {
                if ((result->data2_len != 0 && result->data2 == NULL) || 
                    (result->data2_len == 0 && result->data2 != NULL)) {
                    char send_buff[1024];
                    send_buff[0] = RPC_ERROR;
                    write(conn_fd, send_buff, 1);
                } else {
                    char *data_buff = rpc_data_compose(RPC_CALL, &buff_size, result);
                    char *send_buff = malloc(buff_size + 1);
                    send_buff[0] = RPC_CALL;
                    memcpy(send_buff + 1, data_buff, buff_size);
                    write(conn_fd, send_buff, buff_size + 1);
                }
            }
        }

        /* Clear transfered data once finished */
        for (int i = 0; i < 1024; i++) {
            recv_buff[i] = 0;
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

    /* Create socket for client connection */
    for (client->rp = client->res; 
         client->rp != NULL; 
         client->rp = client->rp->ai_next) {
            client->socket_fd = socket(client->rp->ai_family, 
                                   client->rp->ai_socktype, 
                                   client->rp->ai_protocol);
            if (client->socket_fd == -1) {
                continue;
            }
            if (connect(client->socket_fd, 
                        client->rp->ai_addr, 
                        client->rp->ai_addrlen) != -1) {
                            break;
            }
            printf("yoyo\n");
            close(client->socket_fd);
    }

    return client;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    /* Check if arguments are valid */
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    /* Send compose name data to server */
    int comp_len = strlen(name) + 2;
    char *comp_name = malloc(comp_len);
    comp_name[0] = RPC_FIND;
    strcpy(comp_name + 1, name);
    write(cl->socket_fd, comp_name, strlen(comp_name));

    /* Read result sending from server */
    char recv_buff[1024];
    read(cl->socket_fd, recv_buff, sizeof(recv_buff) - 1);

    /* Check if result is valid */
    if (recv_buff[0] == 0) {
        return NULL;
    } else if (recv_buff[0] == RPC_FIND) {
        if (recv_buff[1] == 0) {
            return NULL;
        }
    }

    /* Return handle for further reference */
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    handle->name_len = strlen(name);
    handle->name = malloc(handle->name_len);
    memcpy(handle->name, name, handle->name_len);
    
    return handle;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    /* Check if arguments are valid */
    if (cl == NULL || h == NULL || payload == NULL || 
        payload->data2_len == 0 || payload->data2 == NULL) {
        return NULL;
    }

    /* Generate and send composed rpc handle and data */
    int data_buff_size;
    char *handle_buff = rpc_handle_compose(RPC_CALL, h);
    char *data_buff = rpc_data_compose(RPC_CALL, &data_buff_size, payload);
    char *send_buff = malloc(h->handle_size + data_buff_size);
    memcpy(send_buff, handle_buff, h->handle_size);
    memcpy(send_buff + h->handle_size, data_buff, data_buff_size);
    write(cl->socket_fd, send_buff, h->handle_size + data_buff_size);

    /* Receive function calling result */
    char recv_buff[1024];
    read(cl->socket_fd, recv_buff, sizeof(recv_buff) - 1);
    if (recv_buff[0] == RPC_ERROR) {
        return NULL;
    }
    rpc_data *data = rpc_data_decompose(RPC_CALL, recv_buff + 1, 0);

    return data;
}

void rpc_close_client(rpc_client *cl) {
    close(cl->socket_fd);
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

char *rpc_data_compose(int type, int *size, rpc_data *data) {
    /* Check if rpc data is valid */
    if (data == NULL) {
        return NULL;
    }

    /* Apply data compose by different query type */
    if (type == RPC_CALL) {
        /* Record total size of each part */
        uint64_t data1 = (uint64_t) data->data1;
        uint64_t data2_len = (uint64_t) data->data2_len;
        *size = sizeof(uint64_t) + sizeof(uint64_t) + data2_len;
        char *comp_data = malloc(*size);

        /* data2 in rpc_data */
        if (data->data2_len != 0) {
            memcpy(comp_data + sizeof(uint64_t) + sizeof(uint64_t), 
                   data->data2, data2_len);
        }

        /* data1 in rpc_data */
        data1 = rpc_htonl(data1);
        memcpy(comp_data, 
             &(data1), sizeof(uint64_t));

        /* data2_len in rpc_data */
        data2_len = rpc_htonl(data2_len);
        memcpy(comp_data + sizeof(uint64_t), 
             &(data2_len), sizeof(uint64_t));

        return comp_data;
    }

    /* Unsupported call type, composed failed */
    return NULL;
}

rpc_data *rpc_data_decompose(int type, char *comp_data, int offset) {
    /* Apply data decompose by different query type */
    if (type == RPC_CALL) {
        rpc_data *data = malloc(sizeof(rpc_data));
        uint64_t data1, data2_len;

        /* data1 in rpc_data */
        memcpy(&(data1), 
                 comp_data + offset, sizeof(uint64_t));
        data1 = rpc_ntohl(data1);

        /* data2_len in rpc_data */
        memcpy(&(data2_len), 
                 comp_data + offset + sizeof(uint64_t), sizeof(uint64_t));
        data2_len = rpc_ntohl(data2_len);

        /* data2 in rpc_data */
        if (data2_len != 0) {
            data->data2_len = (size_t) data2_len;
            data->data2 = malloc(data->data2_len * sizeof(void));
            memcpy(data->data2, 
                   comp_data + offset + sizeof(uint64_t) + sizeof(uint64_t), 
                   data2_len);
        } else {
            data->data2 = NULL;
        }

        data->data1 = (int) data1;
        data->data2_len = (size_t) data2_len;

        return data;
    }

    /* Unsupported call type, decomposed failed */
    return NULL;
}

char *rpc_handle_compose(int type, rpc_handle *handle) {
    /* Apply data compose by different query type */
    if (type == RPC_CALL) {
        /* Record total size of each part */
        char call_type = RPC_CALL;
        int size = sizeof(char) + 
                   sizeof(size_t) + 
                   handle->name_len * sizeof(char) + 
                   sizeof(int);
        char *comp_handle = malloc(size * sizeof(char));
        
        /* RPC_CALL indicator */
        memcpy(comp_handle, 
               &call_type, sizeof(char));

        /* Handler function name length */
        memcpy(comp_handle + sizeof(char), 
               &(handle->name_len), sizeof(size_t));

        /* Handler function name */
        memcpy(comp_handle + sizeof(char) + 
               sizeof(size_t), 
               handle->name, handle->name_len * sizeof(char));

        /* Handle size */
        handle->handle_size = size;
        memcpy(comp_handle + sizeof(char) + 
               sizeof(size_t) + handle->name_len * sizeof(char), 
             &(handle->handle_size), sizeof(int));

        return comp_handle;
    }

    /* Unsupported call type, composed failed */
    return NULL;
}

rpc_handle *rpc_handle_decompose(char *comp_handle) {
    rpc_handle *handle = malloc(sizeof(rpc_handle));

    /* Handler function name length */
    memcpy(&(handle->name_len), 
             comp_handle + sizeof(char), 
             sizeof(size_t));

    /* Handler function name */
    handle->name = malloc(handle->name_len * sizeof(char));
    memcpy(handle->name, 
           comp_handle + sizeof(char) + sizeof(size_t), 
           handle->name_len * sizeof(char));
    handle->name[handle->name_len] = '\0';

    /* Handle size */
    memcpy(&(handle->handle_size), 
             comp_handle + sizeof(char) + sizeof(size_t) +
             handle->name_len * sizeof(char), 
             sizeof(int));

    return handle;
}

uint64_t rpc_htonl(uint64_t value) {
    /* Make byte order into Big Endian */
    uint32_t high_part = htonl((uint32_t)(value >> 32));
    uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));

    return ((uint64_t)high_part << 32) | low_part;
}

uint64_t rpc_ntohl(uint64_t value) {
    /* Make byte order into Little Endian */
    uint32_t high_part = ntohl((uint32_t)(value >> 32));
    uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
    
    return ((uint64_t)high_part << 32) | low_part;
}
