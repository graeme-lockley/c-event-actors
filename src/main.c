#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "actor.h"

/**
 * The purpose of this piece of code is to:
 *   - Create the root-actor which does nothing of any consequence other than to
 *     have identity and be the root.
 *   - Create the socket-server actor which will create a listener in a thread 
 *     and then, everytime a request is made, will message the new socket back 
 *     to the configured actor.  This actor can do with it is it pleases. 
 * 
 *     The socket-server exists in one of the following states:
 *       SSA_INIT: 
 *         The socket-server is still performing the necessary setup and initialisation
 *       SSA_RUNNING: 
 *         The socket-server is accepting connections
 * 
 *     The socket-server will respond to the following messages:
 *       MSG_ACTOR_INIT: 
 *         Initialises the socket server by creating the listener thread.  The 
 *         actor is set into the INIT
 *         state.
 *       SSA_LISTENER_SUCCEEDED:
 *         Changes the actor into the RUNNING state.
 *       SSA_LISTENER_FAILED:
 *         Forces the socket-server to display an error message, releases all 
 *         resources then terminates the actor.
 *       MSG_CLOSE:
 *         Closes the listener, releases all resources then terminates the 
 *         actor.
 *   - Create the socket-accept-factory actor which receives the SSA_ON_ACCEPT 
 *     message and creates a socket-handler actor to process the socket request.
 *     This actor is transient and, on completing its responsibility, will 
 *     terminate.
 */

/**
 * Name: root
 * Acronym: root
 */
int root_actor_handler(int actor_id, void *state, struct Msg *msg)
{
    printf("root: %d %d\n", msg->sender, msg->msg_type);
    return HANDLER_OKAY_CONTINUE;
}

/**
 * Name: socket-server
 * Acronym: SSA
 */

#define SSA_INIT 0
#define SSA_RUNNING 1

struct SSA_State
{
    int state;
    char *name;
    int port;
    ACTOR_REF on_accept;
    int server_fd;
    struct sockaddr_in address;
};

static void free_SSA_State(void *m)
{
    struct SSA_State *v = (struct SSA_State *)m;
    if (v->name != NULL)
        free(v->name);
    free(v);
}

#define SSA_LISTENER_SUCCEEDED 100
#define SSA_LISTENER_FAILED 101
#define SSA_ON_ACCEPT 102

/* arguments that are passed when creating the socket-server actor */
struct SSA_Args
{
    char *name; /* can be NULL */
    int port;
    ACTOR_REF on_accept;
};

static void free_SSA_Args(void *m)
{
    struct SSA_Args *v = (struct SSA_Args *)m;
    if (v->name != NULL)
        free(v->name);
    free(v);
}

struct SSA_ListenerSucceededMsg
{
    int server_fd;
    struct sockaddr_in address;
};

struct SSA_ListenerFailedMsg
{
    char *msg;
};

static void free_SSA_ListenerFailedMsg(void *m)
{
    struct SSA_ListenerFailedMsg *v = (struct SSA_ListenerFailedMsg *)m;
    free(v->msg);
    free(v);
}

struct SSA_OnAcceptMsg
{
    int socket;
};

struct CreateSocketServerArgs
{
    char *name;
    int port;
    ACTOR_REF on_complete;
    ACTOR_REF on_accept;
};

static void free_CreateSocketServerArgs(void *m)
{
    struct CreateSocketServerArgs *v = (struct CreateSocketServerArgs *)m;
    if (v->name != NULL)
        free(v->name);
    free(v);
}

static void *create_socket_server(void *args)
{
    struct CreateSocketServerArgs *a = (struct CreateSocketServerArgs *)args;

    printf("create socket server: %s %d %d\n", a->name == NULL ? "(null)" : a->name, a->port, a->on_accept);

    struct sockaddr_in address;
    const int addrlen = sizeof(address);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        struct SSA_ListenerFailedMsg *msg = (struct SSA_ListenerFailedMsg *)malloc(sizeof(struct SSA_ListenerFailedMsg));
        msg->msg = strdup("In socket");
        actor_post(a->on_complete, a->on_complete, SSA_LISTENER_FAILED, msg, free_SSA_ListenerFailedMsg);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(a->port);

    memset(address.sin_zero, '\0', sizeof address.sin_zero);

    if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0)
    {
        struct SSA_ListenerFailedMsg *msg = (struct SSA_ListenerFailedMsg *)malloc(sizeof(struct SSA_ListenerFailedMsg));
        msg->msg = strdup("In bind");
        actor_post(a->on_complete, a->on_complete, SSA_LISTENER_FAILED, msg, free_SSA_ListenerFailedMsg);
        return NULL;
    }

    if (listen(server_fd, 10) < 0)
    {
        struct SSA_ListenerFailedMsg *msg = (struct SSA_ListenerFailedMsg *)malloc(sizeof(struct SSA_ListenerFailedMsg));
        msg->msg = strdup("In listen");
        actor_post(a->on_complete, a->on_complete, SSA_LISTENER_FAILED, msg, free_SSA_ListenerFailedMsg);
        return NULL;
    }

    while (1)
    {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

        if (new_socket < 0)
        {
            printf("Socket server closed\n");
            return NULL;
        }

        struct SSA_OnAcceptMsg *msg = (struct SSA_OnAcceptMsg *)malloc(sizeof(struct SSA_OnAcceptMsg));
        msg->socket = new_socket;
        actor_post(a->on_complete, a->on_accept, SSA_ON_ACCEPT, msg, free);
    }

    free_CreateSocketServerArgs(a);

    return NULL;
}

