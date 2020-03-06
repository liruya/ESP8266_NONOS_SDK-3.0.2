#include "user_task.h"

static const char *TAG = "Task";

LOCAL bool ICACHE_FLASH_ATTR check_task(task_t *ptask) {
	if (ptask == NULL || ptask->vtable == NULL) {
		return false;
	}
	if (ptask->vtable->start == NULL ||
		ptask->vtable->stop == NULL) {
		return false;		
	}
	return true;
}

void ICACHE_FLASH_ATTR user_task_stop(task_t **task) {
	DBG(TAG, "task stop... %d  *%d  **%d", task, *task, **task);
	if (task == NULL) {
		return;
	}
	task_t *ptask = *task;
	if (check_task(ptask)) {
		os_timer_disarm(&ptask->timer);
		if (ptask->vtable->stop(task)) {
			ptask->status = false;
		}
		if (ptask->impl != NULL && ptask->impl->post_excute != NULL) {
			ptask->impl->post_excute();
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR task_timeout_cb(void *arg) {
	DBG(TAG, "task timeout... %d", arg);
	task_t **task = arg;
	task_t *ptask = *task;
	user_task_stop(task);
	if (check_task(ptask)) {
		if (ptask->vtable->timeout_cb != NULL) {
			ptask->vtable->timeout_cb();
		}
	}
}

void ICACHE_FLASH_ATTR user_task_start(task_t **task) {
	DBG(TAG, "task start... %d  *%d  **%d", task, *task, **task);
	if (task == NULL) {
		return;
	}
	task_t *ptask = *task;
	if (check_task(ptask)) {
		if (ptask->impl != NULL && ptask->impl->pre_excute != NULL) {
			ptask->impl->pre_excute();
		}

		if (ptask->vtable->start(task)) {
			ptask->status = true;
			
			os_timer_disarm(&ptask->timer);
			os_timer_setfn(&ptask->timer, task_timeout_cb, task);
			os_timer_arm(&ptask->timer, ptask->timeout, 0);
		}
	}
}

bool ICACHE_FLASH_ATTR user_task_status(task_t **task) {
	if (task == NULL) {
		return false;
	}
	task_t *ptask = *task;
	if (ptask != NULL) {
		return ptask->status;
	}
	return false;
}
