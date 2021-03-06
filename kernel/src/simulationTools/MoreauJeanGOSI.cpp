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
#include "MoreauJeanGOSI.hpp"
#include "Simulation.hpp"
#include "Model.hpp"
#include "NonSmoothDynamicalSystem.hpp"
#include "NewtonEulerDS.hpp"
#include "LagrangianLinearTIDS.hpp"
#include "NewtonEulerR.hpp"
#include "LagrangianRheonomousR.hpp"
#include "NewtonImpactNSL.hpp"
#include "MultipleImpactNSL.hpp"
#include "NewtonImpactFrictionNSL.hpp"
#include "CxxStd.hpp"

#include "TypeName.hpp"

#include "OneStepNSProblem.hpp"
#include "BlockVector.hpp"


//#define DEBUG_STDOUT
//#define DEBUG_NOCOLOR
//#define DEBUG_MESSAGES
//#define DEBUG_WHERE_MESSAGES
#include <debug.h>


using namespace RELATION;

// --- constructor from a set of data ---
MoreauJeanGOSI::MoreauJeanGOSI(double theta, double gamma):
  OneStepIntegrator(OSI::MOREAUJEANOSI2), _useGammaForRelation(false), _explicitNewtonEulerDSOperators(false)
{
  _theta = theta;
  if (!isnan(gamma))
  {
    _gamma = gamma;
    _useGamma = true;
  }
  else
  {
    _gamma = 1.0;
    _useGamma = false;
  }
}


void MoreauJeanGOSI::initialize(Model& m)
{
  OneStepIntegrator::initialize(m);
  // Get initial time
  double t0 = m.t0();
  // Compute W(t0) for all ds


  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
  {
    if (!checkOSI(dsi)) continue;
    // Memory allocation for workX. workX[ds*] corresponds to xfree (or vfree in lagrangian case).
    // workX[*itDS].reset(new SiconosVector((*itDS)->dimension()));

    SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

    // W initialization
    initW(t0, ds, *dsi);
    Type::Siconos dsType = Type::value(*ds);
    if (dsType == Type::LagrangianLinearTIDS || dsType == Type::LagrangianDS)
    {
      ds->allocateWorkVector(DynamicalSystem::local_buffer, _dynamicalSystemsGraph->properties(*dsi).W->size(0));
    }
  }

  SP::OneStepNSProblems  allOSNS  = _simulation->oneStepNSProblems();
  ((*allOSNS)[SICONOS_OSNSP_TS_VELOCITY])->setIndexSetLevel(1);
  ((*allOSNS)[SICONOS_OSNSP_TS_VELOCITY])->setInputOutputLevel(1);
  //  ((*allOSNS)[SICONOS_OSNSP_TS_VELOCITY])->initialize(_simulation);
}

void MoreauJeanGOSI::initW(double t, SP::DynamicalSystem ds, DynamicalSystemsGraph::VDescriptor& dsv)
{
  DEBUG_PRINT("MoreauJeanGOSI::initW starts\n");
  // This function:
  // - allocate memory for a matrix W

  if (!ds || !dsv)
    RuntimeException::selfThrow("MoreauJeanGOSI::initW(t, ds, dsv) - ds == NULL or dsv == NULL");

  if (!(checkOSI(_dynamicalSystemsGraph->descriptor(ds))))
    RuntimeException::selfThrow("MoreauJeanOSI::initW(t,ds) - ds does not belong to the OSI.");

  if (_dynamicalSystemsGraph->properties(dsv).W)
    RuntimeException::selfThrow("MoreauJeanGOSI::initW(t,ds) - W(ds) is already in the map and has been initialized.");

  double h = _simulation->timeStep();
  Type::Siconos dsType = Type::value(*ds);


  if (dsType == Type::LagrangianDS)
  {
    SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);
    _dynamicalSystemsGraph->properties(dsv).W.reset(new SimpleMatrix(*d->mass()));
    // Compute the W matrix
    computeW(t, ds, *_dynamicalSystemsGraph->properties(dsv).W);
    // WBoundaryConditions initialization
    if (d->boundaryConditions())
      initWBoundaryConditions(d);
  }
  // 2 - Lagrangian linear systems
  else if (dsType == Type::LagrangianLinearTIDS)
  {
    SP::LagrangianLinearTIDS d = std11::static_pointer_cast<LagrangianLinearTIDS> (ds);
    _dynamicalSystemsGraph->properties(dsv).W.reset(new SimpleMatrix(*d->mass()));
    SiconosMatrix& W = *_dynamicalSystemsGraph->properties(dsv).W;

    SP::SiconosMatrix K = d->K();
    SP::SiconosMatrix C = d->C();

    if (C)
      scal(h * _theta, *C, W, false); // W += h*_theta *C
    if (K)
      scal(h * h * _theta * _theta, *K, W, false); // W = h*h*_theta*_theta*K

    // WBoundaryConditions initialization
    if (d->boundaryConditions())
      initWBoundaryConditions(d);
  }

  // === ===
  else if (dsType == Type::NewtonEulerDS)
  {
    SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
    _dynamicalSystemsGraph->properties(dsv).W.reset(new SimpleMatrix(*d->mass()));

    computeW(t,ds, *_dynamicalSystemsGraph->properties(dsv).W);
    // WBoundaryConditions initialization
    if (d->boundaryConditions())
      initWBoundaryConditions(d);

  }
  else RuntimeException::selfThrow("MoreauJeanGOSI::initW - not yet implemented for Dynamical system of type : " + Type::name(*ds));

  // Remark: W is not LU-factorized nor inversed here.
  // Function PLUForwardBackward will do that if required.
  DEBUG_PRINT("MoreauJeanGOSI::initW ends\n");


}


void MoreauJeanGOSI::initWBoundaryConditions(SP::DynamicalSystem ds)
{
  // This function:
  // - allocate memory for a matrix WBoundaryConditions
  // - insert this matrix into WBoundaryConditionsMap with ds as a key

  DEBUG_PRINT("MoreauJeanGOSI::initWBoundaryConditions(SP::DynamicalSystem ds) starts\n");
  if (!ds)
    RuntimeException::selfThrow("MoreauJeanGOSI::initWBoundaryConditions(t,ds) - ds == NULL");

  if (!(checkOSI(_dynamicalSystemsGraph->descriptor(ds))))
    RuntimeException::selfThrow("MoreauJeanGOSI::initWBoundaryConditions(t,ds) - ds does not belong to the OSI.");

  Type::Siconos dsType = Type::value(*ds);
  unsigned int dsN = ds->number();

  if (dsType == Type::LagrangianLinearTIDS || dsType == Type::LagrangianDS || dsType == Type::NewtonEulerDS)
  {
    if (_WBoundaryConditionsMap.find(dsN) != _WBoundaryConditionsMap.end())
      RuntimeException::selfThrow("MoreauJeanGOSI::initWBoundaryConditions(t,ds) - WBoundaryConditions(ds) is already in the map and has been initialized.");

    // Memory allocation for WBoundaryConditions
    unsigned int sizeWBoundaryConditions = ds->dimension(); // n for first order systems, ndof for lagrangian.

    SP::BoundaryCondition bc;
    if (dsType == Type::LagrangianDS || dsType == Type::LagrangianLinearTIDS)
    {
      SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);
      bc = d->boundaryConditions();
    }
    else if (dsType == Type::NewtonEulerDS)
    {
      SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
      bc = d->boundaryConditions();
    }
    unsigned int numberBoundaryConditions = bc->velocityIndices()->size();
    _WBoundaryConditionsMap[dsN].reset(new SimpleMatrix(sizeWBoundaryConditions, numberBoundaryConditions));
    computeWBoundaryConditions(ds);
  }
  else
    RuntimeException::selfThrow("MoreauJeanGOSI::initWBoundaryConditions - not yet implemented for Dynamical system of type :" +  Type::name(*ds));
    DEBUG_PRINT("MoreauJeanGOSI::initWBoundaryConditions(SP::DynamicalSystem ds) ends \n");
}


