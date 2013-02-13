@echo off
setlocal enabledelayedexpansion

set proto_files=
::for /r %%i in (*.proto) DO call :concat_files %%i

for /R . %%f in (*.proto) do (
  set B=%%f
  call :concat_files !B:%CD%\=!
)
::echo proto_files = %proto_files%

set proto_paths=
for /d %%i in (*) DO call :concat_paths %%i
::echo proto_path = %proto_paths%

cmd /c ..\protoc.exe ^
--proto_path=.\ ^
%proto_paths% ^
%proto_files% ^
--docbook_out=.

cmd /c ..\transform.bat ..\fop-1.1 .\docbook_out.xml .\docbook_out.pdf

:concat_files
set proto_files=%proto_files% %1
goto :eof

:concat_paths
set proto_paths=%proto_paths% --proto_path=.\%1
goto :eof