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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include <assert.h>

#include "SiconosLapack.h"
#include "NumericsOptions.h"
#include "gfc3d_Solvers.h"
#include "NonSmoothDrivers.h"
#include "fc3d_Solvers.h"
#include "cond.h"
#include "pinv.h"
#include <string.h>

#include "sanitizer.h"

//#define TEST_COND
//#define OUTPUT_DEBUG
extern int *Global_ipiv;
extern int  Global_MisInverse;
extern int  Global_MisLU;

#pragma GCC diagnostic ignored "-Wmissing-prototypes"

/* Global Variable for the reformulation of the problem */

int reformulationIntoLocalProblem(GlobalFrictionContactProblem* problem, FrictionContactProblem* localproblem)
{
  int info = -1;

  NumericsMatrix *M = problem->M;
  NumericsMatrix *H = problem->H;


  localproblem->numberOfContacts = problem->numberOfContacts;
  localproblem->dimension =  problem->dimension;
  localproblem->mu =  problem->mu;

  assert(M);
  assert(H);

  if (H->storageType != M->storageType)
  {
    //     if(verbose==1)
    printf(" ->storageType != M->storageType :This case is not taken into account\n");
    return info;
  }

  if (M->storageType == 0)
  {


    int n = M->size0;
    int m = H->size1;
    int nm = n * m;
    int infoDGETRF = 0;
    int infoDGETRS = 0;
    Global_ipiv = (int *)malloc(n * sizeof(int));


    double *Htmp = (double*)malloc(nm * sizeof(double));
    // compute W = H^T M^-1 H
    //Copy Htmp <- H
    cblas_dcopy_msan(nm,  H->matrix0 , 1, Htmp, 1);
    //Compute Htmp   <- M^-1 Htmp
    Global_MisLU = 0; /*  Assume that M is not already LU */
    DGETRF(n, n, M->matrix0, n, Global_ipiv, &infoDGETRF);
    assert(!infoDGETRF);
    Global_MisLU = 1;
    DGETRS(LA_NOTRANS, n, m,  M->matrix0, n, Global_ipiv, Htmp, n, &infoDGETRS);

    assert(!infoDGETRS);
    /*      DGESV(n, m, M->matrix0, n, ipiv, Htmp, n, infoDGESV); */

    localproblem->M = newNumericsMatrix();
    NumericsMatrix *Wnum = localproblem->M;
    Wnum->storageType = 0;
    Wnum-> size0 = m;
    Wnum-> size1 = m;
    Wnum->matrix0 = (double*)malloc(m * m * sizeof(double));
    Wnum->matrix1 = NULL;
    Wnum->matrix2 = NULL;
    Wnum->internalData = NULL;
    // Compute W <-  H^T M^1 H



    assert(H->matrix0);
    assert(Htmp);
    assert(Wnum->matrix0);



    cblas_dgemm(CblasColMajor,CblasTrans, CblasNoTrans, m, m, n, 1.0, H->matrix0, n, Htmp, n, 0.0, Wnum->matrix0, m);
    /*     DGEMM(CblasTrans,CblasNoTrans,m,m,n,1.0,H->matrix0,n,Htmp,n,0.0,Wnum->matrix0,m); */

    // compute localq = H^T M^(-1) q +b

    //Copy localq <- b
    localproblem->q = (double*)malloc(m * sizeof(double));
    cblas_dcopy_msan(m, problem->b , 1, localproblem->q, 1);

    double* qtmp = (double*)malloc(n * sizeof(double));
    cblas_dcopy_msan(n,  problem->q, 1, qtmp, 1);

    // compute H^T M^(-1) q + b

    assert(Global_MisLU);
    DGETRS(LA_NOTRANS, n, 1,  M->matrix0, n, Global_ipiv, qtmp , n, &infoDGETRS);

    /*      DGESV(n, m, M->matrix0, n, ipiv, problem->q , n, infoDGESV); */

    cblas_dgemv(CblasColMajor,CblasTrans, n, m, 1.0, H->matrix0 , n, qtmp, 1, 1.0, localproblem->q, 1);
    // Copy mu
    localproblem->mu = problem->mu;



    free(Htmp);
    free(qtmp);


  }

  else
  {
    int n = M->size0;
    int m = H->size1;

    assert(!Global_ipiv);
    Global_ipiv = (int *)malloc(n * sizeof(int));

    // compute W = H^T M^-1 H
    //Copy Htmp <- H
    SparseBlockStructuredMatrix *HtmpSBM = (SparseBlockStructuredMatrix*)malloc(sizeof(SparseBlockStructuredMatrix));
    /* copySBM(H->matrix1 , HtmpSBM); */

#ifdef OUTPUT_DEBUG
    FILE* fileout;
    fileout = fopen("dataM.sci", "w");
    printInFileForScilab(M, fileout);
    fclose(fileout);
    printf("Display M\n");
    printSBM(M->matrix1);
#endif
    //Compute Htmp   <- M^-1 HtmpSBM
    /* DGESV(n, m, M->matrix0, n, ipiv, Htmp, n, infoDGESV); */
    assert(!inverseDiagSBM(M->matrix1));
    Global_MisInverse = 1;
#ifdef OUTPUT_DEBUG
    fileout = fopen("dataMinv.sci", "w");
    printInFileForScilab(M, fileout);
    fclose(fileout);
    printf("Display Minv\n");
    printSBM(M->matrix1);
#endif
    allocateMemoryForProdSBMSBM(M->matrix1, H->matrix1, HtmpSBM);
    double alpha = 1.0, beta = 1.0;

    prodSBMSBM(alpha, M->matrix1, H->matrix1, beta, HtmpSBM);
#ifdef OUTPUT_DEBUG
    fileout = fopen("dataH.sci", "w");
    printInFileForScilab(H, fileout);
    fclose(fileout);
    printf("Display H\n");
    printSBM(H->matrix1);

    fileout = fopen("dataHtmpSBM.sci", "w");
    printInFileSBMForScilab(HtmpSBM, fileout);
    fclose(fileout);
    printf("Display HtmpSBM\n");
    printSBM(HtmpSBM);
#endif





    SparseBlockStructuredMatrix *Htrans = (SparseBlockStructuredMatrix*)malloc(sizeof(SparseBlockStructuredMatrix));
    transposeSBM(H->matrix1, Htrans);
#ifdef OUTPUT_DEBUG
    fileout = fopen("dataHtrans.sci", "w");
    printInFileSBMForScilab(Htrans, fileout);
    fclose(fileout);
    printf("Display Htrans\n");
    printSBM(Htrans);
#endif
    localproblem->M = newNumericsMatrix();
    NumericsMatrix *Wnum = localproblem->M;
    Wnum->storageType = 1;
    Wnum-> size0 = m;
    Wnum-> size1 = m;
    Wnum->matrix1 = (SparseBlockStructuredMatrix*)malloc(sizeof(SparseBlockStructuredMatrix));
    Wnum->matrix0 = NULL;
    SparseBlockStructuredMatrix *W =  Wnum->matrix1;

    allocateMemoryForProdSBMSBM(Htrans, HtmpSBM, W);
    prodSBMSBM(alpha, Htrans, HtmpSBM, beta, W);
#ifdef OUTPUT_DEBUG
    fileout = fopen("dataW.sci", "w");
    printInFileForScilab(Wnum, fileout);
    fclose(fileout);
    printf("Display W\n");
    printSBM(W);
#endif

#ifdef TEST_COND
    NumericsMatrix *WnumInverse = newNumericsMatrix();
    WnumInverse->storageType = 0;
    WnumInverse-> size0 = m;
    WnumInverse-> size1 = m;
    WnumInverse->matrix1 = NULL;
    WnumInverse->matrix2 = NULL;
    WnumInverse->internalData = NULL;
    WnumInverse->matrix0 = (double*)malloc(m * m * sizeof(double));
    double * WInverse = WnumInverse->matrix0;
    SBMtoDense(W, WnumInverse->matrix0);

    FILE * file1 = fopen("dataW.dat", "w");
    printInFileForScilab(WnumInverse, file1);
    fclose(file1);

    double * WInversetmp = (double*)malloc(m * m * sizeof(double));
    memcpy(WInversetmp, WInverse, m * m * sizeof(double));
    double  condW;
    condW = cond(WInverse, m, m);

    int* ipiv = (int *)malloc(m * sizeof(*ipiv));
    int infoDGETRF = 0;
    DGETRF(m, m, WInverse, m, ipiv, &infoDGETRF);
    assert(!infoDGETRF);
    int infoDGETRI = 0;
    DGETRI(m, WInverse, m, ipiv, &infoDGETRI);


    free(ipiv);
    assert(!infoDGETRI);


    double  condWInverse;
    condWInverse = cond(WInverse, m, m);




    FILE * file2 = fopen("dataWInverse.dat", "w");
    printInFileForScilab(WnumInverse, file2);
    fclose(file2);

    double tol = 1e-24;
    pinv(WInversetmp, m, m, tol);
    NumericsMatrix *WnumInversetmp = newNumericsMatrix();
    WnumInversetmp->storageType = 0;
    WnumInversetmp-> size0 = m;
    WnumInversetmp-> size1 = m;
    WnumInversetmp->matrix1 = NULL;
    WnumInversetmp->matrix2 = NULL;
    WnumInversetmp->internalData = NULL;
    WnumInversetmp->matrix0 = WInversetmp ;

    FILE * file3 = fopen("dataWPseudoInverse.dat", "w");
    printInFileForScilab(WnumInversetmp, file3);
    fclose(file3);


    free(WInverse);
    free(WInversetmp);
    free(WnumInverse);
#endif

    localproblem->q = (double*)malloc(m * sizeof(double));
    //Copy q<- b
    cblas_dcopy_msan(m, problem->b  , 1, localproblem->q, 1);
    // compute H^T M^-1 q+ b
    double* qtmp = (double*)malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) qtmp[i] = 0.0;
    double beta2 = 0.0;
    prodSBM(n, n, alpha, M->matrix1, problem->q, beta2, qtmp);
    prodSBM(n, m, alpha, Htrans, qtmp, beta, localproblem->q);

    localproblem->mu = problem->mu;

    /*     FILE * filecheck = fopen("localproblemcheck.dat","w"); */
    /*     frictionContact_printInFile(localproblem,filecheck); */
    /*     fclose(filecheck); */

    freeSBM(HtmpSBM);
    freeSBM(Htrans);
    free(HtmpSBM);
    free(Htrans);
    free(qtmp);
  }


  return info;
}
int computeGlobalVelocity(GlobalFrictionContactProblem* problem, double * reaction, double * globalVelocity)
{
  int info = -1;

  if (problem->M->storageType == 0)
  {
    int n = problem->M->size0;
    int m = problem->H->size1;


    /* Compute globalVelocity   <- H reaction + q*/

    /* globalVelocity <- problem->q */
    cblas_dcopy(n,  problem->q , 1, globalVelocity, 1);
    /* globalVelocity <-  H*reaction + globalVelocity*/
    cblas_dgemv(CblasColMajor,CblasNoTrans, n, m, 1.0, problem->H->matrix0 , n, reaction , 1, 1.0, globalVelocity, 1);
    /* Compute globalVelocity <- M^(-1) globalVelocity*/
    assert(Global_ipiv);
    assert(Global_MisLU);
    int infoDGETRS = 0;
    DGETRS(LA_NOTRANS, n, 1,   problem->M->matrix0, n, Global_ipiv, globalVelocity , n, &infoDGETRS);

    assert(!infoDGETRS);

    free(Global_ipiv);
    Global_ipiv = NULL;


  }
  else
  {
    int n = problem->M->size0;
    int m = problem->H->size1;

    /* Compute qtmp   <- H reaction + q*/

    double* qtmp = (double*)malloc(n * sizeof(double));
    double alpha = 1.0;
    double beta = 1.0;

    cblas_dcopy_msan(n,  problem->q , 1, qtmp, 1);
    prodSBM(m, n, alpha, problem->H->matrix1, reaction, beta, qtmp);
    /* Compute global velocity = M^(-1) qtmp*/


    /*      inverseDiagSBM(M->matrix1); We assume that M->matrix1 is already inverse*/
    assert(Global_MisInverse);

    double beta2 = 0.0;
    prodSBM(n, n, alpha,  problem->M->matrix1, qtmp, beta2, globalVelocity);

    free(qtmp);
    free(Global_ipiv);
    Global_ipiv = NULL;
  }

  return info;
}

