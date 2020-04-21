/* timing machinery
 * Copyright (C) 1998-2001,2013  D. Hugh Redelmeier.
 * Copyright (C) 2005-2008 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2012 Paul Wouters <paul@libreswan.org>
 * Copyright (C) 2013 Matt Rogers <mrogers@redhat.com>
 * Copyright (C) 2017-2019 Andrew Cagney <cagney@gnu.org>
 * Copyright (C) 2019 Paul Wouters <pwouters@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef _TIMER_H
#define _TIMER_H

#include "deltatime.h"
#include "monotime.h"

struct state;   /* forward declaration */
struct fd;

struct pluto_event {
	enum event_type ev_type;        /* Event type if time based */
	const char *ev_name;		/* Name or enum_name(ev_type) */
	struct state   *ev_state;       /* Pointer to relevant state (if any) */
	struct event *ev;               /* libevent data structure */
	monotime_t ev_time;
	struct pluto_event *next;
};

extern void event_schedule(enum event_type type, deltatime_t delay,
			   struct state *st);
void event_delete(enum event_type type, struct state *st);
extern void event_schedule_s(enum event_type type, time_t delay_seconds,
			     struct state *st);
extern void event_force(enum event_type type, struct state *st);
extern void delete_event(struct state *st);
extern void handle_next_timer_event(void);
extern void init_timer(void);

extern void timer_list(const struct fd *whackfd);
extern char *revive_conn;

/*
 * Since global timers (one-shot or periodic) rely on global state
 * they don't need a context parameter.
 *
 * XXX: implementation can be found in server.c and not timer.c as it
 * is just easier.
 */
typedef void (global_timer_cb)(void);
void enable_periodic_timer(enum event_type type, global_timer_cb *cb,
			   deltatime_t period);

void init_oneshot_timer(enum event_type type, global_timer_cb *cb);
void schedule_oneshot_timer(enum event_type type, deltatime_t delay);
void deschedule_oneshot_timer(enum event_type type);

#endif /* _TIMER_H */
