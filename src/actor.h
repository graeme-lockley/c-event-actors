#ifndef __ACTOR_H__
#define __ACTOR_H__

#define ACTOR_REF int

#define ROOT_ACTOR_REF 0

struct Msg
{
    ACTOR_REF sender;
    int msg_type;
    void *args;
};

#define MSG_ACTOR_INIT 0
#define MSG_ACTOR_SHUTDOWN 1
#define MSG_CLOSE 2

#define HANDLER_OKAY_CONTINUE 0
#define HANDLER_OKAY_STOP 1
#define HANDLER_FATAL_ERROR 2
#define HANDLER_ERROR 3
#define HANDLER_UNRECOGNISED_MSG_TYPE 4
#define HANDLER_NOT_IN_STATE_FOR_MSG_TYPE 5

/**
 * An actor is guaranteed to receive the MSG_ACTOR_INIT as it's first message with the args passed as the message args.
 * No other messages are guaranteed to be sent to an actor.
 */
extern ACTOR_REF actor_create(ACTOR_REF parent, char *name, int (*handler)(int actor_id, void *state, struct Msg *msg), void *args, void (*free_args)(void *args));
extern void actor_post(ACTOR_REF sender, ACTOR_REF target, int msg_type, void *args, void (*free_args)(void *args));
extern void actor_process();

#endif