void MoreauJeanGOSI::computeWBoundaryConditions(SP::DynamicalSystem ds)
{
  // Compute WBoundaryConditions matrix of the Dynamical System ds, at
  // time t and for the current ds state.

  // When this function is called, WBoundaryConditionsMap[ds] is
  // supposed to exist and not to be null Memory allocation has been
  // done during initWBoundaryConditions.

  assert(ds &&
         "MoreauJeanGOSI::computeWBoundaryConditions(t,ds) - ds == NULL");

  Type::Siconos dsType = Type::value(*ds);
  unsigned int dsN = ds->number();
  if (dsType == Type::LagrangianLinearTIDS || dsType == Type::LagrangianDS ||  dsType == Type::NewtonEulerDS)
  {
    assert((_WBoundaryConditionsMap.find(dsN) != _WBoundaryConditionsMap.end()) &&
           "MoreauJeanGOSI::computeW(t,ds) - W(ds) does not exists. Maybe you forget to initialize the osi?");

    SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[dsN];

    SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

    int columnindex = 0;

    std::vector<unsigned int>::iterator itindex;

    SP::BoundaryCondition bc;
    if (dsType == Type::LagrangianDS || dsType == Type::LagrangianLinearTIDS)
    {
      LagrangianDS& d = static_cast<LagrangianDS&> (*ds);
      bc = d.boundaryConditions();
    }
    else if (dsType == Type::NewtonEulerDS)
    {
      NewtonEulerDS& d = static_cast<NewtonEulerDS&> (*ds);
      bc = d.boundaryConditions();
    }
    SP::SiconosMatrix W = _dynamicalSystemsGraph->properties(_dynamicalSystemsGraph->descriptor(ds)).W;

    for (itindex = bc->velocityIndices()->begin() ;
         itindex != bc->velocityIndices()->end();
         ++itindex)
    {

      W->getCol(*itindex, *columntmp);
      /*\warning we assume that W is symmetric in the Lagrangian case
        we store only the column and not the row */

      WBoundaryConditions->setCol(columnindex, *columntmp);
      double diag = (*columntmp)(*itindex);
      columntmp->zero();
      (*columntmp)(*itindex) = diag;

      W->setCol(*itindex, *columntmp);
      W->setRow(*itindex, *columntmp);


      columnindex ++;
    }
    DEBUG_EXPR(W->display());
  }
  else
    RuntimeException::selfThrow("MoreauJeanGOSI::computeWBoundaryConditions - not yet implemented for Dynamical system type : " +  Type::name(*ds));
}


void MoreauJeanGOSI::computeW(double t, SP::DynamicalSystem ds, SiconosMatrix& W)
{
  // Compute W matrix of the Dynamical System ds, at time t and for the current ds state.
  DEBUG_PRINT("MoreauJeanGOSI::computeW starts\n");

  assert(ds &&
         "MoreauJeanGOSI::computeW(t,ds) - ds == NULL");

  double h = _simulation->timeStep();
  Type::Siconos dsType = Type::value(*ds);

  if (dsType == Type::LagrangianLinearTIDS)
  {
    // Nothing: W does not depend on time.
  }
  else if (dsType == Type::LagrangianDS)
  {

    SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);
    SP::SiconosMatrix K = d->jacobianqForces(); // jacobian according to q
    SP::SiconosMatrix C = d->jacobianqDotForces(); // jacobian according to velocity

    d->computeMass();
    W = *d->mass();

    if (C)
    {
      d->computeJacobianqDotForces(t);
      scal(-h * _theta, *C, W, false); // W -= h*_theta*C
    }

    if (K)
    {
      d->computeJacobianqForces(t);
      scal(-h * h * _theta * _theta, *K, W, false); //*W -= h*h*_theta*_theta**K;
    }
  }
  // === ===
  else if (dsType == Type::NewtonEulerDS)
  {
    SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
    W = *(d->mass());

    SP::SiconosMatrix K = d->jacobianqForces(); // jacobian according to q
    SP::SiconosMatrix C = d->jacobianvForces(); // jacobian according to velocity

    if (C)
    {
      d->computeJacobianvForces(t);
      scal(-h * _theta, *C, W, false); // W -= h*_theta*C
    }
    if (K)
    {
      d->computeJacobianqForces(t);
      SP::SiconosMatrix T = d->T();
      DEBUG_EXPR(T->display(););
      DEBUG_EXPR(K->display(););
      SP::SimpleMatrix  buffer (new SimpleMatrix(*(d->mass())));
      prod(*K, *T, *buffer, true);
      scal(-h * h * _theta * _theta, *buffer, W, false);
      //*W -= h*h*_theta*_theta**K;
    }
  }
  else RuntimeException::selfThrow("MoreauJeanGOSI::computeW - not yet implemented for Dynamical system of type : " +Type::name(*ds));
  DEBUG_PRINT("MoreauJeanGOSI::computeW ends\n");
  // Remark: W is not LU-factorized here.
  // Function PLUForwardBackward will do that if required.
}

void MoreauJeanGOSI::computeInitialNewtonState()
{
  DEBUG_PRINT("MoreauJeanGOSI::computeInitialNewtonState() starts\n");
  // Compute the position value giving the initial velocity.
  // The goal of to save one newton iteration for nearly linear system
  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
  {
    if (!checkOSI(dsi)) continue;
    SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

    if (_explicitNewtonEulerDSOperators)
    {
      if (Type::value(*ds) == Type::NewtonEulerDS){
        // The goal is to update T() and MObjToAbs() one time at the beginning of the Newton Loop
        // We want to be explicit on this function since we do not compute their Jacobians.
        SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
        SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
        //SP::SiconosVector q = d->q();
        computeT(qold,d->T());
        computeMObjToAbs(qold,d->MObjToAbs());
      }
    }
    // The goal is to converge in one iteration of the system is almost linear
    // we start the Newton loop q = q0+hv0
    updatePosition(ds);


  }
  DEBUG_PRINT("MoreauJeanGOSI::computeInitialNewtonState() ends\n");
}



