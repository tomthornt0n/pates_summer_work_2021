@echo off

cl /nologo main.c /DUNICODE /Zi /link /debug /incremental:no /out:gtin8_utils.exe
del main.obj vc140.pdb
