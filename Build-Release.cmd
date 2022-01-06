mkdir bin
mkdir obj
rc.exe /foobj/stadia-vigem.res res/res.rc
cl.exe /GL /Zi /O2 /EHsc /DWIN32 /D_UNICODE /DUNICODE /IViGEmClient/include /Foobj/ /Febin/stadia-vigem.exe ViGEmClient/src/*.cpp obj/stadia-vigem.res src/*.c