double MoreauJeanGOSI::computeResidu()
{
  DEBUG_PRINT("\nMoreauJeanGOSI::computeResidu(), start\n");
  // This function is used to compute the residu for each "MoreauJeanGOSI-discretized" dynamical system.
  // It then computes the norm of each of them and finally return the maximum
  // value for those norms.
  //
  // The state values used are those saved in the DS, ie the last computed ones.
  //  $\mathcal R(x,r) = x - x_{k} -h\theta f( x , t_{k+1}) - h(1-\theta)f(x_k,t_k) - h r$
  //  $\mathcal R_{free}(x,r) = x - x_{k} -h\theta f( x , t_{k+1}) - h(1-\theta)f(x_k,t_k) $

  double t = _simulation->nextTime(); // End of the time step
  double told = _simulation->startingTime(); // Beginning of the time step
  double h = t - told; // time step length

  DEBUG_PRINTF("nextTime %f\n", t);
  DEBUG_PRINTF("startingTime %f\n", told);
  DEBUG_PRINTF("time step size %f\n", h);


  // Operators computed at told have index i, and (i+1) at t.

  // Iteration through the set of Dynamical Systems.
  //
  SP::DynamicalSystem ds; // Current Dynamical System.
  Type::Siconos dsType ; // Type of the current DS.

  double maxResidu = 0;
  double normResidu = maxResidu;

  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
  {
    if (!checkOSI(dsi)) continue;
    ds = _dynamicalSystemsGraph->bundle(*dsi);

    dsType = Type::value(*ds); // Its type
    SP::SiconosVector residuFree = ds->workspace(DynamicalSystem::freeresidu);
    // 3 - Lagrangian Non Linear Systems
    if (dsType == Type::LagrangianDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::computeResidu(), dsType == Type::LagrangianDS\n");
      // residu = M(q*)(v_k,i+1 - v_i) - h*theta*forces(t_i+1,v_k,i+1, q_k,i+1) - h*(1-theta)*forces(ti,vi,qi) - p_i+1

      // -- Convert the DS into a Lagrangian one.
      SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);

      // Get state i (previous time step) from Memories -> var. indexed with "Old"
      SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);
      SP::SiconosVector q = d->q();


      d->computeMass();
      SP::SiconosMatrix M = d->mass();
      SP::SiconosVector v = d->velocity(); // v = v_k,i+1
      //residuFree->zero();
      DEBUG_EXPR(residuFree->display());

      DEBUG_EXPR(qold->display());
      DEBUG_EXPR(vold->display());
      DEBUG_EXPR(q->display());
      DEBUG_EXPR(v->display());

      DEBUG_EXPR(M->display());


      //    std::cout << "(*v-*vold)->norm2()" << (*v-*vold).norm2() << std::endl;

      prod(*M, (*v - *vold), *residuFree); // residuFree = M(v - vold)

      if (d->forces())
      {
        // Cheaper version: get forces(ti,vi,qi) from memory
        SP::SiconosVector fold = d->forcesMemory()->getSiconosVector(0);
        double coef = -h * (1 - _theta);
        scal(coef, *fold, *residuFree, false);

        // Expensive computes forces(ti,vi,qi)
        // d->computeForces(told, qold, vold);
        // double coef = -h * (1 - _theta);
        // // residuFree += coef * fL_i
        // scal(coef, *d->forces(), *residuFree, false);

        // computes forces(ti+1, v_k,i+1, q_k,i+1) = forces(t,v,q)
        d->computeForces(t,q,v);
        coef = -h * _theta;
        scal(coef, *d->forces(), *residuFree, false);

        // or  forces(ti+1, v_k,i+\theta, q(v_k,i+\theta))
        //SP::SiconosVector qbasedonv(new SiconosVector(*qold));
        //*qbasedonv +=  h * ((1 - _theta)* *vold + _theta * *v);
        //d->computeForces(t, qbasedonv, v);
        //coef = -h * _theta;
        // residuFree += coef * fL_k,i+1
        //scal(coef, *d->forces(), *residuFree, false);


      }

      if (d->boundaryConditions())
      {
        d->boundaryConditions()->computePrescribedVelocity(t);

        unsigned int columnindex = 0;
        SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[ds->number()];
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {
          double DeltaPrescribedVelocity =
            d->boundaryConditions()->prescribedVelocity()->getValue(columnindex)
            - v->getValue(*itindex);

          WBoundaryConditions->getCol(columnindex, *columntmp);
          *residuFree -= *columntmp * (DeltaPrescribedVelocity);

          residuFree->setValue(*itindex, - columntmp->getValue(*itindex)   * (DeltaPrescribedVelocity));

          columnindex ++;
        }
      }

      *(d->workspace(DynamicalSystem::free)) = *residuFree; // copy residuFree in Workfree

      //       std::cout << "MoreauJeanGOSI::ComputeResidu LagrangianDS residufree :"  << std::endl;
      DEBUG_EXPR(residuFree->display());

      if (d->p(1))
        *(d->workspace(DynamicalSystem::free)) -= *d->p(1); // Compute Residu in Workfree Notation !!
                                                            // We use DynamicalSystem::free as tmp buffer

      if (d->boundaryConditions())
      {
        unsigned int columnindex = 0;
        SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[ds->number()];
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {
          double DeltaPrescribedVelocity =
            d->boundaryConditions()->prescribedVelocity()->getValue(columnindex)
            - v->getValue(*itindex);

          WBoundaryConditions->getCol(columnindex, *columntmp);

          d->workspace(DynamicalSystem::free)->setValue(*itindex, - columntmp->getValue(*itindex)   * (DeltaPrescribedVelocity));

          columnindex ++;
        }
      }


      DEBUG_EXPR(d->workspace(DynamicalSystem::free)->display());
      normResidu = d->workspace(DynamicalSystem::free)->norm2();
      DEBUG_PRINTF("normResidu= %e\n", normResidu);
    }
    // 4 - Lagrangian Linear Systems
    else if (dsType == Type::LagrangianLinearTIDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::computeResidu(), dsType == Type::LagrangianLinearTIDS\n");
      // ResiduFree = h*C*v_i + h*Kq_i +h*h*theta*Kv_i+hFext_theta     (1)
      // This formulae is only valid for the first computation of the residual for v = v_i
      // otherwise the complete formulae must be applied, that is
      // ResiduFree = M(v - vold) + h*((1-theta)*(C v_i + K q_i) +theta * ( C*v + K(q_i+h(1-theta)v_i+h theta v)))
      //                     +hFext_theta     (2)
      // for v != vi, the formulae (1) is wrong.
      // in the sequel, only the equation (1) is implemented

      // -- Convert the DS into a Lagrangian one.
      SP::LagrangianLinearTIDS d = std11::static_pointer_cast<LagrangianLinearTIDS> (ds);

      // Get state i (previous time step) from Memories -> var. indexed with "Old"
      SP::SiconosVector qold = d->qMemory()->getSiconosVector(0); // qi
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0); //vi

      DEBUG_EXPR(qold->display(););
      DEBUG_EXPR(vold->display(););
      DEBUG_EXPR(d->q()->display(););
      DEBUG_EXPR(d->velocity()->display(););

      // --- ResiduFree computation Equation (1) ---
      residuFree->zero();
      double coeff;
      // -- No need to update W --

      SP::SiconosVector v = d->velocity(); // v = v_k,i+1

      SP::SiconosMatrix C = d->C();
      if (C)
        prod(h, *C, *vold, *residuFree, false); // vfree += h*C*vi

      SP::SiconosMatrix K = d->K();
      if (K)
      {
        coeff = h * h * _theta;
        prod(coeff, *K, *vold, *residuFree, false); // vfree += h^2*_theta*K*vi
        prod(h, *K, *qold, *residuFree, false); // vfree += h*K*qi
      }

      SP::SiconosVector Fext = d->fExt();
      if (Fext)
      {
        // computes Fext(ti)
        d->computeFExt(told);
        coeff = -h * (1 - _theta);
        scal(coeff, *(d->fExt()), *residuFree, false); // vfree -= h*(1-_theta) * fext(ti)
        // computes Fext(ti+1)
        d->computeFExt(t);
        coeff = -h * _theta;
        scal(coeff, *(d->fExt()), *residuFree, false); // vfree -= h*_theta * fext(ti+1)
      }


      // Computation of the complete residual Equation (2)
      //   ResiduFree = M(v - vold) + h*((1-theta)*(C v_i + K q_i) +theta * ( C*v + K(q_i+h(1-theta)v_i+h theta v)))
      //                     +hFext_theta     (2)
      //       SP::SiconosMatrix M = d->mass();
      //       SP::SiconosVector realresiduFree (new SiconosVector(*residuFree));
      //       realresiduFree->zero();
      //       prod(*M, (*v-*vold), *realresiduFree); // residuFree = M(v - vold)
      //       SP::SiconosVector qkplustheta (new SiconosVector(*qold));
      //       qkplustheta->zero();
      //       *qkplustheta = *qold + h *((1-_theta)* *vold + _theta* *v);
      //       if (C){
      //         double coef = h*(1-_theta);
      //         prod(coef, *C, *vold , *realresiduFree, false);
      //         coef = h*(_theta);
      //         prod(coef,*C, *v , *realresiduFree, false);
      //       }
      //       if (K){
      //         double coef = h*(1-_theta);
      //         prod(coef,*K , *qold , *realresiduFree, false);
      //         coef = h*(_theta);
      //         prod(coef,*K , *qkplustheta , *realresiduFree, false);
      //       }

      //       if (Fext)
      //       {
      //         // computes Fext(ti)
      //         d->computeFExt(told);
      //         coeff = -h*(1-_theta);
      //         scal(coeff, *Fext, *realresiduFree, false); // vfree -= h*(1-_theta) * fext(ti)
      //         // computes Fext(ti+1)
      //         d->computeFExt(t);
      //         coeff = -h*_theta;
      //         scal(coeff, *Fext, *realresiduFree, false); // vfree -= h*_theta * fext(ti+1)
      //       }


      if (d->boundaryConditions())
      {
        d->boundaryConditions()->computePrescribedVelocity(t);

        unsigned int columnindex = 0;
        SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[ds->number()];
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {

          double DeltaPrescribedVelocity =
            d->boundaryConditions()->prescribedVelocity()->getValue(columnindex)
            - vold->getValue(*itindex);

          WBoundaryConditions->getCol(columnindex, *columntmp);
          *residuFree += *columntmp * (DeltaPrescribedVelocity);

          residuFree->setValue(*itindex, - columntmp->getValue(*itindex)   * (DeltaPrescribedVelocity));

          columnindex ++;

        }
      }

      (* d->workspace(DynamicalSystem::free)) = *residuFree; // copy residuFree in Workfree
      if (d->p(1))
        *(d->workspace(DynamicalSystem::free)) -= *d->p(1); // Compute Residu in Workfree Notation !!
                                                            // We use DynamicalSystem::free as tmp buffer

      //      std::cout << "MoreauJeanGOSI::ComputeResidu LagrangianLinearTIDS residu :"  << std::endl;
      //      d->workspace(DynamicalSystem::free)->display();


      //     normResidu = d->workspace(DynamicalSystem::free)->norm2();
      normResidu = 0.0; // we assume that v = vfree + W^(-1) p
      //     normResidu = realresiduFree->norm2();
      //DEBUG_PRINTF("normResidu (really computed) = %e\n", d->workspace(DynamicalSystem::free)->norm2() );
    }
    else if (dsType == Type::NewtonEulerDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::computeResidu(), dsType == Type::NewtonEulerDS\n");
      // residu = M (v_k,i+1 - v_i) - h*_theta*forces(t,v_k,i+1, q_k,i+1) - h*(1-_theta)*forces(ti,vi,qi) - pi+1

      // -- Convert the DS into a Lagrangian one.
      SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);

      // Get the state  (previous time step) from memory vector
      // -> var. indexed with "Old"
      SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);


      // Get the current state vector
      SP::SiconosVector q = d->q();
      SP::SiconosVector v = d->velocity(); // v = v_k,i+1

      // Get the (constant mass matrix)
      SP::SiconosMatrix massMatrix = d->mass();
      prod(*massMatrix, (*v - *vold), *residuFree, true); // residuFree = M(v - vold)
      DEBUG_EXPR(residuFree->display(););

      if (d->forces())  // if fL exists
      {
        DEBUG_PRINTF("MoreauJeanGOSI:: _theta = %e\n",_theta);
        DEBUG_PRINTF("MoreauJeanGOSI:: h = %e\n",h );

        // Cheaper version: get forces(ti,vi,qi) from memory
        SP::SiconosVector fold = d->forcesMemory()->getSiconosVector(0);
        double coef = -h * (1 - _theta);
        scal(coef, *fold, *residuFree, false);

        // Expensive version to check ...
        //d->computeForces(told,qold,vold);
        //double coef = -h * (1.0 - _theta);
        //scal(coef, *d->forces(), *residuFree, false);

        DEBUG_PRINT("MoreauJeanGOSI:: old forces :\n");
        DEBUG_EXPR(d->forces()->display(););
        DEBUG_EXPR(residuFree->display(););

        // computes forces(ti,v,q)
        d->computeForces(t,q,v);
        coef = -h * _theta;
        scal(coef, *d->forces(), *residuFree, false);
        DEBUG_PRINT("MoreauJeanGOSI:: new forces :\n");
        DEBUG_EXPR(d->forces()->display(););
        DEBUG_EXPR(residuFree->display(););

      }


      if (d->boundaryConditions())
      {
        d->boundaryConditions()->computePrescribedVelocity(t);

        unsigned int columnindex = 0;
        SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[ds->number()];
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {

          DEBUG_PRINTF("columnindex = %i\n",columnindex);
          DEBUG_PRINTF("*itindex = %i\n",*itindex);
          double DeltaPrescribedVelocity =
            d->boundaryConditions()->prescribedVelocity()->getValue(columnindex)
            - v->getValue(*itindex);

          DEBUG_EXPR(d->boundaryConditions()->prescribedVelocity()->display());

          WBoundaryConditions->getCol(columnindex, *columntmp);
          *residuFree -= *columntmp * (DeltaPrescribedVelocity);


          residuFree->setValue(*itindex, - columntmp->getValue(*itindex)   * (DeltaPrescribedVelocity));

          columnindex ++;
        }
      }

      *(d->workspace(DynamicalSystem::free)) = *residuFree;
      if (d->p(1))
        *(d->workspace(DynamicalSystem::free)) -= *d->p(1);// We use DynamicalSystem::free as tmp buffer


      if (d->boundaryConditions())
      {
        unsigned int columnindex = 0;
        SP::SimpleMatrix WBoundaryConditions = _WBoundaryConditionsMap[ds->number()];
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {
          double DeltaPrescribedVelocity =
            d->boundaryConditions()->prescribedVelocity()->getValue(columnindex)
            - v->getValue(*itindex);

          WBoundaryConditions->getCol(columnindex, *columntmp);

          d->workspace(DynamicalSystem::free)->setValue(*itindex, - columntmp->getValue(*itindex)   * (DeltaPrescribedVelocity));

          columnindex ++;
        }
      }

      DEBUG_PRINT("MoreauJeanGOSI::computeResidu :\n");
      DEBUG_EXPR(residuFree->display(););
      DEBUG_EXPR(if (d->p(1)) d->p(1)->display(););
      DEBUG_EXPR((d->workspace(DynamicalSystem::free))->display(););

      normResidu = d->workspace(DynamicalSystem::free)->norm2();
      DEBUG_PRINTF("normResidu= %e\n", normResidu);
    }
    else
      RuntimeException::selfThrow("MoreauJeanGOSI::computeResidu - not yet implemented for Dynamical system of type: " + Type::name(*ds));

    if (normResidu > maxResidu) maxResidu = normResidu;

  }
  return maxResidu;
}

