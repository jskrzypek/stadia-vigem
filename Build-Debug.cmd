mkdir bin
mkdir Foobj
mkdir obj
rc.exe /foobj/stadia-vigem.res res/res.rc
cl.exe /Zi /Od /EHsc /DWIN32 /D_UNICODE /DUNICODE /Iinclude /Foobj/ /Febin/stadia-vigem.exe lib/ViGEmClient/*.cpp obj/stadia-vigem.res src/*.c