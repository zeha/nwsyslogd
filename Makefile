.silent
.shell: .auto cmd.exe /c

LIB_ROOT	= .

.include files.mak
.include make\tools.mak
.include make\vers.mak
.include make\flags.mak

NLM_NAME		= syslogd
DESCRIPTION	= NetWare syslogd Daemon and Interfaces

#- Version and copyright information ------------------------------------------
#
COPYRIGHT_STRING	= "Copyright (c) 2004 by Novell, Inc. All rights reserved."
LIBRARY_VERSION	= 0

MAJOR_VERSION		= 1
MINOR_VERSION		= 0
REVISION				= 0


#- Build mode of buildmaster in effect ----------------------------------------
#
%if "$(BUILD_ROLE)" == "Buildmaster"
%if "$(BUILD_MODE)" == "Optimize"
FINAL	= $(LIB_ROOT)\final
%else
FINAL	= $(LIB_ROOT)\final_d
%endif
%else
FINAL	= $(LIB_ROOT)\final
%endif


C_SOURCES   = $[m,*.c,$[@,files.mak]]
OBJECT_LIST	= $[f,,$(C_SOURCES),.o]


#- Linking --------------------------------------------------------------------
# Rule and target to link the library. This is the first (default) target. The
# command, "make link," causes only the link portion of the process to run and
# is useful only if you are certain that all the intermediate objects are in
# order. Because of the sheer lethargy of GNU Make, this target was created as
# useful. It is the perfect command after performing a "make delbin."
#
syslogd.nlm:	$(OBJECT_LIST) syslogd.def
	%echo Linking syslogd.nlm from directives in syslogd.def...
	@$(RM)	syslogd.map syslogd.nvm
	@$(LINKER) -commandfile syslogd.def -zerobss -nostdlib -sym on			\
					-map syslogd.map -o syslogd.nlm $(OBJECT_LIST) libcpre.o	\
					| $(TEE) > link.err
	@$(DELZERO) link.err
	@$(RM) syslogd.ncv
	%echo ------------------------------------------------------------
	%echo syslogd.nlm build finished...

all:			syslogd.nlm

#- Link-definition file -------------------------------------------------------
# Target to build the link definition file which will direct the linker.
#
syslogd.def:	makefile syslogd.xdc msg\syslogd.msg libc.imp netware.imp
	@%echo Creating link commands for syslogd.nlm...
	@%echo Description	$(COMPOSED_DESCRIPTION) >  syslogd.def
	@%echo Version		$(COMPOSED_VERSION) >> syslogd.def
	@%echo Copyright	$(COPYRIGHT_STRING) >> syslogd.def
	@%echo Debug >> syslogd.def
	@%echo Flag_On		$(MULTIPLE) >> syslogd.def
	@%echo Start			_LibCPrelude >> syslogd.def
	@%echo Exit			_LibCPostlude >> syslogd.def
	@%echo XDCData		syslogd.xdc >> syslogd.def
	@%echo Messages		msg\syslogd.msg >> syslogd.def
	@%echo Module		libc >> syslogd.def
	@%echo Import		@libc.imp >> syslogd.def
	@%echo Import		@netware.imp >> syslogd.def
	@%echo Export		@syslogd.exp >> syslogd.def



#- Cross-domain data ----------------------------------------------------------
# Target to produce MPK-unsafe lists when warranted or, in the normal case, an
# XDC record that signals MPK that all functions in the NLM are completely
# MP-safe. These indications are placed in the cross-domain data (XDC) of the
# NLM.
#
# The list or MP-safe record is produced by MPKXDC.Exe and supplied as XDC data
# at link time. The -n switch will mean that all interfaces are MP-safe.
#
# The difference between this procedure and the way it was done for CLib is
# that we're just not going to concern ourselves with SMP NetWare.
#
syslogd.xdc:
	%echo ------------------------------------------------------------
	%echo Designating in XDC data that syslogd.nlm is multiprocessor-safe...
	@$(MPKLISTER) -n $@


#- Message file make or build procedure ---------------------------------------
# Create/Update the message database; sources must exist in a file on the path
# messages.h.
#
#- Message export target ------------------------------------------------------
#
# Export the messages from the message database to a message file. This is only
# the English one. The other languages are done by a different process and not
# by the development engineering team.
#
msg\syslogd.msg:	msg\syslogd.mdb
	%echo Updating syslogd.msg...
	@-$(MSGEXP) msg\syslogd cstring nlm msg\syslogd.msg


#- Message database target ----------------------------------------------------
# Extract the message source file (in the server library projects, this usually
# amounts only to 'messages.h') into the message database. If the message data-
# base doesn't exist, create it.
#
msg\syslogd.mdb:	messages.h
	@%echo Extracting syslogd message database from messages.h...
	@-$(MSGMAKE)  msg\syslogd syslogd $(MAJOR_VERSION).$(MINOR_VERSION) 4 1
	@-$(MSGEXTR) -mmsg\syslogd -f -w -smessages.h


#- Message data check ---------------------------------------------------------
# Verify the message database for errors including unverified text, undocu-
# mented pronouns (%s, %d, etc.) and other potential errors.
#
msgcheck:
	@-$(MSGCHECK) msg\syslogd