void MoreauJeanGOSI::computeFreeState()
{
  DEBUG_PRINT("\nMoreauJeanGOSI::computeFreeState() starts\n");
  // This function computes "free" states of the DS belonging to this Integrator.
  // "Free" means without taking non-smooth effects into account.

  double t = _simulation->nextTime(); // End of the time step

  // Operators computed at told have index i, and (i+1) at t.

  //  Note: integration of r with a theta method has been removed
  //  SiconosVector *rold = static_cast<SiconosVector*>(d->rMemory()->getSiconosVector(0));

  // Iteration through the set of Dynamical Systems.
  //


  SP::DynamicalSystem ds; // Current Dynamical System.
  SP::SiconosMatrix W; // W MoreauJeanGOSI matrix of the current DS.
  Type::Siconos dsType ; // Type of the current DS.

  DynamicalSystemsGraph::VIterator dsi, dsend;

  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
   {
    if (!checkOSI(dsi)) continue;
    ds = _dynamicalSystemsGraph->bundle(*dsi);
    dsType = Type::value(*ds); // Its type
    SiconosMatrix& W = *_dynamicalSystemsGraph->properties(*dsi).W;
    // 3 - Lagrangian Non Linear Systems
    if (dsType == Type::LagrangianDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::computeFreeState(), dsType == Type::LagrangianDS\n");
      // IN to be updated at current time: W, M, q, v, fL
      // IN at told: qi,vi, fLi

      // Note: indices i/i+1 corresponds to value at the beginning/end of the time step.
      // Index k stands for Newton iteration and thus corresponds to the last computed
      // value, ie the one saved in the DynamicalSystem.
      // "i" values are saved in memory vectors.

      // vFree = v_k,i+1 - W^{-1} ResiduFree
      // with
      // ResiduFree = M(q_k,i+1)(v_k,i+1 - v_i) - h*theta*forces(t,v_k,i+1, q_k,i+1) - h*(1-theta)*forces(ti,vi,qi)

      // -- Convert the DS into a Lagrangian one.
      SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);

      // Get state i (previous time step) from Memories -> var. indexed with "Old"
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);
      SP::SiconosVector v = d->velocity(); // v = v_k,i+1
      DEBUG_EXPR(vold->display());
      DEBUG_EXPR(v->display());


      // --- ResiduFree computation ---
      // ResFree = M(v-vold) - h*[theta*forces(t) + (1-theta)*forces(told)]
      //
      // vFree pointer is used to compute and save ResiduFree in this first step.
      SiconosVector& vfree = *d->workspace(DynamicalSystem::free);//workX[d];
      vfree = *(d->workspace(DynamicalSystem::freeresidu));

      computeW(t, d, W);

      // -- vfree =  v - W^{-1} ResiduFree --
      // At this point vfree = residuFree
      // -> Solve WX = vfree and set vfree = X
      W.PLUForwardBackwardInPlace(vfree);
      // -> compute real vfree
      vfree *= -1.0;
      vfree += *v;
      DEBUG_EXPR(vfree.display());

    }
    // 4 - Lagrangian Linear Systems
    else if (dsType == Type::LagrangianLinearTIDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::computeFreeState(), dsType == Type::LagrangianLinearTIDS\n");
      // IN to be updated at current time: Fext
      // IN at told: qi,vi, fext
      // IN constants: K,C

      // Note: indices i/i+1 corresponds to value at the beginning/end of the time step.
      // "i" values are saved in memory vectors.

      // vFree = v_i + W^{-1} ResiduFree    // with
      // ResiduFree = (-h*C -h^2*theta*K)*vi - h*K*qi + h*theta * Fext_i+1 + h*(1-theta)*Fext_i

      // -- Convert the DS into a Lagrangian one.
      SP::LagrangianLinearTIDS d = std11::static_pointer_cast<LagrangianLinearTIDS> (ds);

      // Get state i (previous time step) from Memories -> var. indexed with "Old"
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0); //vi

      // --- ResiduFree computation ---
      // vFree pointer is used to compute and save ResiduFree in this first step.

      // Velocity free and residu. vFree = RESfree (pointer equality !!).
      SP::SiconosVector vfree = d->workspace(DynamicalSystem::free);//workX[d];
      (*vfree) = *(d->workspace(DynamicalSystem::freeresidu));

      W.PLUForwardBackwardInPlace(*vfree);
      *vfree *= -1.0;
      *vfree += *vold;

      DEBUG_EXPR(vfree->display());


    }
    else if (dsType == Type::NewtonEulerDS)
    {
      // IN to be updated at current time: W, M, q, v, fL
      // IN at told: qi,vi, fLi

      // Note: indices i/i+1 corresponds to value at the beginning/end of the time step.
      // Index k stands for Newton iteration and thus corresponds to the last computed
      // value, ie the one saved in the DynamicalSystem.
      // "i" values are saved in memory vectors.

      // vFree = v_k,i+1 - W^{-1} ResiduFree
      // with
      // ResiduFree = M(q_k,i+1)(v_k,i+1 - v_i) - h*theta*forces(t,v_k,i+1, q_k,i+1)
      //                                        - h*(1-theta)*forces(ti,vi,qi)

      // -- Convert the DS into a NewtonEuler one.
      SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);

      // Get state i (previous time step) from Memories -> var. indexed with "Old"
      SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);

      // --- ResiduFree computation ---
      // ResFree = M(v-vold) - h*[theta*forces(t) + (1-theta)*forces(told)]
      //
      // vFree pointer is used to compute and save ResiduFree in this first step.
      SP::SiconosVector vfree = d->workspace(DynamicalSystem::free);//workX[d];
      (*vfree) = *(d->workspace(DynamicalSystem::freeresidu));
      //*(d->vPredictor())=*(d->workspace(DynamicalSystem::freeresidu));

      // -- Update W --
      // Note: during computeW, mass and jacobians of forces will be computed/
      computeW(t, d, W);
      SP::SiconosVector v = d->velocity(); // v = v_k,i+1

      // -- vfree =  v - W^{-1} ResiduFree --
      // At this point vfree = residuFree
      // -> Solve WX = vfree and set vfree = X
      //    std::cout<<"MoreauJeanGOSI::computeFreeState residu free"<<endl;
      //    vfree->display();
      W.PLUForwardBackwardInPlace(*vfree);
      //    std::cout<<"MoreauJeanGOSI::computeFreeState -WRfree"<<endl;
      //    vfree->display();
      //    scal(h,*vfree,*vfree);
      // -> compute real vfree
      *vfree *= -1.0;
      *vfree += *v;
    }
    else
      RuntimeException::selfThrow("MoreauJeanGOSI::computeFreeState - not yet implemented for Dynamical system of type: " +  Type::name(*ds));
  }
  DEBUG_PRINT("MoreauJeanGOSI::computeFreeState() ends\n");

}

