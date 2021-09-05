@echo off

if not exist resources.obj cl /nologo /c resources.c 

cl /nologo main.c /DUNICODE /utf-8 /O2 /link resources.obj /release /incremental:no /out:hangman.exe
del main.obj vc140.pdb