int freeLocalProblem(FrictionContactProblem* localproblem)
{
  int info = -1;

  /*    if (!localproblem->M->storageType) */
  /*  { */
  if (localproblem->M->matrix0)
    free(localproblem->M->matrix0);
  /*  } */
  /*     else */
  /*  { */
  if (localproblem->M->matrix1)
  {
    freeSBM(localproblem->M->matrix1);
    free(localproblem->M->matrix1);
  }
  /*  } */
  free(localproblem->M);
  free(localproblem->q);
  free(localproblem);
  return info;
}



void  gfc3d_nsgs_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{

  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);

  fc3d_nsgs(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);


}
int gfc3d_nsgs_wr_setDefaultSolverOptions(SolverOptions* options)
{


  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the NSGS_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_NSGS_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  fc3d_nsgs_setDefaultSolverOptions(options->internalSolvers);
  return 0;
}



void  gfc3d_globalAlartCurnier_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{

  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);


  // From StorageType==1 to Storage==0
  if (localproblem->M->storageType == 1)
  {
    printf("Warning: fc3d_globalAlartCurnier is only implemented for dense matrices.\n");
    printf("Warning: The problem is reformulated.\n");

    localproblem->M->matrix0 = (double*)malloc(localproblem->M->size0 * localproblem->M->size1 * sizeof(double));

    SBMtoDense(localproblem->M->matrix1, localproblem->M->matrix0);

    freeSBM(localproblem->M->matrix1);
    free(localproblem->M->matrix1);
    localproblem->M->matrix1 = NULL;
    localproblem->M->matrix2 = NULL;
    localproblem->M->internalData = NULL;
    localproblem->M->storageType = 0;
  }


  //
  fc3d_nonsmooth_Newton_AlartCurnier(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);


}
int gfc3d_globalAlartCurnier_wr_setDefaultSolverOptions(SolverOptions* options)
{


  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the NSN_AC_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_NSN_AC_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  gfc3d_nonsmooth_Newton_AlartCurnier_setDefaultSolverOptions(options->internalSolvers);
  return 0;
}

