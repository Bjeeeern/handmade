@echo off

REM TODO(bjorn): Comment out what the flags represent.

SET defines=-DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 
SET errors=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -wd4101
SET flags=-MTd -nologo -GR -EHa -Od -Oi -Z7 -FC 
SET asmflags=/c /Zi 
SET links=user32.lib gdi32.lib winmm.lib
SET linkflags=-incremental:no -opt:ref

if not exist "%builddir%" mkdir "%builddir%"
pushd "%builddir%"

del *.pdb >nul 2>nul

REM 64-bit build

ml64 /Fo handmade_intrinsics.obj %asmflags% %platformdir%\handmade_intrinsics.asm

SET "platformsource=%platformdir%\win32_game.cpp"

cl %defines% %errors% %flags% -Fmwin32_game.map "%platformsource%" -link %linkflags% %links% -PDB:win32_game.pdb

REM 32-bit build
REM TODO(bjorn): compile for 32-bit, use vcvarsall?
REM cl %defines% %errors% %flags% %platformsource% %linker_flags% -subsystem:windows,5.1 %links%

popd

REM EXIT /B
REM :EOF