int socket_server_handler(int actor_id, void *state, struct Msg *msg)
{
    if (msg->msg_type == MSG_ACTOR_INIT)
    {
        printf("SSA: INIT\n");

        struct SSA_Args *a = (struct SSA_Args *)msg->args;
        struct SSA_State *s = (struct SSA_State *)malloc(sizeof(struct SSA_State));
        s->state = SSA_INIT;
        s->name = a->name == NULL ? NULL : strdup(a->name);
        s->port = a->port;
        s->on_accept = a->on_accept;
        *((struct SSA_State **)state) = s;

        struct CreateSocketServerArgs *cssArgs = (struct CreateSocketServerArgs *)malloc(sizeof(struct CreateSocketServerArgs));
        cssArgs->name = a->name == NULL ? NULL : strdup(a->name);
        cssArgs->port = a->port;
        cssArgs->on_complete = actor_id;
        cssArgs->on_accept = a->on_accept;

        pthread_t thread;
        pthread_create(&thread, NULL, create_socket_server, (void *)cssArgs);

        return HANDLER_OKAY_CONTINUE;
    }

    if (msg->msg_type == SSA_LISTENER_SUCCEEDED)
    {
        printf("SSA: Listener Succeeded\n");

        struct SSA_State *s = *((struct SSA_State **)state);
        struct SSA_ListenerSucceededMsg *m = (struct SSA_ListenerSucceededMsg *)msg->args;

        if (s->state == SSA_RUNNING)
            return HANDLER_NOT_IN_STATE_FOR_MSG_TYPE;

        s->server_fd = m->server_fd;
        s->address = m->address;

        return HANDLER_OKAY_CONTINUE;
    }

    if (msg->msg_type == SSA_LISTENER_FAILED)
    {
        struct SSA_State *s = *((struct SSA_State **)state);
        struct SSA_ListenerFailedMsg *m = (struct SSA_ListenerFailedMsg *)msg->args;

        printf("SSA: Listener Failed: %s\n", m->msg);

        if (s->state == SSA_RUNNING)
            return HANDLER_NOT_IN_STATE_FOR_MSG_TYPE;

        free_SSA_State(s);
        *((struct SSA_State **)state) = NULL;

        return HANDLER_OKAY_STOP;
    }

    if (msg->msg_type == MSG_CLOSE)
    {
        printf("SSA: Close\n");

        struct SSA_State *s = *((struct SSA_State **)state);

        if (s->state != SSA_RUNNING)
            return HANDLER_NOT_IN_STATE_FOR_MSG_TYPE;

        close(s->server_fd);

        free_SSA_State(s);
        *((struct SSA_State **)state) = NULL;

        return HANDLER_OKAY_STOP;
    }

    return HANDLER_UNRECOGNISED_MSG_TYPE;
}

int socket_accept_handler(int actor_id, void *state, struct Msg *msg)
{
    if (msg->msg_type == MSG_ACTOR_INIT)
    {
        printf("sah: INIT\n");

        return HANDLER_OKAY_CONTINUE;
    }

    if (msg->msg_type == SSA_ON_ACCEPT)
    {
        printf("sah: SSA_ON_ACCEPT\n");
        struct SSA_OnAcceptMsg *m = (struct SSA_OnAcceptMsg *)msg->args;

        char buffer[30000] = {0};
        char *hello = "Hello World";
        read(m->socket, buffer, 30000);
        printf("%s\n", buffer);
        fflush(stdout);
        write(m->socket, hello, strlen(hello));
        printf("------------------Hello message sent-------------------\n");
        close(m->socket);

        return HANDLER_OKAY_CONTINUE;
    }

    return HANDLER_UNRECOGNISED_MSG_TYPE;
}

#define ALLOC(s) (s *)malloc(sizeof(s))

int main(int argc, char **argv)
{
    actor_create(ROOT_ACTOR_REF, "root-actor", root_actor_handler, NULL, NULL);
    ACTOR_REF ss_actor = actor_create(ROOT_ACTOR_REF, "socket-accept-actor", socket_accept_handler, NULL, NULL);

    struct SSA_Args *ssa_args = ALLOC(struct SSA_Args);
    ssa_args->name = NULL;
    ssa_args->port = 8080;
    ssa_args->on_accept = ss_actor;

    actor_create(ROOT_ACTOR_REF, "socket-server", socket_server_handler, ssa_args, free_SSA_Args);

    while (1)
    {
        sleep(1);
        actor_process();
    }

    return 0;
}