#- Intermediate and final object clean-up -------------------------------------
# Target to clear out subdirectories for a clean rebuild.
#
clean:	delbin
	%if %exists(*.o)
		@$(RM) -v *.o
	%endif
	%if %exists(*.err)
		@$(RM) -v *.err
	%endif
	%if %exists(*.lst)
		@$(RM) -v *.lst
	%endif
	%if %exists(*.e)
		@$(RM) -v *.e
	%endif


#- Binary-only clean-up -------------------------------------------------------
# Target to clean out all target binaries (not objects) before building. This
# is useful when it is not desirable to recompile everything.
#
delbin:
	@$(RM) syslogd.def syslogd.map syslogd.xdc syslogd.sym syslogd.nlm syslogd.err syslogd.exe

# - Copy files to unofficial release area -------------------------------------
# Used by development team to update the unofficial release area on Prv-TDS for
# internal, front-line developers that can't await official builds let alone
# SDK releases. This target assumes H: is mapped to Prv-TDS/Dev: somewhere.
#
release:		zip
	@%echo Copying syslogd files to Prv-TDS/Dev:\Release...
	@copy changes			H:\release\syslogd
	@copy syslogd.exe		H:\release\syslogd
	@copy syslogd.zip		H:\release\syslogd
	@copy syslogd.nlm		H:\release\syslogd
	@copy syslogd.sdk		H:\release\syslogd
	@copy syslogd.map		H:\release\syslogd
	@copy syslogd.def		H:\release\syslogd


# - Copy files to private NDK installation area -------------------------------
# Used by development team to update their own NDK release area for testing.
# This target assumes C:\Novell\NDK\syslogd is the NDK area.
ndk:		forcemake
	@%echo Updating private NDK installation area...
	@copy syslogd.nlm		C:\novell\ndk\syslogd\lib\debug
	@copy syslogd.map		C:\novell\ndk\syslogd\lib\debug
	@copy syslogd.sym		C:\novell\ndk\syslogd\lib\debug
	@copy syslogd.imp		C:\novell\ndk\syslogd\imports


#- Zip up the files for sending to friends ------------------------------------
# Target to zip up the essential for use by those who are living on the
# bleeding edge.
#
zip:		forcemake
	@%echo --------------------------------------
	@%echo Zipping up syslogd files for use by souls of great temerity...
	@$(RM)	syslogd.zip syslogd.exe
	@$(ZIP)	-add -silent syslogd.zip msg\syslogd.msg
	@$(ZIP)	-add -silent syslogd.zip syslogd.nlm
	@$(ZIP)	-add -silent syslogd.zip syslogd.map
	@$(ZIP)	-add -silent syslogd.zip syslogd.def
	@$(ZIP)	-add -silent syslogd.zip syslogd.sdk
	@$(ZIP)	-add -silent syslogd.zip changes
	@$(ZIP)	-sfx -silent syslogd.zip
	@%echo Created self-extracting archive syslogd.exe
	@%echo
	@%echo Don[squote]t forget to tell your user to unzip...
	@%echo ...using the command: syslogd.exe -dir

forcemake:

#----- Header paths -----------------------------------------------------------
# Preprocessor includes used:
#
INC_LIST	 = -I. -ISDK

#- Compilation rule -----------------------------------------------------------
# Rule to compile C source code using Metrowerks CodeWarrior C/C++ compiler.
# Use -v to help analyze nasty compilation problems that seem to defy all
# understanding!
#
#
FLAGS		= -nosyspath -nostdinc -r -char unsigned -enum int -align 1
WARNINGS	= -w noimplicit,nounwanted,unused,comma,nonotused,nounusedexpr,empty

%if "$(BUILD_MODE)" == "Optimize"
C_FLAGS 	= -opt full $(FLAGS) $(WARNINGS)
%else
C_FLAGS 	= -g        $(FLAGS) $(WARNINGS) -DDEBUG
%endif

.c.o:	$<
	@%echo Compiling $< to $@...
	@$(COMPILER) $(C_FLAGS) $(INC_LIST) -c $< | $(TEE) > $*.err
	@$(DELZERO) $*.err


#- Preprocessing rule ---------------------------------------------------------
# Rule to get C-preprocessor output...
#
.c.e:	$<
	@%echo Compiling $< to $@...
	@$(COMPILER) $(C_FLAGS) $(INC_LIST) -E $< > $*.e


# - Tools version discovery ---------------------------------------------------
# Display the version of each of the tools.
#
CC_VERSION	= $(COMPILER) -version
ASM_VERSION	= $(ASSEMBLER) -version
LD_VERSION	= $(LINKER) -version

version:
	@%echo ------------------------------------------
	@$(CC_VERSION)
	@%echo ------------------------------------------
	@$(ASM_VERSION)
	@%echo ------------------------------------------
	@$(LD_VERSION)



# - Build and install library in final directories ----------------------------
# Used by the buildmaster to completely build and copy all final objects ready
# for release.
#
final:	all
	%echo ------------------------------------------------------------
	@%echo Full build of syslogd.nlm has finished...
	@%echo Copying imports, log file, maps, messages and symbols...
	@copy	syslogd.nlm		$(FINAL)
	@copy	changes			$(FINAL)\logs\syslogd.log
	@copy	syslogd.map		$(FINAL)\maps
	@copy	syslogd.def		$(FINAL)\maps
	@copy	msg\syslogd.mdb	$(FINAL)\msg\4
	@copy	msg\syslogd.msg	$(FINAL)\msg\4 
