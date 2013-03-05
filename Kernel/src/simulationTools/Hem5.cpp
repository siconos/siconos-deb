/* Siconos-Kernel, Copyright INRIA 2005-2012.
 * Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 * Siconos is a free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * Siconos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Siconos; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Contact: Vincent ACARY, siconos-team@lists.gforge.inria.fr
*/

#include "Hem5.hpp"
#include "EventDriven.hpp"
#include "LagrangianLinearTIDS.hpp"
#include "BlockVector.hpp"
#include "NonSmoothDynamicalSystem.hpp"
#include "Model.hpp"
#include "Topology.hpp"
#include "LagrangianRheonomousR.hpp"
#include "LagrangianScleronomousR.hpp"
#include "NewtonImpactNSL.hpp"
#include "MultipleImpactNSL.hpp"
#include "NewtonImpactFrictionNSL.hpp"

using namespace std;
using namespace RELATION;

#define DEBUG_STDOUT
#define DEBUG_MESSAGES
#include "debug.h"


// ===== Out of class objects and functions =====

// global object and wrapping functions -> required for function plug-in and call in fortran routine.
SP::Hem5 hem5_global_object;

// This first function must have the same signature as argument FPROB  in HEM5
extern "C" void Hem5_fprob_wrapper(integer* IFCN,
                                   integer* NQ,
                                   integer* NV,
                                   integer* NU,
                                   integer* NL,
                                   integer* LDG, integer* LDF, integer* LDA,
                                   integer* NBLK, integer* NMRC,
                                   integer* NPGP, integer* NPFL,
                                   integer* INDGR, integer* INDGC, integer * INDFLR, integer * INDFLC,
                                   doublereal* time,
                                   doublereal* q, doublereal* v, doublereal* u,  doublereal* xl,
                                   doublereal* G, doublereal* GQ, doublereal * F,
                                   doublereal* GQQ, doublereal* GT, doublereal * FL,
                                   doublereal* QDOT, doublereal* UDOT, doublereal * AM)
{
  return hem5_global_object->fprob(IFCN,
                                   NQ,
                                   NV,
                                   NU,
                                   NL,
                                   LDG,  LDF,  LDA,
                                   NBLK,  NMRC,
                                   NPGP,  NPFL,
                                   INDGR,  INDGC, INDFLR, INDFLC,
                                   time,
                                   q,  v,  u,   xl,
                                   G,  GQ, F,
                                   GQQ,  GT, FL,
                                   QDOT,  UDOT, AM);
}

// This first function must have the same signature as argument SOLOUT in HEM5
extern "C" void Hem5_solout_wrapper(integer* MODE,
                                    integer* NSTEP,
                                    integer* NQ,
                                    integer* NV,
                                    integer* NU,
                                    integer* NL,
                                    integer* LDG, integer* LDF, integer* LDA,
                                    integer* LRDO, integer* LIDO,
                                    fprobpointer FPROB,
                                    doublereal* q, doublereal* v, doublereal* u,
                                    doublereal *DOWK, integer* IDOWK)
{
  return hem5_global_object->solout(MODE,
                                    NSTEP,
                                    NQ,
                                    NV,
                                    NU,
                                    NL,
                                    LDG,  LDF, LDA,
                                    LRDO, LIDO,
                                    FPROB,
                                    q, v,  u,
                                    DOWK, IDOWK);
}


Hem5::Hem5(SP::DynamicalSystem ds):
  OneStepIntegrator(OSI::LSODAR)
{
  // add ds in the set
  OSIDynamicalSystems->insert(ds);
  _intData.resize(9);
  for (int i = 0; i < 9; i++) _intData[i] = 0;
  _sizeMem = 2;
}

Hem5::Hem5(DynamicalSystemsSet& newDS):
OneStepIntegrator(OSI::LSODAR, newDS)
{
  _intData.resize(9);
  for (int i = 0; i < 9; i++) _intData[i] = 0;
  _sizeMem = 2;
}

void Hem5::setTol(integer newItol, SA::doublereal newRtol, SA::doublereal newAtol)
{
  _intData[4] = newItol; // ITOL  indicates whether RTOL and ATOL are scalar (ITOL=0), or array of
                         //           dimension NQ + NV + NU (ITOL=1)
  rtol = newRtol;
  atol = newAtol;
}
void Hem5::setTol(integer newItol, doublereal newRtol, doublereal newAtol)
{
  _intData[4] = newItol; // ITOL  indicates whether RTOL and ATOL are scalar (ITOL=0), or array of
                         //           dimension NQ + NV + NU (ITOL=1)
  rtol[0] = newRtol; // rtol
  atol[0] = newRtol;  // atol
}

void Hem5::setMaxStepSize(doublereal _maxStep)
{
  rwork[5] = _maxStep;
}

