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

#include "SolverOptions.h"
#include <assert.h>

#include "NonSmoothDrivers.h"
#include "NCP_Solvers.h"
#include "NCP_cst.h"

char SICONOS_NCP_NEWTON_FBLSA_STR[] = "NCP Newton FBLSA";
char SICONOS_NCP_NEWTON_MINFBLSA_STR[] = "NCP Newton minFBLSA";
char SICONOS_NCP_PATHSEARCH_STR[] = "NCP Path search";
char SICONOS_NCP_PATH_STR[] = "NCP PATH";

int ncp_driver(NCP_struct* problem, double *z , double *F, SolverOptions* options,  NumericsOptions* global_options)
{
  assert(options && "ncp_driver null input for solver options.\n");

  /* Set global options */
  if (global_options)
    setNumericsOptions(global_options);

  /* Checks inputs */
  assert(problem && z && F && "ncp_driver null input for MixedComplementarityProblem and/or unknowns (z,w)");

  /* Output info. : 0: ok -  >0: error (which depends on the chosen solver) */
  int info = -1;

  switch (options->solverId)
  {
  case SICONOS_NCP_NEWTON_FBLSA: // Fischer-Burmeister + Newton w/ LS
    ncp_newton_FBLSA(problem, z, F, &info, options);
    break;
  case SICONOS_NCP_NEWTON_MINFBLSA: // min (+ FB as backup) + Newton w/ LS
    ncp_newton_minFBLSA(problem, z, F, &info, options);
    break;
  case SICONOS_NCP_PATHSEARCH: // pathsearch method
    ncp_pathsearch(problem, z, F, &info, options);
    break;
  case SICONOS_NCP_PATH: // PATH method
    ncp_path(problem, z, F, &info, options);
    break;
  default:
    fprintf(stderr, "ncp_driver error: unknown solver id: %d\n", options->solverId);
    exit(EXIT_FAILURE);
  }

  /* check the conditions 0 <= z _|_ F(z) >= 0 */
  if (options->filterOn > 0)
  {
    int info_ = ncp_compute_error(problem->n, z, F, options->dparam[0], &(options->dparam[1]));
    if (info <= 0) /* info was not set or the solver was happy */
      info = info_;
  }

  return info;
}