void MoreauJeanGOSI::prepareNewtonIteration(double time)
{
  DEBUG_BEGIN(" MoreauJeanOSI::prepareNewtonIteration(double time)\n");
  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
   {
    if (!checkOSI(dsi)) continue;
    SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);
    SiconosMatrix& W = *_dynamicalSystemsGraph->properties(*dsi).W;
    computeW(time, ds, W);
  }

  if (!_explicitNewtonEulerDSOperators)
  {
    DynamicalSystemsGraph::VIterator dsi, dsend;

    for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
    {
      if (!checkOSI(dsi)) continue;

      SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

      //  VA <2016-04-19 Tue> We compute T and MObjToAbs to be consitent with the Jacobian at the beginning of the Newton iteration and not at the end
      Type::Siconos dsType = Type::value(*ds);
      if (dsType == Type::NewtonEulerDS)
      {
        SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
        computeT(d->q(),d->T());
        computeMObjToAbs(d->q(),d->MObjToAbs());
      }
    }

  }


  DEBUG_END(" MoreauJeanOSI::prepareNewtonIteration(double time)\n");

}


struct MoreauJeanGOSI::_NSLEffectOnFreeOutput : public SiconosVisitor
{
  using SiconosVisitor::visit;

  OneStepNSProblem& _osnsp;
  Interaction& _inter;