void Hem5::setMaxNstep(integer _maxNumberSteps)
{
  iwork[11] = _maxNumberSteps;
}

void Hem5::updateIntData()
{
  //   Integer parameters for HEM5 are saved in vector intData.

  // 1 - _intData[0] NQ size of the position vector q
  _intData[0] = _qWork->size();

  // 2 - _intData[1] NV size of the position vector v
  _intData[1] = _vWork->size();

  // 3 - _intData[2] NU size of the external dynamic vector u
  _intData[2] = 0;

  // 4 -  _intData[3] NL size of the Lagrange multiplier vector lambda
  _intData[3] = numberOfConstraints();

  // 3 - Itol, itask, iopt
  _intData[4] = 0; // ITOL indicates whether RTOL and ATOL are scalar (ITOL=0), or array of
                   //  dimension NQ + NV + NU (ITOL=1)
  _intData[5] = 0; // IOUT selects the dense output formula

  // this computation has to be redone every time _indData[3] is recompyuted.


   // IWK(14)  MODE (=0: FULL LINEAR ALGEBRA WITH DEC, =1: IDEM WITH FL,
  //                      =2: FULL LINEAR ALGEBRA WITH DGETRF, =3: FL
  //                      =4: SPARSE, =5: IDEM WITH FL)
  int MODE = 0;
  _intData[8] = MODE;
  int NZA = 0;
  int LL =0;
  int IS = 0;  // size of IMEM common work space arrays for MA28PACK
  int IXS = 0; // size of XMEM common work space arrays for MA28PACK

  int LDG = 0; // LDG : leading dimension of the Jacabian of constraints (G) (or non zeroa elements in sparse case)
  int LDF = 0; // LDF : leading dimension of the L or FL (L)

  int NMRC = (int)_intData[1]; // NMRC : size of a block of M
  int NBLK = 1;                // NBLK : number of block of M

  if (MODE <=3 )
  {
    LL = 8 * ( (int)_intData[1] * (int)_intData[3]  )
       + 4 * ( (int)_intData[1] + (int)_intData[3]  )*( (int)_intData[1] + (int)_intData[3]);
    LDG = _intData[3];
    LDF = _intData[3];
    NZA = LDG + std::max(LDG,LDF) + NMRC*NMRC*NBLK;
    IS  = 0; // Sparse solver MA28 is not called
    IXS = 0; // Sparse solver MA28 is not called
  }
  if (MODE >3)
  {
    RuntimeException::selfThrow("Hem5::updateIntData(), MODE >3 Sparse case not implemented ...");
  }

  // 5 - LWK length of real array rwork
  _intData[6] = 19 + 27*(int)_intData[0] + 28 * (int)_intData[1] + 27 *  (int)_intData[2]
    + 5*((int)_intData[1] + (int)_intData[3]) + 4*NZA + 2*IXS + LL;

  // 6 - LIWK length of integer array iwork
  _intData[7] = 95 + 2*((int)_intData[1]+(int)_intData[3]) + 2*IS + 12*LDG + 4 * LDF + 4 *NZA;
  _intData[7] *= 2;
}

void Hem5::updateData()
{
  // Used to update some data (iwork ...) when _intData is modified.
  // Warning: it only checks sizes and possibly reallocate memory, but no values are set.

  unsigned int sizeTol = _intData[0]; // size of rtol, atol ...
                                      // If itol (_intData[4]) = 0 => scalar else, vector of size neq (_intData[0]).
  //  if(_intData[0]==1) sizeTol = 1;
  //  else sizeTol = _intData[0];

  rtol.reset(new doublereal[sizeTol]) ;    // rtol, relative tolerance

  atol.reset(new doublereal[sizeTol]) ;    // atol, absolute tolerance
  for (unsigned int i = 0; i < sizeTol; i++)
  {
    atol[i] = 0.0;
  }

  iwork.reset(new integer[_intData[7]]);
  for (int i = 0; i < _intData[7]; i++) iwork[i] = 0;

  rwork.reset(new doublereal[_intData[6]]);
  for (int i = 0; i < _intData[6]; i++) rwork[i] = 0.0;

}

void Hem5::fillqWork(integer* NQ, doublereal* q)
{
  unsigned int sizeQ = (unsigned int)(*NQ);
  for (unsigned int i = 0; i < sizeQ ; ++i)
    (*_qWork)(i) = q[i];
}

void Hem5::fillvWork(integer* NV, doublereal* v)
{
  unsigned int sizeV = (unsigned int)(*NV);
  for (unsigned int i = 0; i < sizeV ; ++i)
    (*_vWork)(i) = v[i];
}

void Hem5::computeRhs(double t)
{
  DSIterator it;
  for (it = OSIDynamicalSystems->begin(); it != OSIDynamicalSystems->end(); ++it)
    (*it)->computeRhs(t);
}

