@echo off
:: wrapper for install-windows-packages.ps1 - run from an ADMINISTRATOR
:: command prompt: right-click "cmd" -> "Als Administrator ausfuehren"
::   install-windows-packages.cmd              -> BASE MSVC
::   install-windows-packages.cmd BASE MSVC MINGW ENV
::   install-windows-packages.cmd -Check
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-windows-packages.ps1" %*
