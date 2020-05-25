@echo off

SET "source=%~dp0"
SET "builddir=%source%..\build"

SET "filename=handmade"

REM TODO(bjorn): Comment out what the flags represent.
SET defines=-DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 
SET errors=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -wd4101
SET flags=-MTd -nologo -GR -EHa -O2 -Oi -Z7 -FC -I..\library\tobii_4C\include
SET asmflags=/c /Zi 
SET links=user32.lib gdi32.lib winmm.lib tobii_stream_engine.lib opengl32.lib
SET linkflags=-LIBPATH:..\library\tobii_4C\lib\tobii -incremental:no -opt:ref

if not exist "%builddir%" mkdir "%builddir%"
pushd "%builddir%"

del *.pdb >nul 2>nul

copy ..\library\tobii_4C\lib\tobii\tobii_stream_engine.dll . >nul 2>&1

REM 64-bit build

ml64 /Fo handmade_intrinsics.obj %asmflags% "%source%\handmade_intrinsics.asm"

cl %defines% %errors% %flags% -Fmwin32_game.map "%source%\win32_game.cpp" -link %linkflags% %links% -PDB:win32_game.pdb

cl %defines% %errors% %flags% -Fm "%source%\%filename%.cpp" -LD -link %linkflags% handmade_intrinsics.obj -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -PDB:game_%random%.pdb -OUT:game.dll

REM 32-bit build
REM TODO(bjorn): compile for 32-bit, use vcvarsall?
REM cl %defines% %errors% %flags% %platformsource% %linker_flags% -subsystem:windows,5.1 %links%

@xcopy ..\temp . /Y >nul 2>&1

popd
