/*
 * File:   linked_list.h
 * Author: Francesco
 *
 * Created on 28 febbraio 2015, 16.05
 */

#ifndef LINKED_LIST_H
#define	LINKED_LIST_H

#ifdef	__cplusplus
extern "C" {
#endif

#define LINKED_LIST_OK 0
#define LINKED_LIST_NOK -1

typedef void linked_list;
typedef void linked_list_iterator;
typedef void *any_t;


int linked_list_size(linked_list* ll);

/*
 * Write in the position denoted by the value pointer the element in the i-th
 * position. Returns LINKED_LIST_OK in case of success and LINKED_LIST_NOK in case
 * of failure.
 */
extern int linked_list_get(linked_list * ll, int i, any_t *value);

/*
 * Add value at the end of the list. Return LINKED_LIST_NOK in case of failure or
 * the position of the element in case of success
 */
extern int linked_list_add(linked_list * ll, any_t value);

/*
 * Remove the element in the i-th position. Return LINKED_LIST_OK in case of success
 * and LINKED_LIST_NOK in case of failure
 */
extern int linked_list_remove(linked_list *ll, int i);

/*
 * Create an empty linked list and return a pointer or NULL in case of insuccess
 */
extern linked_list * linked_list_new();

/*
 * Destroy the linked_list and its content. Returns LINKED_LIST_OK in case of success
 * and LINKED_LIST_NOK in case of insuccess
 */
extern int linked_list_delete(linked_list *ll);

/*
 * Returns a pointer to an iterator for the list or NULL in case of error or if the list is empty
 */
extern linked_list_iterator * linked_list_iterator_new(linked_list *ll);

/*
 * Returns a pointer to the next element in the list or NULL is there is no more elements
 */
extern linked_list_iterator * linked_list_iterator_next(linked_list_iterator * iter);

/*
 * Returns the value of list in correspondence of the iterator
 */
extern any_t linked_list_iterator_getvalue(linked_list_iterator *iter);

#ifdef	__cplusplus
}
#endif

#endif	/* LINKED_LIST_H */
