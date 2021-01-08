#ifndef SGE_LIST_H_
#define SGE_LIST_H_

typedef struct sge_list {
	struct sge_list *next, *prev;
} sge_list;


#define LIST_INIT(list)		\
do {						\
	(list)->next = (list);	\
	(list)->prev = (list);	\
} while(0)

#define LIST_ADD_TAIL(list, node)	\
do {								\
	(node)->next = (list);			\
	(node)->prev = (list)->prev;	\
	(list)->prev->next = (node);	\
	(list)->prev = (node);			\
} while(0)

#define LIST_ADD_HEAD(list, node)	\
do {								\
	(node)->prev = (list);			\
	(node)->next = (list)->next;	\
	(list)->next->prev = (node);	\
	(list)->next = (node);			\
} while(0)

#define LIST_REMOVE(node)				\
do {									\
	(node)->prev->next = (node)->next;	\
	(node)->next->prev = (node)->prev;	\
} while(0)

#define LIST_EMPTY(list)	((list)->next == (list))

#define LIST_DATA(ptr, type, member)	\
(type*)((void*)(ptr) - (void*)(&(((type*)0)->member)))

#define LIST_FOREACH(start, head)		\
for (start = (head)->next; (start) != (head); start = (start)->next) 


#endif