void  gfc3d_nsgs_velocity_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{
  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);

  /* Change into dense if neccessary*/

  int m = localproblem->M->size0;
  assert(localproblem->M->size0 == localproblem->M->size1);

  if (localproblem->M->storageType == 1)
  {

    localproblem->M->matrix0 = (double*)malloc(m * m * sizeof(double));
    SBMtoDense(localproblem->M->matrix1, localproblem->M->matrix0);
    freeSBM(localproblem->M->matrix1);
    free(localproblem->M->matrix1);
    localproblem->M->storageType = 0;
    localproblem->M->matrix1 = NULL;
    localproblem->M->matrix2 = NULL;
    localproblem->M->internalData = NULL;
  }

  fc3d_nsgs_velocity(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);

}
int gfc3d_nsgs_velocity_wr_setDefaultSolverOptions(SolverOptions* options)
{




  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the NSGSV_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_NSGSV_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  fc3d_nsgs_velocity_setDefaultSolverOptions(options->internalSolvers);
  return 0;

}

void  gfc3d_proximal_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{

  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);

  fc3d_proximal(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);


}
int gfc3d_proximal_wr_setDefaultSolverOptions(SolverOptions* options)
{


  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the PROX_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_PROX_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  fc3d_proximal_setDefaultSolverOptions(options->internalSolvers);
  return 0;
}
void  gfc3d_DeSaxceFixedPoint_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{

  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);

  fc3d_DeSaxceFixedPoint(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);


}
int gfc3d_DeSaxceFixedPoint_setDefaultSolverOptions(SolverOptions* options)
{


  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the DSFP_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_DSFP_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  fc3d_DeSaxceFixedPoint_setDefaultSolverOptions(options->internalSolvers);
  return 0;
}

void  gfc3d_TrescaFixedPoint_wr(GlobalFrictionContactProblem* problem, double *reaction , double *velocity, double* globalVelocity, int *info, SolverOptions* options)
{

  // Reformulation
  FrictionContactProblem* localproblem = (FrictionContactProblem *) malloc(sizeof(FrictionContactProblem));

  reformulationIntoLocalProblem(problem, localproblem);

  fc3d_TrescaFixedPoint(localproblem, reaction , velocity , info , options->internalSolvers);

  computeGlobalVelocity(problem, reaction, globalVelocity);
  freeLocalProblem(localproblem);


}
int gfc3d_TrescaFixedPoint_setDefaultSolverOptions(SolverOptions* options)
{


  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the DSFP_WR Solver\n");
  }

  options->solverId = SICONOS_GLOBAL_FRICTION_3D_TFP_WR;

  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 0;
  options->dSize = 0;
  options->iparam = NULL;
  options->dparam = NULL;
  options->dWork = NULL;
  null_SolverOptions(options);
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  fc3d_TrescaFixedPoint_setDefaultSolverOptions(options->internalSolvers);
  return 0;
}
