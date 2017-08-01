@echo off
set "concat="
for /f "delims=" %%a in ('dir lib\*.c /B') do (
	call set concat=%%concat%%lib\%%a 
)
@echo on

gcc -o %~n1.exe %~n1.c %concat% -llibftdi1