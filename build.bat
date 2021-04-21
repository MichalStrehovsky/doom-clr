@echo off
md runtime
pushd runtime
cl ..\linuxdoom-1.10\*.c /wd 4113 /wd 4311 /wd 4047 /wd 4312 /wd 4020 /wd 4700 /wd 4133 /Dalloca=_alloca /Dstrcasecmp=stricmp /Dstrncasecmp=strnicmp /link /out:doom_crt.exe
popd