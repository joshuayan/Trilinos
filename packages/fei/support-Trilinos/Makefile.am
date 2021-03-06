# @HEADER
# ************************************************************************
# 
#              Finite Element Interface to Linear Solvers
#                 Copyright (2001) Sandia Corporation
# 
# Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
# license for use of this work by or on behalf of the U.S. Government.
# 
# Questions? Contact Alan Williams (william@sandia.gov) 
# 
# ************************************************************************
# @HEADER

# The following line helps the test harness recover from build errors.

all-local:
	@echo ""
	@echo "Trilinos package fei subdirectory support-Trilinos built successfully."
	@echo ""

include $(top_builddir)/Makefile.export.fei

# ------------------------------------------------------------------------
# For each category, create two variables - NAME and NAME_H. The
# second is the list of headers to be installed, i.e., any header that
# might someday be needed by some other code outside FEI. The first is
# the list of all source and any other header files.
# ------------------------------------------------------------------------

DEFS = 

DEFAULT_INCLUDES = $(FEI_INCLUDES)

CORE_H = \
	$(top_srcdir)/support-Trilinos/fei_Aztec_BlockMap.hpp \
	$(top_srcdir)/support-Trilinos/fei_AztecDMSR_Matrix.hpp \
	$(top_srcdir)/support-Trilinos/fei_AztecDVBR_Matrix.hpp \
	$(top_srcdir)/support-Trilinos/fei_Aztec_LinSysCore.hpp \
	$(top_srcdir)/support-Trilinos/fei_Aztec_Map.hpp \
	$(top_srcdir)/support-Trilinos/fei_Aztec_Vector.hpp \
	$(top_srcdir)/support-Trilinos/fei_Factory_Trilinos.hpp \
	$(top_srcdir)/support-Trilinos/fei_Include_Trilinos.hpp \
	$(top_srcdir)/support-Trilinos/fei_LinProbMgr_EpetraBasic.hpp \
	$(top_srcdir)/support-Trilinos/fei_MatrixTraits_Epetra.hpp \
	$(top_srcdir)/support-Trilinos/fei_Solver_Amesos.hpp \
	$(top_srcdir)/support-Trilinos/fei_Solver_AztecOO.hpp \
	$(top_srcdir)/support-Trilinos/fei_Trilinos_Helpers.hpp \
	$(top_srcdir)/support-Trilinos/fei_trilinos_macros.hpp \
	$(top_srcdir)/support-Trilinos/fei_VectorTraits_Epetra.hpp

CORE = \
	$(srcdir)/fei_Aztec_BlockMap.cpp \
	$(srcdir)/fei_AztecDMSR_Matrix.cpp \
	$(srcdir)/fei_AztecDVBR_Matrix.cpp \
	$(srcdir)/fei_Aztec_LinSysCore.cpp \
	$(srcdir)/fei_Aztec_Map.cpp \
	$(srcdir)/fei_Aztec_Vector.cpp \
	$(srcdir)/fei_Factory_Trilinos.cpp \
	$(srcdir)/fei_LinProbMgr_EpetraBasic.cpp \
	$(srcdir)/fei_Solver_Amesos.cpp \
	$(srcdir)/fei_Solver_AztecOO.cpp \
	$(srcdir)/fei_Trilinos_Helpers.cpp

# ------------------------------------------------------------------------
# FEI library specifications
# ------------------------------------------------------------------------

FEI_LIB = libfei_trilinos.a


FEI_H = \
	$(CORE_H)

libfei_trilinos_a_SOURCES = \
	$(CORE) 

EXTRA_libfei_trilinos_a_SOURCES = 

# ------------------------------------------------------------------------
# For using a special archiver
# ------------------------------------------------------------------------

# SPECIAL NOTE: Why is the "else" there?  For some reason, automake
# fails to define <lib>_AR if USE_ALTERNATE_AR is false, so we have to
# define it manually.

if USE_ALTERNATE_AR

libfei_trilinos_a_AR = $(ALTERNATE_AR)
else

libfei_trilinos_a_AR = $(AR) cru

endif

# ------------------------------------------------------------------------
# Some C++ compilers create extra .o-files for templates. We need to
# be sure to include these, and this is the hack to do it.
# On alpha-dec machines, the extra .o's are needed for Epetra only.
# ------------------------------------------------------------------------

libfei_trilinos_a_LIBADD = $(FEI_TEMPLATE_OBJS) $(XTRALDADD)

# ------------------------------------------------------------------------
# List of all libraries to install in $(libexecdir)
# ------------------------------------------------------------------------

lib_LIBRARIES = $(FEI_LIB)

# ------------------------------------------------------------------------
# List of all headers to install in $(includedir)
# ------------------------------------------------------------------------

include_HEADERS = $(FEI_H) 

# ------------------------------------------------------------------------
# Files to be deleted by 'make maintainer-clean'
# ------------------------------------------------------------------------

MAINTAINERCLEANFILES = Makefile.in 