void Hem5::computeJacobianRhs(double t)
{
  DSIterator it;
  for (it = OSIDynamicalSystems->begin(); it != OSIDynamicalSystems->end(); ++it)
    (*it)->computeJacobianRhsx(t);
}

void Hem5::fprob(integer* IFCN,
                 integer* NQ,
                 integer* NV,
                 integer* NU,
                 integer* NL,
                 integer* LDG, integer* LDF, integer* LDA,
                 integer* NBLK, integer* NMRC,
                 integer* NPGP, integer* NPFL,
                 integer* INDGR, integer* INDGC, integer * INDFLR, integer * INDFLC,
                 doublereal* time,
                 doublereal* q, doublereal* v, doublereal* u,  doublereal* xl,
                 doublereal* G, doublereal* GQ, doublereal * F,
                 doublereal* GQQ, doublereal* GT, doublereal * FL,
                 doublereal* QDOT, doublereal* UDOT, doublereal * AM)
{
  DEBUG_PRINTF("Hem5::fprob(integer* IFCN,...) with IFCN = %i \n", (int)*IFCN);
  DEBUG_PRINTF("NQ = %i\t NV = %i \t NU = %i, NL = %i \n", (int)*NQ, (int)*NV, (int)*NU, (int)*NL);
  DEBUG_PRINTF("LDG = %i\t LDF = %i \t LDA = %i \n", (int)*LDG, (int)*LDF, (int)*LDA);

  // fill in xWork vector (ie all the x of the ds of this osi) with x
  fillqWork(NQ, q);
  fillvWork(NV, v);

  double t = *time;
  simulationLink->model()->setCurrentTime(t);

  SP::DynamicalSystemsGraph dsGraph = simulationLink->model()->nonSmoothDynamicalSystem()->dynamicalSystems();



  int ifcn = (int)(*IFCN);

  if ((ifcn ==1) or (ifcn>=7)) // compute Mass AM
  {
    unsigned int pos=0;
    for (DynamicalSystemsGraph::VIterator vi = dsGraph->begin(); vi != dsGraph->end(); ++vi)
    {
      SP::DynamicalSystem ds = dsGraph->bundle(*vi);
      if (Type::value(*ds) == Type::LagrangianDS ||
          Type::value(*ds) == Type::LagrangianLinearTIDS)
      {
        LagrangianDS& lds = *std11::static_pointer_cast<LagrangianDS>(ds);
        lds.computeMass();
        for (unsigned int ii =pos ; ii < ((unsigned int)(*NV)+pos); ii ++)
        {
          for (unsigned int jj =pos ; jj < ((unsigned int)(*NV)+pos); jj ++)
          {
            AM[ii + jj*(int)(*NV)] = lds.mass()->getValue(ii,jj) ;
          }
        }
        pos += lds.getDim();
      }
      else
      {
        RuntimeException::selfThrow("Hem5::fprob(), Only integration of Lagrangian DS is allowed");
      }
      DEBUG_EXPR(
        for (int kk =0 ; kk < (int)(*NV)* (int)(*NV); kk ++)
        {
          std::cout << AM[kk] << std::endl;
        }
        );
    }
  }
  if ((ifcn ==1) or (ifcn == 5) or (ifcn == 7) or (ifcn==8)) // compute F
  {
    for (DynamicalSystemsGraph::VIterator vi = dsGraph->begin(); vi != dsGraph->end(); ++vi)
    {
      SP::DynamicalSystem ds = dsGraph->bundle(*vi);
      if (Type::value(*ds) == Type::LagrangianDS ||
          Type::value(*ds) == Type::LagrangianLinearTIDS)
      {
        LagrangianDS& lds = *std11::static_pointer_cast<LagrangianDS>(ds);
        fillqWork(NQ,q);
        fillvWork(NV,v);
        lds.computeForces((double)*time);
      }
      else if (Type::value(*ds) == Type::NewtonEulerDS)
      {
        RuntimeException::selfThrow("Hem5::fprob(), Integration of Newton Euler DS not yet implemented.");
      }
      else
      {
        RuntimeException::selfThrow("Hem5::fprob(), Only integration of Lagrangian DS is allowed");
      }
    }
    for (unsigned int ii =0 ; ii < (unsigned int)(*NV); ii ++)
    {
      F[ii] = _forcesWork->getValue(ii) ;
    }
  }
  if ((ifcn == 4)) // compute G (constraints)
  {
    InteractionsGraph::VIterator ui, uiend;
    SP::InteractionsGraph indexSet2
      = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(2);
    assert(indexSet2);
    for (std11::tie(ui, uiend) = indexSet2->vertices(); ui != uiend; ++ui)
    {
      SP::Interaction inter = indexSet2->bundle(*ui);
      inter->computeOutput(t,0);
      assert(0);
    }

  }

  if ((ifcn == 6) or (ifcn >= 10) ) // compute GP ( Jacobian of the constraints)
  {
    InteractionsGraph::VIterator ui, uiend;
    SP::InteractionsGraph indexSet2 = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(2);
    for (std11::tie(ui, uiend) = indexSet2->vertices(); ui != uiend; ++ui)
    {
      SP::Interaction inter = indexSet2->bundle(*ui);
      inter->computeJach(t);
      assert(0);
    }
  }

  if ((ifcn == 5) or (ifcn == 7) ) // compute GPP ( Hessian of the constraints)
  {
    //RuntimeException::selfThrow("Hem5::fprob(), G_qq is not available");
    std::cout << "Hem5::fprob(), G_qq is not available " << std::endl;
  }

  if ((ifcn == 3) or (ifcn == 6) or (ifcn >= 10) ) // compute GT (partial time derivative of the constraints)
  {
    InteractionsGraph::VIterator ui, uiend;
    SP::InteractionsGraph indexSet2 = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(2);
    for (std11::tie(ui, uiend) = indexSet2->vertices(); ui != uiend; ++ui)
    {
      SP::Interaction inter = indexSet2->bundle(*ui);
      inter->computeJach(t);
      assert(0);
    }
  }

  if ((ifcn == 0) ) // compute UDOT
  {
    for (int ii ; ii < (int)*NU ; ii++)
    {
      assert(0);
    }
  }

  if ((ifcn == 1) or (ifcn == 2) or (ifcn == 10) ) // compute QDOT
  {
    unsigned int pos=0;
    for (DynamicalSystemsGraph::VIterator vi = dsGraph->begin(); vi != dsGraph->end(); ++vi)
    {
      SP::DynamicalSystem ds = dsGraph->bundle(*vi);
      if (Type::value(*ds) == Type::LagrangianDS ||
          Type::value(*ds) == Type::LagrangianLinearTIDS)
      {
        LagrangianDS& lds = *std11::static_pointer_cast<LagrangianDS>(ds);
        unsigned int dim = lds.getDim();
        for (unsigned int i =0 ; i < dim ; i++)
        {
          QDOT[i+pos] = v[i+pos];
        }
        pos +=dim ;
      }
      else if (Type::value(*ds) == Type::NewtonEulerDS)
      {
        RuntimeException::selfThrow("Hem5::fprob(), Integration of Newton Euler DS not yet implemented.");
      }
      else
      {
        RuntimeException::selfThrow("Hem5::fprob(), Only integration of Mechanical DS is allowed");
      }

    }
    DEBUG_EXPR(
      for (int kk =0 ; kk < (int)(*NV); kk ++)
      {
        std::cout << QDOT[kk] << std::endl;
      }
      );
  }

  DEBUG_PRINTF("END : Hem5::fprob(integer* IFCN,...) with IFCN = %i \n \n", (int)*IFCN);
}
// void Hem5::g(integer* nEq, doublereal*  time, doublereal* x, integer* ng, doublereal* gOut)
// {
//   std11::static_pointer_cast<EventDriven>(simulationLink)->computeg(shared_from_this(), nEq, time, x, ng, gOut);
// }

