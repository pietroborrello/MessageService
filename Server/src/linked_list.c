#include "linked_list.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct _linked_list_node {
    any_t value;
    struct _linked_list_node *next;
} linked_list_node;

typedef struct {
    linked_list_node *head;
    linked_list_node *tail;
    int size;
} linked_list_struct;

int linked_list_size(linked_list* ll)
{
	linked_list_struct *ptr = (linked_list_struct *)ll;
	return ptr->size;
}

int linked_list_get(linked_list * ll, int index, any_t *value) {
    linked_list_struct *ptr = (linked_list_struct *) ll;
    if (index < 0 || index >= ptr->size)
        return LINKED_LIST_NOK;
    int i;
    linked_list_node *current = ptr->head;
    for (i = 0; i < index && current != NULL; i++) {
        current = current->next;
    }
    if (current == NULL)
        return LINKED_LIST_NOK;
    *value = current->value;
    return LINKED_LIST_OK;
}

int linked_list_add(linked_list * ll, any_t value) {
    linked_list_struct *ptr = (linked_list_struct *) ll;
    linked_list_node *added = (linked_list_node *) malloc(sizeof(linked_list_node));
    added->next = NULL;
    added->value = value;
    if (ptr->tail != NULL)
        ptr->tail->next = added;
    if (ptr->head == NULL)
        ptr->head = added;
    ptr->tail = added;
    ptr->size++;
    return LINKED_LIST_OK;
}

int linked_list_remove(linked_list *ll, int index) {
    linked_list_struct *ptr = (linked_list_struct *) ll;
    if (index < 0 || index >= ptr->size)
        return LINKED_LIST_NOK;
    int i;
    linked_list_node *current = ptr->head;
    linked_list_node *previous = NULL;
    for (i = 0; i < index && current != NULL; i++) {
        previous = current;
        current = current->next;
    }
    if (ptr->head == ptr->tail)
        ptr->tail = NULL;
    if (previous == NULL)
        ptr->head = current->next;
    else
        previous->next = current->next;
    free(current->value);
    free(current);
    ptr->size--;
    return LINKED_LIST_OK;
}

linked_list * linked_list_new() {
    linked_list_struct *ptr = (linked_list_struct *) malloc(sizeof(linked_list_struct));
    ptr->head = NULL;
    ptr->tail = NULL;
    ptr->size = 0;
    return (linked_list *) ptr;
}

int linked_list_delete(linked_list *ll) {

	if (ll == NULL) {
		return LINKED_LIST_NOK;
	}

    linked_list_struct *ptr = (linked_list_struct *) ll;
    int i;
    for (i = 0; ptr->size > 0; i++) {
        linked_list_remove(ll, 0);
    }
    free(ll);
    return LINKED_LIST_OK;
}

linked_list_iterator * linked_list_iterator_new(linked_list *ll) {
    linked_list_struct *ptr = (linked_list_struct *) ll;
    return (linked_list_iterator *) ptr->head;
}

linked_list_iterator * linked_list_iterator_next(linked_list_iterator * iter) {
    linked_list_node *ptr = (linked_list_node *) iter;
    return ptr->next;
}

any_t linked_list_iterator_getvalue(linked_list_iterator *iter) {
    linked_list_node *ptr = (linked_list_node *) iter;
    return ptr->value;
}
