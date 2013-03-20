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

set fop_path=..\fop-1.1
set docbook_input=.\custom_template-out.xml
set pdf_output=.\custom_template-out.pdf

cmd /c %fop_path%\fop.bat ^
-xml %docbook_input% ^
-xsl %fop_path%\docbook-xsl-1.78.0\fo\docbook.xsl ^
-pdf %pdf_output% ^
-param page.orientation landscape ^
-param paper.type USletter

:concat_files
set proto_files=%proto_files% %1
goto :eof

:concat_paths
set proto_paths=%proto_paths% --proto_path=.\%1
goto :eof