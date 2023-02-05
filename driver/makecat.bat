del *.cat
"c:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x86\infverif.exe" /v /u xchub.inf
"c:\Program Files (x86)\Windows Kits\10\bin\x86\Inf2Cat.exe" /driver:.\ /os:2000,XP_X86,XP_X64,Server2003_X86,Server2003_X64,Vista_X86,Vista_X64,Server2008_X86,Server2008_X64,7_X86,7_X64,Server2008R2_X64,8_X86,8_X64,Server8_X64,6_3_X86,6_3_X64,Server6_3_X64,10_X86,10_X64 /uselocaltime /v
"c:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\signtool.exe" sign /f ..\..\codesign.pfx /p password /fd sha256 /t http://timestamp.sectigo.com xchub.cat
pause