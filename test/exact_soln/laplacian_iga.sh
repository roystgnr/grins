#!/bin/bash

set -e

INPUT="${GRINS_TEST_INPUT_DIR}/laplacian_iga.in"
TESTDATA="./laplacian_iga.xda"

${LIBMESH_RUN:-} ${GRINS_BUILDSRC_DIR}/grins \
  $INPUT \
  $PETSC_OPTIONS

${LIBMESH_RUN:-} ${GRINS_TEST_DIR}/generic_exact_solution_testing_app \
  --input $INPUT \
  vars='u' norms='L2' tol='1e-8' \
  u_L2_error='0' \
  u_exact_soln='1.5' \
  test_data=$TESTDATA

# Now remove the test turd
rm $TESTDATA
