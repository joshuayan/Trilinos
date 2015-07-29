# Do we need to list KLU2 as a dependency?
# Need a flag for LGPL vs BSD, SuperLU is required only for BSD.
#
# SET(LIB_REQUIRED_DEP_PACKAGES Teuchos Tpetra KLU2)
SET(LIB_REQUIRED_DEP_PACKAGES Teuchos Tpetra Amesos)
SET(LIB_OPTIONAL_DEP_PACKAGES Epetra EpetraExt )
SET(TEST_REQUIRED_DEP_PACKAGES Amesos)
SET(TEST_OPTIONAL_DEP_PACKAGES)
# SET(LIB_REQUIRED_DEP_TPLS SuperLU)
SET(LIB_REQUIRED_DEP_TPLS )
SET(LIB_OPTIONAL_DEP_TPLS MPI SuperLU SuperLUMT SuperLUDist PARDISO_MKL ParMETIS METIS Cholmod MUMPS)
SET(TEST_REQUIRED_DEP_TPLS)
SET(TEST_OPTIONAL_DEP_TPLS)
