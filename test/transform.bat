set fop_path=%1
set docbook_input=%2
set pdf_output=%3

cmd /c %fop_path%\fop.bat ^
-xml %docbook_input% ^
-xsl %fop_path%\docbook-xsl-1.78.0\fo\docbook.xsl ^
-pdf %pdf_output% ^
-param page.orientation landscape ^
-param paper.type USletter ^
-param body.font.family sans-serif
