#------------------------------------------------------------------------------
# A concatenation of the version information to create the version for the
# linker plus an alpha string that goes on internal builds warning that they
# aren't to be distributed.
#
COMPOSED_VERSION = $(MAJOR_VERSION) $(MINOR_VERSION) $(REVISION)
ALPHA_STRING	  = , á$(LIBRARY_VERSION)

#------------------------------------------------------------------------------
# Addendum to description; change from " [Unauthorized version]" to "" for any
# released products. If à or á are used, append the library version number.
# Either the description tail or at least the image type (profiling, optimized
# or debugging) should be removed for official, shipping builds. Builds pro-
# duced by the engineering (who do not define 'BUILDMASTER') team get the
# string, "unauthorized," added in.
#
%if "$(BUILD_MODE)" == "Optimize"
IMAGE_TYPE			= optimized
%else											# DEFAULT: debuggable libraries
IMAGE_TYPE			= debugging
%endif

%if "$(BUILD_USE)" == "Shipping"		# make a null tail...
DESCRIPTION_TAIL	=
%else
%if "$(BUILD_ROLE)" == "Buildmaster"
DESCRIPTION_TAIL	= [$(IMAGE_TYPE), $(LIBRARY_VERSION)]
%else
DESCRIPTION_TAIL	= [unauthorized, $(IMAGE_TYPE)$(ALPHA_STRING)]
%endif
%endif

%if %defined(DESCRIPTION_TAIL)
COMPOSED_DESCRIPTION	= "$(DESCRIPTION) $(DESCRIPTION_TAIL)"
%else											# if shipping, leave off the tail...
COMPOSED_DESCRIPTION	= "$(DESCRIPTION)"
%endif
