#-----------------------------------------------------------------------------
# These are the FLAG_ON values for use with the CodeWarrior linker. To a large
# extent, these are more documentary than useful in building the library.
# Just stack them up in a list called NLMFLAGS in Makefile after including
# this file and MLink.Mak will set them up using %foreach.
#
REENTRANT			= 0x00000001
MULTIPLE				= 0x00000002
SYNCHRONIZE			= 0x00000004
PSEUDOPREEMPTION	= 0x00000008
PREEMPTION			= 0x00000008
OS_DOMAIN			= 0x00000010
AUTOUNLOAD			= 0x00000040
TSR					= 0x01000000
UNICODE_STRINGS	= 0x02000000
