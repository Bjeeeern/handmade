@echo off

REM start cmd /c devenv ..\debug\win32_handmade.sln

REM SET fileName=%1
REM SET lineNum=%2
REM 
REM REM configuration
REM SET executable="%~dp0..\build\win32_game.exe"
REM SET workingDir="%~dp0..\build\"
REM SET codeclap="w:\misc\lib\codeclap\codeclap.exe"
REM 
REM if "%~1" == "" (
REM     %codeclap% --chdir %workingDir% %executable% %fileName%:%lineNum%
REM ) else (
REM     %codeclap% --chdir %workingDir% %executable%
REM )

start cmd /c ..\debug\remedybg\remedybg.exe ..\debug\session.rdbg
