/* InsertLLE (MAT3 ll.c): linked lists of records the caller owns, singly or doubly linked (dbl), whose links are in
 * the records themselves or, for a list of handles (handle), in the records a handle points to: object is then a
 * handle, and its links are object->next's. RemoveLLE.c takes a record out. Host pointers on the PC. */
#include "game/game.h"

/* A record's links (MAT3 tDLLE); in a list of handles, the first word of the handle is the record. */
typedef struct Link {struct Link *next, *prev;} Link;
/* A list's header (MAT3 tDLList). */
typedef struct List {u8 handle, dbl, pad[2];u32 numElements;Link *head, *tail;} List;

/* Inserts object into list before the record before, or at the tail when before is 0. A doubly-linked list
 * relinks in place; a singly-linked one walks from the head to find the record that points at before. Nothing for
 * a null object.
 * Original game: the singly-linked walk doesn't stop once it has linked object in; it goes on through object,
 * whose own link is set only after the walk, so it follows whatever object's link held before. */
void InsertLLE(List *list, Link *object, Link *before) {
    Link *links, *beforeLinks, *current, *header, *next;
    if (object == 0) return;
    if (list->handle) {
        links = object->next;
        if (before) beforeLinks = before->next;
        else beforeLinks = 0;
    } else {
        links = object;
        beforeLinks = before;
    }
    if (before) {
        if (list->dbl) {
            if (beforeLinks->prev) {
                if (list->handle) beforeLinks->prev->next->next = object;
                else beforeLinks->prev->next = object;
            } else list->head = object;
            links->prev = beforeLinks->prev;
            beforeLinks->prev = object;
        } else if (before == list->head) list->head = object;
        else if (list->handle) {
            current = list->head;
            while (current) {
                header = current->next;
                next = header->next;
                if (before == next) {
                    header->next = object;
                    next = current->next->next;
                }
                current = next;
            }
        } else {
            current = list->head;
            while (current) {
                header = current->next;
                if (before == header) {
                    current->next = object;
                    header = object;
                }
                current = header;
            }
        }
    } else {
        if (list->dbl) links->prev = list->tail;
        if (list->tail) {
            if (list->handle) list->tail->next->next = object;
            else list->tail->next = object;
        } else list->head = object;
        list->tail = object;
    }
    links->next = before;
    list->numElements++;
}
