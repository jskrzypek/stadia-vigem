mkdir bin
mkdir obj
mkdir res\generated

mc.exe -U src/messages.mc -h src/ -r res/generated
rc.exe /Foobj/StadiaViGEm.Service.Messages.res res/generated/messages.rc
link.exe -dll -noentry -out:bin/StadiaViGEm.Service.Resources.dll obj/StadiaViGEm.Service.Messages.res

rc.exe /Foobj/StadiaViGEm.Service.res res/res.rc
cl.exe /Zi /Od /EHsc /DWIN32 /D_UNICODE /DUNICODE /Iinclude /Foobj/ /Febin/StadiaViGEm.Service.exe lib/ViGEmClient/*.cpp obj/StadiaViGEm.Service.res src/*.c