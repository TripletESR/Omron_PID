@echo off
title Deploying Omron_PID
echo =====================================================
echo This is batch script for deploying Omron_PID in winodws
Set desktop=%USERPROFILE%\Desktop
echo Desktop PATH: %Desktop%
echo =====================================================

set /p version="   version? (5_7_0)"
set /p qt_bin=" Qt bin? (5.7)"

Set name=Omron_PID.exe
Set origin=%desktop%\build-Omron_PID-Desktop_Qt_%version%_MinGW_32bit-Release\release
Set destination=%desktop%\Omron_PID_release\
Set compiler=C:\Qt\%qt_bin%\mingw53_32\bin\

rem -----------------------------------------
echo =====================================================
echo Copy *.exe to %destination%
echo =====================================================
xcopy /I/Y %origin%\%name% %destination%

rem -----------------------------------------
echo =====================================================
set /p deployFlag="  Deploy Application? (Y/N)"
echo =====================================================

IF "%deployFlag%"=="Y" (
	rem -----------------------------------------
	echo Use windepoly to create nessacary *.dll
	cd %compiler%
	echo %cd%
	windeployqt.exe %destination%\%name%
)

echo --------------- bat script finished. ----------------
pause