  _NSLEffectOnFreeOutput(OneStepNSProblem& p, Interaction& inter) :
    _osnsp(p), _inter(inter) {};

  void visit(const NewtonImpactNSL& nslaw)
  {
    double e;
    e = nslaw.e();
    Index subCoord(4);
    subCoord[0] = 0;
    subCoord[1] = _inter.nonSmoothLaw()->size();
    subCoord[2] = 0;
    subCoord[3] = subCoord[1];
    subscal(e, *_inter.y_k(_osnsp.inputOutputLevel()), *(_inter.yForNSsolver()), subCoord, true);
  }

  void visit(const NewtonImpactFrictionNSL& nslaw)
  {
    double e;
    e = nslaw.en();
    // Only the normal part is multiplied by e
    (*_inter.yForNSsolver())(0) =  e * (*_inter.y_k(_osnsp.inputOutputLevel()))(0);

  }
  void visit(const EqualityConditionNSL& nslaw)
  {
    ;
  }
  void visit(const MixedComplementarityConditionNSL& nslaw)
  {
    ;
  }
};

void MoreauJeanGOSI::NSLcontrib(Interaction& inter, OneStepNSProblem& osnsp)
{
  if (inter.relation()->getType() == Lagrangian || inter.relation()->getType() == NewtonEuler)
  {
    _NSLEffectOnFreeOutput nslEffectOnFreeOutput = _NSLEffectOnFreeOutput(osnsp, inter);
    inter.nonSmoothLaw()->accept(nslEffectOnFreeOutput);
  }
}

