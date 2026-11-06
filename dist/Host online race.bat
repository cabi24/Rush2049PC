@echo off
rem Hosts an online race (README.txt, "Playing online"). The window lists this PC's
rem addresses; your friend starts "Join online race.bat" and types one of them in.
rem Options given to this file are passed on, e.g. a join code:
rem     "Host online race.bat" --net-code SECRET
"%~dp0rush2049pc.exe" --net-host %*
if errorlevel 1 pause
