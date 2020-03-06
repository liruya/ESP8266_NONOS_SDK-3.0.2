#ifndef	__USER_TASK_H__
#define	__USER_TASK_H__

#include "app_common.h"

typedef struct _task_vtable 	task_vtable_t;		//虚函数表
typedef struct _task_impl		task_impl_t;		//接口
typedef struct _task			task_t;

struct _task {
	const task_vtable_t *vtable;
	const task_impl_t 	*impl;
	os_timer_t 			timer;
	uint32_t 			timeout;
	bool 				status;
};

struct _task_vtable {
	bool (*const start)(task_t **);
	bool (*const stop)(task_t **);
	void (*const timeout_cb)();
};

struct _task_impl {
	void (*const pre_excute)();
	void (*const post_excute)();
};

#define	newTaskVTable(start, stop, timeout)	{(start), (stop), (timeout)}
#define	newTaskImpl(pre_exe, post_exe)		{(pre_exe), (post_exe)}

extern void user_task_start(task_t **);
extern void user_task_stop(task_t **);
extern bool user_task_status(task_t **);

#endif