void MoreauJeanGOSI::integrate(double& tinit, double& tend, double& tout, int& notUsed)
{
  // Last parameter is not used (required for LsodarOSI but not for MoreauJeanGOSI).

  double h = tend - tinit;
  tout = tend;


  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
  {
    SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

    Type::Siconos dsType = Type::value(*ds);

    if (dsType == Type::LagrangianLinearTIDS)
    {
      SiconosMatrix& W = *_dynamicalSystemsGraph->properties(*dsi).W;
      // get the ds
      SP::LagrangianLinearTIDS d = std11::static_pointer_cast<LagrangianLinearTIDS> (ds);
      // get velocity pointers for current time step
      SP::SiconosVector v = d->velocity();
      // get q and velocity pointers for previous time step
      SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);
      SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
      // get p pointer

      SP::SiconosVector p = d->p(1);

      // velocity computation :
      //
      // v = vi + W^{-1}[ -h*C*vi - h*h*theta*K*vi - h*K*qi + h*theta*Fext(t) + h*(1-theta) * Fext(ti) ] + W^{-1}*pi+1
      //

      *v = *p; // v = p

      double coeff;
      // -- No need to update W --
      SP::SiconosMatrix C = d->C();
      if (C)
        prod(-h, *C, *vold, *v, false); // v += -h*C*vi

      SP::SiconosMatrix K = d->K();
      if (K)
      {
        coeff = -h * h * _theta;
        prod(coeff, *K, *vold, *v, false); // v += -h^2*theta*K*vi
        prod(-h, *K, *qold, *v, false); // v += -h*K*qi
      }

      SP::SiconosVector Fext = d->fExt();
      if (Fext)
      {
        // computes Fext(ti)
        d->computeFExt(tinit);
        coeff = h * (1 - _theta);
        scal(coeff, *Fext, *v, false); // v += h*(1-theta) * fext(ti)
        // computes Fext(ti+1)
        d->computeFExt(tout);
        coeff = h * _theta;
        scal(coeff, *Fext, *v, false); // v += h*theta * fext(ti+1)
      }
      // -> Solve WX = v and set v = X
      W.PLUForwardBackwardInPlace(*v);
      *v += *vold;
    }
    else RuntimeException::selfThrow("MoreauJeanGOSI::integrate - not yet implemented for Dynamical system of type :" +  Type::name(*ds));
  }
}
void MoreauJeanGOSI::updatePosition(SP::DynamicalSystem ds)
{
  DEBUG_PRINT("\nMoreauJeanGOSI::updateState(SP::DynamicalSystem ds)\n");

  double h = _simulation->timeStep();

  Type::Siconos dsType = Type::value(*ds);

  // 1 - Lagrangian Systems
  if (dsType == Type::LagrangianDS || dsType == Type::LagrangianLinearTIDS)
  {
    // get dynamical system
    SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);

    // Compute q
    SP::SiconosVector v = d->velocity();
    SP::SiconosVector q = d->q();
    //  -> get previous time step state
    SP::SiconosVector vold = d->velocityMemory()->getSiconosVector(0);
    SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
    // *q = *qold + h*(theta * *v +(1.0 - theta)* *vold)
    double coeff = h * _theta;
    scal(coeff, *v, *q) ; // q = h*theta*v
    coeff = h * (1 - _theta);
    scal(coeff, *vold, *q, false); // q += h(1-theta)*vold
    *q += *qold;
  }
  else if (dsType == Type::NewtonEulerDS)
  {
    // get dynamical system
    SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
    SP::SiconosVector v = d->velocity();
    DEBUG_PRINT("MoreauJeanGOSI::updateState()\n ")
      DEBUG_EXPR(d->display());
    DEBUG_PRINT("MoreauJeanGOSI::updateState() prev v\n")
      DEBUG_EXPR(v->display());

    //compute q
    //first step consists in computing  \dot q.
    //second step consists in updating q.
    //
    SP::SiconosMatrix T = d->T();
    SP::SiconosVector dotq = d->dotq();
    prod(*T, *v, *dotq, true);

    DEBUG_PRINT("MoreauJeanGOSI::updateState v\n");
    DEBUG_EXPR(v->display());
    DEBUG_EXPR(dotq->display());

    SP::SiconosVector q = d->q();

    //  -> get previous time step state
    SP::SiconosVector dotqold = d->dotqMemory()->getSiconosVector(0);
    SP::SiconosVector qold = d->qMemory()->getSiconosVector(0);
    // *q = *qold + h*(theta * *v +(1.0 - theta)* *vold)
    double coeff = h * _theta;
    scal(coeff, *dotq, *q) ; // q = h*theta*v
    coeff = h * (1 - _theta);
    scal(coeff, *dotqold, *q, false); // q += h(1-theta)*vold
    *q += *qold;
    DEBUG_PRINT("new q before normalizing\n");
    DEBUG_EXPR(q->display());

    //q[3:6] must be normalized
    d->normalizeq();

    /* \warning VA 02/06/2013.
     * What is the reason of doing the following computation ?
     */
    // dotq->setValue(3, (q->getValue(3) - qold->getValue(3)) / h);
    // dotq->setValue(4, (q->getValue(4) - qold->getValue(4)) / h);
    // dotq->setValue(5, (q->getValue(5) - qold->getValue(5)) / h);
    // dotq->setValue(6, (q->getValue(6) - qold->getValue(6)) / h);

    // d->computeT(); //  VA 09/06/2015. We prefer only compute T() every time--step for Newton convergence reasons.

  }
}