// void Hem5::jacobianfx(integer* sizeOfX, doublereal* time, doublereal* x, integer* ml, integer* mu,  doublereal* jacob, integer* nrowpd)
// {
//   std11::static_pointer_cast<EventDriven>(simulationLink)->computeJacobianfx(shared_from_this(), sizeOfX, time, x, jacob);
// }

void Hem5::initialize()
{

  DEBUG_PRINT("Hem5::initialize()\n");

  OneStepIntegrator::initialize();
  _qWork.reset(new BlockVector());
  _vWork.reset(new BlockVector());
  _aWork.reset(new BlockVector());
  _uWork.reset(new BlockVector());
  _lambdaWork.reset(new BlockVector());
  _forcesWork.reset(new BlockVector());

  // initialize xxxWork with xxx values of the dynamical systems present in the set.
  SP::DynamicalSystemsGraph dsGraph = simulationLink->model()->nonSmoothDynamicalSystem()->dynamicalSystems();

  for (DynamicalSystemsGraph::VIterator vi = dsGraph->begin(); vi != dsGraph->end(); ++vi)
  {
    SP::DynamicalSystem ds = dsGraph->bundle(*vi);
    std::cout << Type::value(*ds)   <<std::endl;
    if (Type::value(*ds) == Type::LagrangianDS ||
        Type::value(*ds) == Type::LagrangianLinearTIDS)
    {
      LagrangianDS& lds = *std11::static_pointer_cast<LagrangianDS>(ds);
      _qWork->insertPtr(lds.q());
      _vWork->insertPtr(lds.velocity());
      _aWork->insertPtr(lds.acceleration());
      if (!lds.forces())
      {
        lds.initForces();
      }
      _forcesWork->insertPtr(lds.forces());
    }
    else
    {
    RuntimeException::selfThrow("Hem5::initialize(), Only integration of Lagrangian DS is allowed");
    }
  }

  // InteractionsGraph::VIterator ui, uiend;
  // SP::InteractionsGraph indexSet0
  //   = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(0);
  // assert(indexSet0);
  // for (std11::tie(ui, uiend) = indexSet0->vertices(); ui != uiend; ++ui)
  // {
  //   SP::Interaction inter = indexSet0->bundle(*ui);
  //   _lambdaWork->insertPtr(inter->lambda(0));
  // }


  _timeStep = 1.e-3; // initial step size guess (typical value 1e-3)

}
void Hem5::solout(integer* MODE,
                  integer* NSTEP,
                  integer* NQ,
                  integer* NV,
                  integer* NU,
                  integer* NL,
                  integer* LDG, integer* LDF, integer* LDA,
                  integer* LRDO, integer* LIDO,
                  fprobpointer FPROB,
                  doublereal* q, doublereal* v, doublereal* u,
                  doublereal *DOWK, integer* IDOWK)

