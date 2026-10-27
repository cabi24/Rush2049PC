# cmake -DOUT=file -DIN=a|b|c -P concat.cmake: OUT is the files of IN one after another.
string(REPLACE "|" ";" IN "${IN}")
set(text "")
foreach(f ${IN})
    file(READ ${f} t)
    string(APPEND text "${t}\n")
endforeach()
file(WRITE ${OUT} "${text}")
