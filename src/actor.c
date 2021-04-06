#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actor.h"

#define HASH_SIZE 31

struct Actor
{
    ACTOR_REF id;
    ACTOR_REF parent;
    char *name;
    void *state;
    int (*handler)(int actor_id, void *state, struct Msg *msg);
    struct Actor *next;
};

struct MsgItem
{
    ACTOR_REF target;
    struct Msg msg;
    void (*free_args)(void *msg);
    struct MsgItem *next;
};

static struct Actor *actors[HASH_SIZE];
static int next_actor_id = 0;

// Process from off of the head and they are linked in processing sequenec.
// Tail is where items are appended - the tail refers to the assign position.
// head --> m1 -> m2 -> m3 -> m4 <--tail
static struct MsgItem *head = NULL;
static struct MsgItem **tail = &head;

static struct Actor *find_actor(ACTOR_REF id)
{
    struct Actor *item = actors[id % HASH_SIZE];

    while (1)
    {
        if (item == NULL || item->id == id)
            return item;

        item = item->next;
    }
}

static struct Actor *get_actor(ACTOR_REF id)
{
    struct Actor *item = find_actor(id);

    if (item == NULL)
    {
        fprintf(stderr, "Actor not found: %d\n", id);
        exit(EXIT_FAILURE);
    }
    return item;
}

ACTOR_REF actor_create(
    ACTOR_REF parent,
    char *name,
    int (*handler)(int actor_id, void *state, struct Msg *msg),
    void *args,
    void (*free_args)(void *args))
{
    ACTOR_REF ref = next_actor_id;
    next_actor_id += 1;

    int idx = ref % HASH_SIZE;

    struct Actor *actor = (struct Actor *)malloc(sizeof(struct Actor));
    actor->id = ref;
    actor->parent = parent;
    actor->name = name == NULL ? NULL : strdup(name);
    actor->state = NULL;
    actor->handler = handler;
    actor->next = actors[idx];
    actors[idx] = actor;

    actor_post(parent, ref, MSG_ACTOR_INIT, args, free_args);

    return ref;
}

void actor_post(ACTOR_REF sender, ACTOR_REF target, int msg_type, void *args, void (*free_args)(void *args))
{
    struct MsgItem *item = (struct MsgItem *)malloc(sizeof(struct MsgItem));

    item->target = target;
    item->msg.sender = sender;
    item->msg.msg_type = msg_type;
    item->msg.args = args;
    item->free_args = free_args;
    item->next = NULL;

    *tail = item;
    tail = &item->next;
}

static void process_item(struct MsgItem *item)
{
    struct Actor *actor = get_actor(item->target);
    actor->handler(item->target, &actor->state, &item->msg);

    if (item->free_args != NULL && item->msg.args != NULL)
        item->free_args(item->msg.args);

    free(item);
}

void actor_process()
{
    while (head != NULL)
    {
        struct MsgItem *item = head;

        head = head->next;
        if (head == NULL)
            tail = &head;

        process_item(item);
    }
}