{
}

unsigned int Hem5::numberOfConstraints()
{
  DEBUG_PRINT("Hem5::updateConstraints() \n");
  InteractionsGraph::VIterator ui, uiend;
  SP::InteractionsGraph indexSet2
    = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(2);
  assert(indexSet2);
  SP::SiconosVector y;
  unsigned int n = 0;
  for (std11::tie(ui, uiend) = indexSet2->vertices(); ui != uiend; ++ui)
  {
    SP::Interaction inter = indexSet2->bundle(*ui);
    n++;
  }
  return n;
}

void Hem5::integrate(double& tinit, double& tend, double& tout, int& idid)
{

  DEBUG_PRINT("Hem5::integrate(double& tinit, double& tend, double& tout, int& idid) with \n");
  DEBUG_PRINTF("tinit = %f, tend= %f, tout = %f, idid = %i\n", tinit, tend,  tout, idid );

  doublereal tend_DR = tend  ;       // next point where output is desired (different from t!)
  doublereal tinit_DR = tinit;       // current (starting) time

  // === Pointers to function ===
  //  --> definition and initialisation thanks to wrapper:
  hem5_global_object = std11::static_pointer_cast<Hem5>(shared_from_this()); // Warning: global object must be initialized to current one before pointers to function initialisation.

  // function to compute the system to simulation
  fprobpointer pointerToFPROB = Hem5_fprob_wrapper;

  // function to compute the system to simulation
  soloutpointer pointerToSOLOUT = Hem5_solout_wrapper;

  // === HEM5 CALL ===


  updateIntData();
  if (!_qtmp)
  {
    _qtmp.reset(new SiconosVector(_qWork->size()));
  }
  else
    _qtmp->resize((int)_intData[0],true);

  DEBUG_PRINTF("Hem5::integrate() _intData[0] (NQ) = %i \n",_intData[0]);

  if (!_vtmp)
  {
    _vtmp.reset(new SiconosVector(_vWork->size()));
  }
  else
     _vtmp->resize((int)_intData[1],true);


  _utmp.reset(new SiconosVector(1));
  DEBUG_PRINTF("Hem5::integrate() _intData[2] (NU) = %i \n",_intData[2]);

  if (!_atmp)
  {
    _atmp.reset(new SiconosVector(_vWork->size()));
  }
  else
    _atmp->resize((int)_intData[1],true);

  if (!_lambdatmp)
  {
    _lambdatmp.reset(new SiconosVector(_intData[3],0.0));
  }
  else
    _lambdatmp->resize((int)_intData[3],true);
  DEBUG_PRINTF("Hem5::integrate() _intData[3] (NL) = %i \n",_intData[3]);

  DEBUG_PRINTF("Hem5::integrate() _intData[6] (LWK) = %i \n",_intData[6]);
  DEBUG_PRINTF("Hem5::integrate() _intData[7] (LIWK) = %i \n",_intData[7]);

  Hem5::updateData();

  rwork[0] = MACHINE_PREC ; // WK(1)   UROUND, THE ROUNDING UNIT, DEFAULT 1.D-16.

  rwork[1] = 0.0 ;          // WK(2)   THE SAFETY FACTOR IN STEP SIZE PREDICTION,
                            //         DEFAULT 0.85D0.
  rwork[2] = 0.0 ; // WK(3), WK(4)   PARAMETERS FOR STEP SIZE SELECTION
  rwork[3] = 0.0 ; //                THE NEW STEP SIZE IS CHOSEN SUBJECT TO THE RESTRICTION
                   //                WK(3) <= HNEW/HOLD <= WK(4).
                   //                DEFAULT VALUES: WK(3)=0.2D0, WK(4)=10.D0
  rwork[5] = 0.0 ; // WK(6)   MAXIMAL STEP SIZE, DEFAULT TEND-T.

  rwork[6] = 0.0 ; // WK(7) = BETA, DEFAULT 0.D0
  rwork[7] = 0.0 ; // WK(8) = ALPHA, DEFAULT 1/5

  iwork[10] = 0 ; // IWK(11)  THIS IS THE MAXIMAL NUMBER OF ALLOWED STEPS.
                  //          THE DEFAULT VALUE (FOR IWK(11)=0) IS 100000.
  iwork[11] = 0 ; // IWK(12)  SWITCH FOR A PROJECTION TO ENSURE CONSISTENT INITIAL VALUE
                  //          FOR IWK(12)=1 AN INITIAL PROJECTION IS PERFORMED.
                  //          NO PROJECTION IS DONE IF IWK(12)=0.
                  //          THE DEFAULT VALUE FOR IWK(12) IS 0.

  iwork[12] = 0 ; // IWK(13)  FOR IWK(13).GT.0 IT IS THE NUMBER OF STEPS BETWEEN
                  //          TWO PROJECTIONS ON THE MANIFOLD  DEFINED BY 0 = g(q,t).
                  //          FOR IWK(13).LE.0 NO PROECTION IS PERFORMED.
                  //          THE DEFAULT VALUE FOR IWK(13) IS 0.


  iwork[13] = _intData[8] ; // IWK(14)  MODE (=0: FULL LINEAR ALGEBRA WITH DEC, =1: IDEM WITH FL,
                     //                =2: FULL LINEAR ALGEBRA WITH DGETRF, =3: FL
                     //                =4: SPARSE, =5: IDEM WITH FL)

  iwork[14] = 1    ; // IWK(15)  IACC (=1: COMPUTE THE ACCELERATION)

  iwork[15] = 1    ; // IWK(16)  IGIIN (=1: COMPUTE NUMERICALLY GII)

// C    IWK(21->29)  IPAR
// C    IPAR(1) = IWK(21) = NMRC (SIZE OF A BLOCK OF AM)
// C    IPAR(2) = IWK(22) = NBLK (NUMBER OF BLOCK OF AM)
// C    IPAR(3) = IWK(23) = NPGP (0 IF GP AS THE SAME PATTERN AS PREVIOUS CALL)
// C    IPAR(4) = IWK(24) = NPFL (0 IF FL AS THE SAME PATTERN AS PREVIOUS CALL)
// C    IPAR(5) = IWK(25) = IS (SIZE OF INTEGER WORK SPACE FOR MA28 (MIN 13*NM))
// C    IPAR(6) = IWK(26) = IXS (SIZE OF REAL WORK SPACE FOR MA28 (MIN NM+4*NZA))
// C    IPAR(7) = IWK(27) = PREVL
// C    IPAR(8) = IWK(28) = IO
// C    IPAR(9) = FLAG TO INDICATE IF UMDFAC HAS BEEN CALLED AT LEAST ONCE

  DEBUG_EXPR( iwork[26] =2;printf("\n"));

  // Set atol and rtol values ...
  rtol[0] = HEM5_RTOL_DEFAULT ; // rtol
  atol[0] = HEM5_ATOL_DEFAULT ;  // atol

  *_qtmp = *_qWork; // Copy into a continuous memory chuck
  *_vtmp = *_vWork; // Copy into a continuous memory chuck
  //*_utmp = *_uWork; // Copy into a continuous memory chuck
  *_atmp = *_aWork; // Copy into a continuous memory chuck

  DEBUG_EXPR(_qtmp->display(););
  DEBUG_EXPR(_vtmp->display(););
  DEBUG_EXPR(_atmp->display(););


  //*_lambdatmp = *_lambdaWork; // Copy into a continuous memory chuck


  assert(_qtmp);
  assert(_vtmp);
  assert(_utmp);
  assert(_atmp);
  assert(_lambdatmp);
  assert(_intData[7]);


  // Management of vectors of Size 0
  doublereal * pointerToU;
  if (_intData[2] ==0)
    pointerToU = NULL;
  else
    pointerToU = &(*_utmp)(0);

  doublereal * pointerToXL;
  if (_intData[3] ==0)
    pointerToXL = NULL;
  else
    pointerToXL = &(*_lambdatmp)(0);

  // call HEM5 to integrate dynamical equation
  F77NAME(hem5)(&(_intData[0]),
                &(_intData[1]),
                &(_intData[2]),
                &(_intData[3]),
                pointerToFPROB,
                &tinit_DR,
                &(*_qtmp)(0),
                &(*_vtmp)(0),
                pointerToU,
                &(*_atmp)(0),
                pointerToXL,
                &tend_DR,
                &_timeStep,
                rtol.get(),
                atol.get(),
                &(_intData[4]),
                pointerToSOLOUT,
                &(_intData[5]),
                rwork.get(),
                &(_intData[6]),
                iwork.get(),
                &(_intData[7]),
                &_idid);

  // === Post ===
  if (_idid < 0) // if istate < 0 => LSODAR failed
  {
    cout << "Hem5::integrate(...) failed - idid = " << _idid << endl;
    cout << " -1 means input is not consistent" << endl;
    cout << " -2 means larger NMAX needed." << endl;
    cout << " -3 means step size becomes too small." << endl;
    cout << " -4 means matrix is singular" << endl;
    cout << " -5 means initial projection: no convergence" << endl;
    RuntimeException::selfThrow("Hem5::integrate(), integration failed");
  }

  DEBUG_EXPR_WE(  cout << "HEM5 Statitics : " << endl;
                  cout << "NSTEP = " << iwork[30] << endl;
                  cout << "NACCPT = " << iwork[31] << endl;
                  cout << "NREJCT = " << iwork[32] << endl;
                  cout << "NFCN = " << iwork[33] << endl;
                  cout << "NDEC = " << iwork[34] << endl;
                  cout << "NSOL = " << iwork[35] << endl; );
  *_qWork = *_qtmp;
  *_vWork = *_vtmp;
  *_aWork = *_atmp;

  DEBUG_PRINTF("tend_DR = %f\n", (double) tend_DR);
  DEBUG_EXPR(_qWork->display());
  DEBUG_EXPR(_vWork->display());
  DEBUG_EXPR(_aWork->display());
  DEBUG_PRINT("\n");
  DEBUG_PRINT("\n");



  InteractionsGraph::VIterator ui, uiend;
  SP::InteractionsGraph indexSet2
    = simulationLink->model()->nonSmoothDynamicalSystem()->topology()->indexSet(2);
  assert(indexSet2);
  SP::SiconosVector y;
  unsigned int pos=0;
  for (std11::tie(ui, uiend) = indexSet2->vertices(); ui != uiend; ++ui)
  {
    SP::Interaction inter = indexSet2->bundle(*ui);
    inter->lambda(2)->setValue(0,(*_lambdatmp)(pos));
    pos++;
  }

  tout  = tinit_DR; // real ouput time
  tend  = tend_DR;  // necessary for next start of HEM5

}


