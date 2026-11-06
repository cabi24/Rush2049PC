@echo off
setlocal
rem Joins a friend's online race (README.txt, "Playing online"): asks for the host's
rem address, which their waiting screen shows (a Tailscale name or 100.x address).
rem Options given to this file are passed on, e.g. a join code:
rem     "Join online race.bat" --net-code SECRET
set "HOST="
set /p "HOST=The host's address (their Tailscale name or 100.x address): "
if not defined HOST exit /b 1
"%~dp0rush2049pc.exe" --net-join "%HOST%" %*
if errorlevel 1 pause
