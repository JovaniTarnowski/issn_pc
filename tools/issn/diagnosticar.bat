@echo off
rem Diagnóstico do dump de inFAMOUS Second Son.
rem Uso: arraste a pasta do jogo (a que tem eboot.bin) para cima deste arquivo,
rem ou rode:  diagnosticar.bat "D:\Jogos\CUSA00004"
chcp 65001 >nul
setlocal
if "%~1"=="" (
  echo Arraste a pasta do jogo ^(com eboot.bin^) para cima deste arquivo.
  pause
  exit /b 1
)
where py >nul 2>nul && (set PY=py -3) || (set PY=python)
%PY% --version >nul 2>nul || (
  echo Python 3 nao encontrado. Instale com:  winget install Python.Python.3.13
  pause
  exit /b 1
)
set PROBE=
if not "%~2"=="" set PROBE=--probe "%~2"
%PY% "%~dp0inspect_dump.py" "%~1" %PROBE%
echo.
pause