void Hem5::updateState(const unsigned int level)
{
  // Compute all required (ie time-dependent) data for the DS of the OSI.
  DSIterator it;

  if (level == 1) // ie impact case: compute velocity
  {
    for (it = OSIDynamicalSystems->begin(); it != OSIDynamicalSystems->end(); ++it)
    {
      SP::LagrangianDS lds = std11::static_pointer_cast<LagrangianDS>(*it);
      lds->computePostImpactVelocity();
    }
  }
  else if (level == 2)
  {
    double time = simulationLink->model()->currentTime();
    for (it = OSIDynamicalSystems->begin(); it != OSIDynamicalSystems->end(); ++it)
      (*it)->update(time);
  }
  else RuntimeException::selfThrow("Hem5::updateState(index), index is out of range. Index = " + level);
}

struct Hem5::_NSLEffectOnFreeOutput : public SiconosVisitor
{
  using SiconosVisitor::visit;

  OneStepNSProblem * _osnsp;
  SP::Interaction _inter;

  _NSLEffectOnFreeOutput(OneStepNSProblem *p, SP::Interaction inter) :
    _osnsp(p), _inter(inter) {};

  void visit(const NewtonImpactNSL& nslaw)
  {
    double e;
    e = nslaw.e();
    Index subCoord(4);
    subCoord[0] = 0;
    subCoord[1] = _inter->getNonSmoothLawSize();
    subCoord[2] = 0;
    subCoord[3] = subCoord[1];
    subscal(e, *_inter->yOld(_osnsp->levelMin()), *(_inter->yp()), subCoord, false); // q = q + e * q
  }

