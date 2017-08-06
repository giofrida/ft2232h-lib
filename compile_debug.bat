@echo off
set "concat="
for /f "delims=" %%a in ('dir lib\*.c /B') do (
	call set concat=%%concat%%lib\%%a 
)
set gccargs=-D DEBUG -Wall -Wextra %concat% -llibftdi1

echo Compiling %~n1.c
echo Arguments: %gccargs%

gcc -o %~dp1%~n1.exe %~dp1%~n1.c %gccargs%