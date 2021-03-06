/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2016 INRIA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


#include "NumericsOptions.h" // for global options
#include "FrictionContactProblem.h"
#include "SolverOptions.h"
#include "fc3d_compute_error.h"
#include "fc3d_projection.h"
#include "projectionOnCone.h"
#include "projectionOnCylinder.h"
#include "SiconosLapack.h"

#include <math.h>
#include <assert.h>

/* #define DEBUG_STDOUT */
/* #define DEBUG_MESSAGES */
#include "debug.h"
void fc3d_unitary_compute_and_add_error(double* restrict r , double* restrict u, double mu, double* restrict error)
{

  double normUT;
  double rho = 1.0;

  double worktmp[3];
  /* Compute the modified local velocity */
  normUT = hypot(u[1], u[2]); // i.e sqrt(u[ic3p1]*u[ic3p1]+u[ic3p2]*u[ic3p2]);
  worktmp[0] = r[0] - rho * (u[0] + mu * normUT);
  worktmp[1] = r[1] - rho * u[1] ;
  worktmp[2] = r[2] - rho * u[2] ;
  projectionOnCone(worktmp, mu);
  worktmp[0] = r[0] -  worktmp[0];
  worktmp[1] = r[1] -  worktmp[1];
  worktmp[2] = r[2] -  worktmp[2];
  *error +=  worktmp[0] * worktmp[0] + worktmp[1] * worktmp[1] + worktmp[2] * worktmp[2];

}
int fc3d_compute_error(
  FrictionContactProblem* problem,
  double *z , double *w, double tolerance,
  SolverOptions * options, double * error)
{
  assert(problem);
  assert(z);
  assert(w);
  assert(error);

  /* Computes w = Mz + q */
  int incx = 1, incy = 1;
  int nc = problem->numberOfContacts;
  int n = nc * 3;
  double *mu = problem->mu;

  cblas_dcopy(n , problem->q , incx , w , incy); // w <-q
  // Compute the current velocity
  prodNumericsMatrix(n, n, 1.0, problem->M, z, 1.0, w);


  *error = 0.;


  int ic, ic3;

  for (ic = 0, ic3 = 0 ; ic < nc ; ic++, ic3 += 3)
  {

    fc3d_unitary_compute_and_add_error(z + ic3, w + ic3, mu[ic], error);
  }
  *error = sqrt(*error);

  /* Computes error */
  double normq = cblas_dnrm2(n , problem->q , incx);
  DEBUG_PRINTF("normq = %12.8e\n", normq);
  *error = *error / (normq + 1.0);
  if (*error > tolerance)
  {
    if (verbose > 1)
      printf(" Numerics - fc3d_compute_error: error = %g > tolerance = %g.\n",
             *error, tolerance);
    return 1;
  }
  else
    return 0;
}



int fc3d_compute_error_velocity(FrictionContactProblem* problem, double *z , double *w, double tolerance, SolverOptions *options, double * error)
{
  /* Checks inputs */
  if (problem == NULL || z == NULL || w == NULL)
    numericsError("fc3d_compute_error", "null input for problem and/or z and/or w");

  /* Computes w = Mz + q */
  int incx = 1, incy = 1;
  int nc = problem->numberOfContacts;
  int n = nc * 3;
  double *mu = problem->mu;
  double worktmp[3] = {0.0, 0.0, 0.0};
  double invmu = 0.0;
  cblas_dcopy(n , problem->q , incx , z , incy); // z <-q

  // Compute the current reaction
  prodNumericsMatrix(n, n, 1.0, problem->M, w, 1.0, z);

  *error = 0.;
  double normUT = 0.0;
  double rho = 1.0;
  for (int ic = 0 ; ic < nc ; ic++)
  {
    /* Compute the modified local velocity */
    normUT = sqrt(w[ic * 3 + 1] * w[ic * 3 + 1] + w[ic * 3 + 2] * w[ic * 3 + 2]);
    worktmp[0] = w[ic * 3] + mu[ic] * normUT - rho * (z[ic * 3]);
    worktmp[1] = w[ic * 3 + 1] - rho * z[ic * 3 + 1] ;
    worktmp[2] = w[ic * 3 + 2] - rho * z[ic * 3 + 2] ;
    invmu = 1.0 / mu[ic];
    projectionOnCone(worktmp, invmu);
    normUT = sqrt(worktmp[1] * worktmp[1] + worktmp[2] * worktmp[2]);
    worktmp[0] = w[ic * 3] - (worktmp[0] - mu[ic] * normUT);
    worktmp[1] = w[ic * 3 + 1] -  worktmp[1];
    worktmp[2] = w[ic * 3 + 2] -  worktmp[2];
    *error +=  worktmp[0] * worktmp[0] + worktmp[1] * worktmp[1] + worktmp[2] * worktmp[2];
  }
  *error = sqrt(*error);

  /* Computes error */
  double normq = cblas_dnrm2(n , problem->q , incx);
  *error = *error / (normq + 1.0);
  if (*error > tolerance)
  {
    /*      if (verbose > 0) printf(" Numerics - fc3d_compute_error_velocity failed: error = %g > tolerance = %g.\n",*error, tolerance); */
    return 1;
  }
  else
    return 0;
}

void fc3d_Tresca_unitary_compute_and_add_error(double *z , double *w, double R, double * error)
{
  double rho = 1.0;
  double worktmp[3];
  /* Compute the modified local velocity */
  worktmp[0] = z[0] - rho * w[0];
  worktmp[1] = z[1] - rho * w[1] ;
  worktmp[2] = z[2] - rho * w[2] ;
  projectionOnCylinder(worktmp, R);
  worktmp[0] = z[0] -  worktmp[0];
  worktmp[1] = z[1] -  worktmp[1];
  worktmp[2] = z[2] -  worktmp[2];
  *error +=  worktmp[0] * worktmp[0]
    + worktmp[1] * worktmp[1]
    + worktmp[2] * worktmp[2];

}
int fc3d_Tresca_compute_error(FrictionContactProblem* problem,
                                           double *z, double * w,
                                           double tolerance, SolverOptions * options,
                                           double* error)
{
  /* Checks inputs */
  if (problem == NULL || z == NULL || w == NULL)
    numericsError("fc3d_Tresca_compute_error", "null input for problem and/or z and/or w");

  /* Computes w = Mz + q */
  int incx = 1, incy = 1;
  int nc = problem->numberOfContacts;
  int n = nc * 3;
  double R;
  cblas_dcopy(n , problem->q , incx , w , incy); // w <-q
  // Compute the current velocity
  prodNumericsMatrix(n, n, 1.0, problem->M, z, 1.0, w);

  *error = 0.;
  int ic, ic3;

  for (ic = 0, ic3 = 0 ; ic < nc ; ic++, ic3 += 3)
  {
    R = (options->dWork[ic]);
    fc3d_Tresca_unitary_compute_and_add_error(z+ic3 , w+ic3, R, error);
  }
  *error = sqrt(*error);

  /* Computes error */
  double normq = cblas_dnrm2(n , problem->q , incx);
  *error = *error / (normq + 1.0);
  if (*error > tolerance)
  {
    /* if (verbose > 0) printf(" Numerics - fc3d_Tresca_compute_error failed: error = %g > tolerance = %g.\n",*error, tolerance);  */
    return 1;
  }
  else
    return 0;
}
