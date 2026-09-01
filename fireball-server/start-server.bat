@echo off
echo 🔥 Starting Fireball Server on Windows...
if "%PORT%"=="" set PORT=9090
if "%HOST%"=="" set HOST=0.0.0.0

python beam_server.py --host %HOST% --port %PORT%
