/* include/game/view.h: a file's own view of a shared datum.
 *
 * include/game declares each datum once, with the type the data loader lays it out by. Some files read a datum
 * through another type: a struct with only the fields that file uses, the bytes of a table, an unsigned copy of a
 * signed word. Such a file says so after its includes:
 *
 *     VIEW(Cam152[], gCamera);          gCamera_view: the datum gCamera, as an array of Cam152
 *     #define gCamera gCamera_view      and the file's code goes on naming it gCamera
 *
 * VIEW declares gCamera_view with the view's type and gCamera's own symbol (an assembler label), so the file's code
 * reads and writes the very same data and compiles exactly as it did with its old declaration of gCamera.
 * tools/port/to_headers.py writes these lines; tools/port/objsame.py shows the code is the same. */
#ifndef GAME_VIEW_H
#define GAME_VIEW_H

#define GAME_VIEW_STR(s) GAME_VIEW_STR2(s)
#define GAME_VIEW_STR2(s) #s
#define VIEW(T, x) extern __typeof__(T) x##_view __asm__(GAME_VIEW_STR(__USER_LABEL_PREFIX__) #x)

#endif /* GAME_VIEW_H */
