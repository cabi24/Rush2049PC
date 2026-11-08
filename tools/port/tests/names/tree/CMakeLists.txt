# rename.py's fixture: a list of stems and a list of paths.
foreach(f draft_old other_draft)
    list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/game/${f}.c)
endforeach()
add_executable(fixture src/blob/func_80F00010.c include/game/old_header.h src/blob/groups/old_group/member.c)