void MoreauJeanGOSI::updateState(const unsigned int level)
{

  DEBUG_PRINT("MoreauJeanGOSI::updateState(const unsigned int level)\n");

  double RelativeTol = _simulation->relativeConvergenceTol();
  bool useRCC = _simulation->useRelativeConvergenceCriteron();
  if (useRCC)
    _simulation->setRelativeConvergenceCriterionHeld(true);

  DynamicalSystemsGraph::VIterator dsi, dsend;
  for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
  {
    if (!checkOSI(dsi)) continue;
    SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

    SiconosMatrix& W = *_dynamicalSystemsGraph->properties(*dsi).W;
    // Get the DS type

    Type::Siconos dsType = Type::value(*ds);

    // 3 - Lagrangian Systems
    if (dsType == Type::LagrangianDS || dsType == Type::LagrangianLinearTIDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::updateState(const unsigned int level), dsType == Type::LagrangianDS || dsType == Type::LagrangianLinearTIDS \n");
      // get dynamical system
      SP::LagrangianDS d = std11::static_pointer_cast<LagrangianDS> (ds);

      //    SiconosVector *vfree = d->velocityFree();
      SP::SiconosVector v = d->velocity();
      bool baux = dsType == Type::LagrangianDS && useRCC && _simulation->relativeConvergenceCriterionHeld();

      // level == LEVELMAX => p(level) does not even exists (segfault)
      // \warning VA 04/08/2015. Why must we check that  d->p(level)->size() > 0 ?
      if (level != LEVELMAX && d->p(level) && d->p(level)->size() > 0)
      {

        assert(((d->p(level)).get()) &&
               " MoreauJeanGOSI::updateState() *d->p(level) == NULL.");
        *v = *d->p(level); // v = p
        if (d->boundaryConditions())
          for (std::vector<unsigned int>::iterator
                 itindex = d->boundaryConditions()->velocityIndices()->begin() ;
               itindex != d->boundaryConditions()->velocityIndices()->end();
               ++itindex)
            v->setValue(*itindex, 0.0);
        W.PLUForwardBackwardInPlace(*v);

        *v +=  * ds->workspace(DynamicalSystem::free);
      }
      else
      {
        *v =  * ds->workspace(DynamicalSystem::free);
      }
      DEBUG_EXPR(v->display());



      if (d->boundaryConditions())
      {
        int bc = 0;
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {
          _WBoundaryConditionsMap[ds->number()]->getCol(bc, *columntmp);
          /*\warning we assume that W is symmetric in the Lagrangian case*/
          double value = - inner_prod(*columntmp, *v);
          if (level != LEVELMAX && d->p(level)&& d->p(level)->size() > 0)
          {
            value += (d->p(level))->getValue(*itindex);
          }
          /* \warning the computation of reactionToBoundaryConditions take into
             account the contact impulse but not the external and internal forces.
             A complete computation of the residu should be better */
          d->reactionToBoundaryConditions()->setValue(bc, value) ;
          bc++;
        }
      }

      SP::SiconosVector q = d->q();
      // Save value of q in stateTmp for future convergence computation
      if (baux)
        ds->addWorkVector(q, DynamicalSystem::local_buffer);

      updatePosition(ds);

      if (baux)
      {
        ds->subWorkVector(q, DynamicalSystem::local_buffer);
        double aux = ((ds->workspace(DynamicalSystem::local_buffer))->norm2()) / (ds->normRef());
        if (aux > RelativeTol)
          _simulation->setRelativeConvergenceCriterionHeld(false);
      }

    }
    else if (dsType == Type::NewtonEulerDS)
    {
      DEBUG_PRINT("MoreauJeanGOSI::updateState(const unsigned int level), dsType == Type::NewtonEulerDS \n");

      // get dynamical system
      SP::NewtonEulerDS d = std11::static_pointer_cast<NewtonEulerDS> (ds);
      SP::SiconosVector v = d->velocity();
      DEBUG_PRINT("MoreauJeanGOSI::updateState()\n ")
      DEBUG_EXPR(d->display());
      DEBUG_PRINT("MoreauJeanGOSI::updateState() prev v\n")
      DEBUG_EXPR(v->display());

      // failure on bullet sims
      // d->p(level) is checked in next condition
      // assert(((d->p(level)).get()) &&
      //       " MoreauJeanGOSI::updateState() *d->p(level) == NULL.");

      if (level != LEVELMAX && d->p(level) && d->p(level)->size() > 0)
      {
        /*d->p has been fill by the Relation->computeInput, it contains
          B \lambda _{k+1}*/
        *v = *d->p(level); // v = p
        if (d->boundaryConditions())
          for (std::vector<unsigned int>::iterator
                 itindex = d->boundaryConditions()->velocityIndices()->begin() ;
               itindex != d->boundaryConditions()->velocityIndices()->end();
               ++itindex)
            v->setValue(*itindex, 0.0);

        W.PLUForwardBackwardInPlace(*v);

        DEBUG_EXPR(d->p(level)->display());
        DEBUG_PRINT("MoreauJeanGOSI::updatestate W CT lambda\n");
        DEBUG_EXPR(v->display());
        *v +=  * ds->workspace(DynamicalSystem::free);
      }
      else
        *v =  * ds->workspace(DynamicalSystem::free);

      DEBUG_PRINT("MoreauJeanGOSI::updatestate work free\n");
      DEBUG_EXPR(ds->workspace(DynamicalSystem::free)->display());
      DEBUG_PRINT("MoreauJeanGOSI::updatestate new v\n");
      DEBUG_EXPR(v->display());

      if (d->boundaryConditions())
      {
        int bc = 0;
        SP::SiconosVector columntmp(new SiconosVector(ds->dimension()));

        for (std::vector<unsigned int>::iterator  itindex = d->boundaryConditions()->velocityIndices()->begin() ;
             itindex != d->boundaryConditions()->velocityIndices()->end();
             ++itindex)
        {
          _WBoundaryConditionsMap[ds->number()]->getCol(bc, *columntmp);
          /*\warning we assume that W is symmetric in the Lagrangian case*/
          double value = - inner_prod(*columntmp, *v);
          if (level != LEVELMAX && d->p(level) && d->p(level)->size() > 0)
          {
            value += (d->p(level))->getValue(*itindex);
          }
          /* \warning the computation of reactionToBoundaryConditions take into
             account the contact impulse but not the external and internal forces.
             A complete computation of the residu should be better */
          d->reactionToBoundaryConditions()->setValue(bc, value) ;
          bc++;
        }
      }

      updatePosition(ds);

    }
    else RuntimeException::selfThrow("MoreauJeanGOSI::updateState - not yet implemented for Dynamical system of type: " +  Type::name(*ds));

  }
}


bool MoreauJeanGOSI::addInteractionInIndexSet(SP::Interaction inter, unsigned int i)
{
  DEBUG_PRINT("addInteractionInIndexSet(SP::Interaction inter, unsigned int i)\n");

  assert(i == 1);
  double h = _simulation->timeStep();
  double y = (inter->y(i - 1))->getValue(0); // for i=1 y(i-1) is the position
  double yDot = (inter->y(i))->getValue(0); // for i=1 y(i) is the velocity

  double gamma = 1.0 / 2.0;
  if (_useGamma)
  {
    gamma = _gamma;
  }
  DEBUG_PRINTF("MoreauJeanGOSI::addInteractionInIndexSet of level = %i yref=%e, yDot=%e, y_estimated=%e.\n", i,  y, yDot, y + gamma * h * yDot);
  y += gamma * h * yDot;
  assert(!isnan(y));
  DEBUG_EXPR(
    if (y <= 0)
    DEBUG_PRINT("MoreauJeanGOSI::addInteractionInIndexSet ACTIVATE.\n");
  );
  return (y <= 0.0);
}


bool MoreauJeanGOSI::removeInteractionInIndexSet(SP::Interaction inter, unsigned int i)
{
  assert(i == 1);
  double h = _simulation->timeStep();
  double y = (inter->y(i - 1))->getValue(0); // for i=1 y(i-1) is the position
  double yDot = (inter->y(i))->getValue(0); // for i=1 y(i) is the velocity
  double gamma = 1.0 / 2.0;
  if (_useGamma)
  {
    gamma = _gamma;
  }
  DEBUG_PRINTF("MoreauJeanGOSI::addInteractionInIndexSet yref=%e, yDot=%e, y_estimated=%e.\n", y, yDot, y + gamma * h * yDot);
  y += gamma * h * yDot;
  assert(!isnan(y));

  DEBUG_EXPR(
    if (y > 0)
    DEBUG_PRINT("MoreauJeanGOSI::removeInteractionInIndexSet DEACTIVATE.\n");
  );
  return (y > 0.0);
}



void MoreauJeanGOSI::display()
{
  OneStepIntegrator::display();

  std::cout << "====== MoreauJeanOSI OSI display ======" <<std::endl;
  DynamicalSystemsGraph::VIterator dsi, dsend;
  if (_dynamicalSystemsGraph)
  {
    for (std11::tie(dsi, dsend) = _dynamicalSystemsGraph->vertices(); dsi != dsend; ++dsi)
    {
      if (!checkOSI(dsi)) continue;
      SP::DynamicalSystem ds = _dynamicalSystemsGraph->bundle(*dsi);

      std::cout << "--------------------------------" <<std::endl;
      std::cout << "--> W of dynamical system number " << ds->number() << ": " <<std::endl;
      if (_dynamicalSystemsGraph->properties(*dsi).W) _dynamicalSystemsGraph->properties(*dsi).W->display();
      else std::cout << "-> NULL" <<std::endl;
      std::cout << "--> and corresponding theta is: " << _theta <<std::endl;
    }
  }
  std::cout << "================================" <<std::endl;
}
