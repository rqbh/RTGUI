#ifndef __RT_LIST_H__
#define __RT_LIST_H__

/**
 * @brief initialize a list
 *
 * @param l list to be initialized
 */
rt_inline void rt_list_init(rt_list_t *l)
{
	l->next = l->prev = l;
}

/**
 * @brief insert a node after a list
 *
 * @param l list to insert it
 * @param n new node to be inserted
 */
rt_inline void rt_list_insert_after(rt_list_t *l, rt_list_t *n)
{
	l->next->prev = n;
	n->next = l->next;

	l->next = n;
	n->prev = l;
}

/**
 * @brief insert a node before a list 
 *
 * @param n new node to be inserted
 * @param l list to insert it
 */
rt_inline void rt_list_insert_before(rt_list_t *l, rt_list_t *n)
{
	l->prev->next = n;
	n->prev = l->prev;

	l->prev = n;
	n->next = l;
}

/**
 * @brief remove node from list.
 * @param n the node to remove from the list.
 */
rt_inline void rt_list_remove(rt_list_t *n)
{
	n->next->prev = n->prev;
	n->prev->next = n->next;

	rt_list_init(n);
}

/**
 * @brief tests whether a list is empty
 * @param l the list to test.
 */
rt_inline int rt_list_isempty(const rt_list_t *l)
{
	return l->next == l;
}

/**
 * @brief get the struct for this entry
 * @param node the entry point
 * @param type the type of structure
 * @param member the name of list in structure
 */
#define rt_list_entry(node, type, member) \
    ((type *)((char *)(node) - (unsigned long)(&((type *)0)->member)))

/* the direction can only be next or prev. If you want to iterate the list in
 * normal order, use next. If you want to iterate the list with reverse order,
 * use prev.*/
#define rt_list_foreach(node, list, direction)	\
	for ((node) = (list)->direction; (node) != list; (node) = (node)->direction)

#endif
