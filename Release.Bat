Rem This updates the distribution area for informal release...
Rem It assumes that H: is mapped to Prv-TDS/Dev: somewhere.

Echo Copying syslogd files to prv-tds/dev:\release for internal release...
Copy syslogd.nlm		H:\release\syslogd
Copy syslogd.map		H:\release\syslogd
Copy syslogd.def		H:\release\syslogd
Copy msg\syslogd.msg	H:\release\syslogd
Copy changes			H:\release\syslogd\changes.log

echo --------------------------------------
echo Zipping up syslogd files for use by souls of great temerity...
del	syslogd.zip
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.nlm
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.map
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.def
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.imp
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.xdc
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.exp
tools\pkware\pkzipc.exe	-add -silent syslogd.zip syslogd.conf
tools\pkware\pkzipc.exe	-add -silent syslogd.zip release.bat
tools\pkware\pkzipc.exe	-add -silent syslogd.zip changes
tools\pkware\pkzipc.exe	-add -silent syslogd.zip makefile
tools\pkware\pkzipc.exe	-add -silent syslogd.zip msg\syslogd.msg
tools\pkware\pkzipc.exe	-add -silent syslogd.zip make\tools.mak
tools\pkware\pkzipc.exe	-add -silent syslogd.zip make\vers.mak
tools\pkware\pkzipc.exe	-add -silent syslogd.zip make\flags.mak
tools\pkware\pkzipc.exe	-add -silent syslogd.zip files.mak
tools\pkware\pkzipc.exe	-add -silent syslogd.zip libcpre.o
tools\pkware\pkzipc.exe	-add -silent syslogd.zip *.h
tools\pkware\pkzipc.exe	-add -silent syslogd.zip *.c
tools\pkware\pkzipc.exe	-sfx -silent syslogd.zip
