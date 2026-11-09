/* RemoveLLE (MAT3 ll.c): takes a record out of a linked list (InsertLLE.c has the lists). */
#include "game/game.h"

/* A record's links (MAT3 tDLLE); in a list of handles, the first word of the handle is the record. */
typedef struct Link {struct Link *next, *prev;} Link;
/* A list's header (MAT3 tDLList). */
typedef struct List {u8 handle, dbl, pad[2];u32 numElements;Link *head, *tail;} List;

/* Unlinks object (a record, or a handle in a list of handles) from list: in place in a doubly-linked list, else by a
 * walk from the head. Nothing for a null object.
 * Original game: the count goes down even when a singly-linked list doesn't hold object. */
void RemoveLLE(List *list, Link *object) {
    Link *links, *current, *header, *next;
    if (object == 0) return;
    if (list->handle) links = object->next;
    else links = object;
    if (list->dbl) {
        if (links->next) {
            if (list->handle) links->next->next->prev = links->prev;
            else links->next->prev = links->prev;
        } else list->tail = links->prev;
        if (links->prev) {
            if (list->handle) links->prev->next->next = links->next;
            else links->prev->next = links->next;
        } else list->head = links->next;
    } else if (object == list->head) {
        list->head = links->next;
        if (list->head == 0) list->tail = 0;
    } else if (list->handle) {
        current = list->head;
        while (current) {
            header = current->next;
            next = header->next;
            if (object == next) {
                header->next = links->next;
                if (links->next == 0) list->tail = current;
                break;
            }
            current = next;
        }
    } else {
        current = list->head;
        while (current) {
            header = current->next;
            if (object == header) {
                current->next = links->next;
                if (links->next == 0) list->tail = current;
                break;
            }
            current = header;
        }
    }
    list->numElements--;
}
