#= Tools ======================================================================
# Tools used to build source code and produce the library. Note that the
# orientation of the slashes is extremely critical. The backslash is essential
# in specifying the CodeWarrior license file because it is read by a Windows
# program (CodeWarrior). The other paths are used by GNU make and must be
# forward slashes because GNU is always an imitation UNIX environment.
#
LM_LICENSE_FILE	= $(LIB_ROOT)\Tools\Metro\license.dat

TOOLS_DIR			= $(LIB_ROOT)\tools

COMPILER				= $(TOOLS_DIR)\metro\mwccnlm.exe
WCC_COMPILER		= $(TOOLS_DIR)\watcom\wcc386.exe
GCC_COMPILER		= $(TOOLS_DIR)\gnu\gcc.exe
ASSEMBLER			= $(TOOLS_DIR)\metro\mwasmnlm.exe
LINKER				= $(TOOLS_DIR)\metro\mwldnlm.exe
LIBRARIAN			= $(TOOLS_DIR)\metro\mwldnlm.exe
WCC_LIBRARIAN		= $(TOOLS_DIR)\watcom\wlib.exe
MPKLISTER			= $(TOOLS_DIR)\mpkxdc.exe
MSGEXP				= $(TOOLS_DIR)\msg32\m32exp.exe
MSGEXTR				= $(TOOLS_DIR)\msg32\m32extr.exe
MSGMAKE				= $(TOOLS_DIR)\msg32\m32make.exe
MSGCHECK				= $(TOOLS_DIR)\msg32\m32check.exe
WUMPALI				= $(TOOLS_DIR)\wumpali.exe
OPUSMAKE				= $(TOOLS_DIR)\Opus\make.exe
ATBFILT				= $(TOOLS_DIR)\atbfilt.exe
DELZERO				= $(TOOLS_DIR)\delzero.exe
MKHELP				= $(TOOLS_DIR)\mkhelp.exe
TEE					= $(TOOLS_DIR)\tee.exe
ZIP					= $(TOOLS_DIR)\pkware\pkzipc.exe
RM						= $(TOOLS_DIR)\rm.exe
CP						= $(TOOLS_DIR)\cp.exe
