#ifndef NaturalMapGenerated_h
#define NaturalMapGenerated_h

#include "SiconosConfig.h"

#if defined(__cplusplus) && !defined(BUILD_AS_CPP)
extern "C"
{
#endif

#include "fc3d_NaturalMapABGenerated.h"
#include "fc3d_NaturalMapFABGenerated.h"
#include "fc3d_NaturalMapFGenerated.h"

void fc3d_NaturalMapFunctionGenerated(
  double *reaction,
  double *velocity,
  double mu,
  double *rho,
  double *f,
  double *A,
  double *B);

#if defined(__cplusplus) && !defined(BUILD_AS_CPP)
}
#endif
#endif
