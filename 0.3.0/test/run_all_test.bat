@echo off

for /R /D %%f in (test-*) do (
	echo %%f
	cd %%f
	cmd /c run.bat
)