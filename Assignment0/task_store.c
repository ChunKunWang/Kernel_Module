#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include "task.h"
#define MAX_NUM 50
#define LOC_NUM 7
#define NO_LOC_NUM 4

char *LOC_name[LOC_NUM] = {"pid", "paged_start", "paged_end", "pinned_start", "pinned_end", "inode_start", "inode_end"};
char *NO_LOC_name[NO_LOC_NUM] = {"vm_ptr", "fs_ptr", "paged_ptr", "pinned_ptr"};

struct task_list
{
	int identifier;
	task *task_ptr;
};

struct task_list *list[MAX_NUM];
int counter = -1;
//task *task_ptr = NULL;

void *task_store(enum operation op, char *parm, task *ptr)
{
	if(op == INIT) {
		//printf("Call task_store, INIT.\n");
		switch(counter) {
			case 0:
				free(list[0]->task_ptr->vm_ptr->pinned_ptr->pinned_start);
				free(list[0]->task_ptr->vm_ptr->pinned_ptr->pinned_end);
				free(list[0]->task_ptr->vm_ptr->paged_ptr->paged_start);
				free(list[0]->task_ptr->vm_ptr->paged_ptr->paged_end);
				free(list[0]->task_ptr->vm_ptr->pinned_ptr);
				free(list[0]->task_ptr->vm_ptr->paged_ptr);
				free(list[0]->task_ptr->vm_ptr);
				free(list[0]->task_ptr->fs_ptr);
				free(list[0]->task_ptr);
				free(list[0]);
				break;
			case -1: break;
			default:
				while(counter >= 0) {
					free(list[counter]->task_ptr->vm_ptr->pinned_ptr->pinned_start);
					free(list[counter]->task_ptr->vm_ptr->pinned_ptr->pinned_end);
					free(list[counter]->task_ptr->vm_ptr->paged_ptr->paged_start);
					free(list[counter]->task_ptr->vm_ptr->paged_ptr->paged_end);
					free(list[counter]->task_ptr->vm_ptr->pinned_ptr);
					free(list[counter]->task_ptr->vm_ptr->paged_ptr);
					free(list[counter]->task_ptr->vm_ptr);
					free(list[counter]->task_ptr->fs_ptr);
					free(list[counter]->task_ptr);
					free(list[counter]);
					counter--;
				}
		}
		counter = 0;
		list[0] = malloc(sizeof(struct task_list));
		if(list[0] == NULL) {
			printf("error: cannot create task_list\n");
			return NULL;
		}
		list[0]->identifier = 0;
		list[0]->task_ptr = malloc(sizeof(task));
		if(list[0]->task_ptr == NULL) {
			printf("error: cannot create task\n");
			return NULL;
		}

		list[0]->task_ptr->fs_ptr = malloc(sizeof(FS));
		if(list[0]->task_ptr->fs_ptr == NULL) {
			printf("error: cannot create FS\n");
			return NULL;
		}

		list[0]->task_ptr->vm_ptr = malloc(sizeof(VM));
		if(list[0]->task_ptr->vm_ptr == NULL) {
			printf("error: cannot create VM\n");
			return NULL;
		}

		list[0]->task_ptr->vm_ptr->paged_ptr = malloc(sizeof(paged));
		if(list[0]->task_ptr->vm_ptr->paged_ptr == NULL) {
			printf("error: cannot create paged\n");
			return NULL;
		}

		list[0]->task_ptr->vm_ptr->pinned_ptr = malloc(sizeof(pinned));
		if(list[0]->task_ptr->vm_ptr->pinned_ptr == NULL) {
			printf("error: cannot create pinned\n");
			return NULL;
		}

		return list[0]->task_ptr;
	}
	else if(op == STORE) {

		if(counter < 0) {
			printf("error: not yet initial task_store!\n");
			return NULL;
		}

		if(counter >= 50) {
			printf("error: task structure size limit is 50!\n");
			return NULL;
		}

		if(!isdigit(parm[0])) {
			printf("error: parm is not a number\n");
			return NULL;
		}

		if(ptr->vm_ptr->paged_ptr->paged_start == NULL
			|| ptr->vm_ptr->paged_ptr->paged_end == NULL
			|| ptr->vm_ptr->pinned_ptr->pinned_start == NULL
			|| ptr->vm_ptr->pinned_ptr->pinned_end == NULL ) 
		{
			printf("error: missing pointer in task struct\n");
                        return NULL;
		}

		int id = atoi(parm);
		int list_index = 0, i = 0;
		//printf("Call task_store id:%d, STORE.\n", id);

		while(i <= counter) {
			if(list[i]->identifier == id) {
				list_index = i;
				printf("Note: replace the entry of identifier %d\n", id);
				counter--;
				break;
			}
			else list_index = ++i;
		}
		counter++;
		
		/* we have to create a new task_list */
		if(list_index == counter) {
			//printf("creat number of %d task_list\n", list_index);
			list[list_index] = malloc(sizeof(struct task_list));
			list[list_index]->task_ptr = malloc(sizeof(task));
			list[list_index]->task_ptr->fs_ptr = malloc(sizeof(FS));
			list[list_index]->task_ptr->vm_ptr = malloc(sizeof(VM));
			list[list_index]->task_ptr->vm_ptr->paged_ptr = malloc(sizeof(paged));
			list[list_index]->task_ptr->vm_ptr->pinned_ptr = malloc(sizeof(pinned));
		}

		//printf("list_index: %d\n", list_index);
		/*
		fprintf(stderr, "[%d] pid: %lu, start: %lu, end: %lu\n", id
							               , ptr->pid
								       , ptr->fs_ptr->inode_start
								       , ptr->fs_ptr->inode_end);
		*/
		list[list_index]->identifier = id;
		list[list_index]->task_ptr->pid = ptr->pid;
		list[list_index]->task_ptr->fs_ptr->inode_start = ptr->fs_ptr->inode_start; 
		list[list_index]->task_ptr->fs_ptr->inode_end = ptr->fs_ptr->inode_end; 
		list[list_index]->task_ptr->vm_ptr->paged_ptr->paged_start = ptr->vm_ptr->paged_ptr->paged_start; 
		list[list_index]->task_ptr->vm_ptr->paged_ptr->paged_end = ptr->vm_ptr->paged_ptr->paged_end; 
		list[list_index]->task_ptr->vm_ptr->pinned_ptr->pinned_start = ptr->vm_ptr->pinned_ptr->pinned_start; 
		list[list_index]->task_ptr->vm_ptr->pinned_ptr->pinned_end = ptr->vm_ptr->pinned_ptr->pinned_end; 
		/*
		fprintf(stderr, "[%d] pid: %lu, start: %lu, end: %lu\n", list[counter]->identifier
								       , list[counter]->task_ptr->pid
								       , list[counter]->task_ptr->fs_ptr->inode_start
								       , list[counter]->task_ptr->fs_ptr->inode_end);
		*/		
		return list[list_index]->task_ptr;
	}
	else if(op == LOCATE) {
		//printf("Call task_store, LOCATE.\n");
		if( parm == NULL ) {
			printf("error: missing parm\n");
                        return NULL;
		}

		char temp[(int)strlen(parm)+1];
		strcpy(temp, parm);
		temp[(int)strlen(parm)] = '\0';
		//printf("parm input: %s in length %d\n", parm, (int)strlen(parm));
		//printf("temp input: %s in length %d\n", temp, (int)strlen(temp));

		char *token = strtok(temp, " ");
		if(token==NULL) {
			printf("error: wrong parm format\n");
			return NULL;
		}

		int target_id = atoi(token);
		int pos_task_list = 0;
		//printf("target id: %d\n", target_id);

		while( pos_task_list <= counter ) {
			if( list[pos_task_list]->identifier == target_id ) break;
			pos_task_list++;
		}

		if( pos_task_list > counter ) {
			printf("error: identifier not found\n");
			return NULL;
		}
		//printf("position: %d\n", pos_task_list);

		token = strtok(NULL, " ");
		//printf("target string: %s\n", token);
	
		int i = 0;
		bool NoLocError = false;
		while(i < NO_LOC_NUM) {
			if(strstr(token, NO_LOC_name[i]) != NULL) NoLocError = true;
			i++;
		}

		if(NoLocError) {
			printf("error: struct field is not allowed to be located\n");
			return NULL;
		}

		i = 0;
		while(i < LOC_NUM) {
			if(strstr(token, LOC_name[i]) != NULL) {
				//printf("found %s\n", token);
				if((int)strlen(token) != (int)strlen(LOC_name[i])) {
					printf("error: locate string error\n");
					return NULL;
				}
				break;
			}
			i++;
		}

		switch(i) {
			case 0: 
				return &list[pos_task_list]->task_ptr->pid;
				break;
			case 1: 
				return &list[pos_task_list]->task_ptr->vm_ptr->paged_ptr->paged_start;
				break;
			case 2: 
				return &list[pos_task_list]->task_ptr->vm_ptr->paged_ptr->paged_end;
				break;
			case 3: 
				return &list[pos_task_list]->task_ptr->vm_ptr->pinned_ptr->pinned_start;
				break;
			case 4: 
				return &list[pos_task_list]->task_ptr->vm_ptr->pinned_ptr->pinned_end;
				break;
			case 5: 
				return &list[pos_task_list]->task_ptr->fs_ptr->inode_start;
				break;
			case 6: 
				return &list[pos_task_list]->task_ptr->fs_ptr->inode_end;
				break;
			default:
				return NULL;
		}
	}
	else
		printf("Call task_store, operation error.\n");

	return NULL;
}