  // visit function added by Son (9/11/2010)
  void visit(const MultipleImpactNSL& nslaw)
  {
    ;
  }
  // note : no NewtonImpactFrictionNSL
};


void Hem5::computeFreeOutput(SP::Interaction inter, OneStepNSProblem * osnsp)
{
  SP::OneStepNSProblems  allOSNS  = simulationLink->oneStepNSProblems();

  // Get relation and non smooth law types
  RELATION::TYPES relationType = inter->getRelationType();
  RELATION::SUBTYPES relationSubType = inter->getRelationSubType();

  SP::DynamicalSystem ds = *(inter->dynamicalSystemsBegin());

  unsigned int sizeY = inter->getNonSmoothLawSize();

  unsigned int relativePosition = 0;
  SP::Interaction mainInteraction = inter;
  Index coord(8);
  coord[0] = relativePosition;
  coord[1] = relativePosition + sizeY;
  coord[2] = 0;
  coord[4] = 0;
  coord[6] = 0;
  coord[7] = sizeY;
  SP::SiconosMatrix  C;
  //   SP::SiconosMatrix  D;
  //   SP::SiconosMatrix  F;
  SP::SiconosVector Yp;
  SP::BlockVector Xfree;


  // All of these values should be stored in the node corrseponding to the Interactionwhen a Moreau scheme is used.
  Yp = inter->yp();

  /* V.A. 10/10/2010
       * Following the type of OSNS  we need to retrieve the velocity or the acceleration
       * This tricks is not very nice but for the moment the OSNS do not known if
       * it is in accelaration of not
       */

  //SP::OneStepNSProblems  allOSNS  = _simulation->oneStepNSProblems();
  if (((*allOSNS)[SICONOS_OSNSP_ED_SMOOTH_ACC]).get() == osnsp)
  {
    Xfree  = inter->dataFree();
    //       std::cout << "Computeqblock Xfree (Gamma)========" << std::endl;
    //       Xfree->display();
  }
  else  if (((*allOSNS)[SICONOS_OSNSP_ED_IMPACT]).get() == osnsp)
  {
    Xfree = inter->dataQ1();
    //       std::cout << "Computeqblock Xfree (Velocity)========" << std::endl;
    //       Xfree->display();

  }
  else
    RuntimeException::selfThrow(" computeqBlock for Event Event-driven is wrong ");

  if (relationType == Lagrangian)
  {
    C = mainInteraction->relation()->C();
    if (C)
    {
      assert(Xfree);
      assert(Yp);

      coord[3] = C->size(1);
      coord[5] = C->size(1);

      subprod(*C, *Xfree, *Yp, coord, true);
    }

    SP::SiconosMatrix ID(new SimpleMatrix(sizeY, sizeY));
    ID->eye();

    Index xcoord(8);
    xcoord[0] = 0;
    xcoord[1] = sizeY;
    xcoord[2] = 0;
    xcoord[3] = sizeY;
    xcoord[4] = 0;
    xcoord[5] = sizeY;
    xcoord[6] = 0;
    xcoord[7] = sizeY;
    // For the relation of type LagrangianRheonomousR
    if (relationSubType == RheonomousR)
    {
      if (((*allOSNS)[SICONOS_OSNSP_ED_SMOOTH_ACC]).get() == osnsp)
      {
        RuntimeException::selfThrow("Hem5::computeFreeOutput not yet implemented for LCP at acceleration level with LagrangianRheonomousR");
      }
      else if (((*allOSNS)[SICONOS_OSNSP_TS_VELOCITY]).get() == osnsp)
      {
        std11::static_pointer_cast<LagrangianRheonomousR>(inter->relation())->computehDot(simulation()->getTkp1(), *inter);
        subprod(*ID, *(std11::static_pointer_cast<LagrangianRheonomousR>(inter->relation())->hDot()), *Yp, xcoord, false); // y += hDot
      }
      else
        RuntimeException::selfThrow("Hem5::computeFreeOutput not implemented for SICONOS_OSNSP ");
    }
    // For the relation of type LagrangianScleronomousR
    if (relationSubType == ScleronomousR)
    {
      if (((*allOSNS)[SICONOS_OSNSP_ED_SMOOTH_ACC]).get() == osnsp)
      {
        std11::static_pointer_cast<LagrangianScleronomousR>(inter->relation())->computeNonLinearH2dot(simulation()->getTkp1(), *inter);
        subprod(*ID, *(std11::static_pointer_cast<LagrangianScleronomousR>(inter->relation())->Nonlinearh2dot()), *Yp, xcoord, false); // y += NonLinearPart
      }
    }
  }
  else
    RuntimeException::selfThrow("Hem5::computeFreeOutput not yet implemented for Relation of type " + relationType);
  if (((*allOSNS)[SICONOS_OSNSP_ED_IMPACT]).get() == osnsp)
  {
    if (inter->getRelationType() == Lagrangian || inter->getRelationType() == NewtonEuler)
    {
      SP::SiconosVisitor nslEffectOnFreeOutput(new _NSLEffectOnFreeOutput(osnsp, inter));
      inter->nonSmoothLaw()->accept(*nslEffectOnFreeOutput);
    }
  }

}
void Hem5::display()
{
  OneStepIntegrator::display();
  cout << " --- > Hem5 specific values: " << endl;
  cout << "Number of equations: " << _intData[0] << endl;
  cout << "Number of constraints: " << _intData[1] << endl;
  cout << "itol, itask, istate, iopt, lrw, liw, jt: (for details on what are these variables see opkdmain.f)" << endl;
  cout << _intData[2] << ", " << _intData[3] << ", " << _intData[4] << ", " << _intData[5] << ", " << _intData[6]  << ", " << _intData[7]  << ", " << _intData[8] << endl;
  cout << "====================================" << endl;
}