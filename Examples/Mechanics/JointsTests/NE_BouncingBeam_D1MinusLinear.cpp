/* Siconos-sample , Copyright INRIA 2005-2011.
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
 * Contact: Vincent ACARY vincent.acary@inrialpes.fr
 */

/*!\file NE....cpp
  \brief \ref EMNE_MULTIBODY - C++ input file, Time-Stepping version - O.B.

  A multibody example.
  Direct description of the model without XML input.
  Simulation with a Time-Stepping scheme.
*/

#include "SiconosKernel.hpp"
#include "KneeJointR.hpp"
#include "PrismaticJointR.hpp"
#include <boost/math/quaternion.hpp>
using namespace std;

/* Given a position of a point in the Inertial Frame and the configuration vector q of a solid
 * returns a position in the spatial frame.
 */
void fromInertialToSpatialFrame(double *positionInInertialFrame, double *positionInSpatialFrame, SP::SiconosVector  q  )
{
double q0 = q->getValue(3);
double q1 = q->getValue(4);
double q2 = q->getValue(5);
double q3 = q->getValue(6);

::boost::math::quaternion<double>    quatQ(q0, q1, q2, q3);
::boost::math::quaternion<double>    quatcQ(q0, -q1, -q2, -q3);
::boost::math::quaternion<double>    quatpos(0, positionInInertialFrame[0], positionInInertialFrame[1], positionInInertialFrame[2]);
::boost::math::quaternion<double>    quatBuff;

//perform the rotation
quatBuff = quatQ * quatpos * quatcQ;

positionInSpatialFrame[0] = quatBuff.R_component_2()+q->getValue(0);
positionInSpatialFrame[1] = quatBuff.R_component_3()+q->getValue(1);
positionInSpatialFrame[2] = quatBuff.R_component_4()+q->getValue(2);

}
void tipTrajectories(SP::SiconosVector  q, double * traj, double length)
{
  double positionInInertialFrame[3];
  double positionInSpatialFrame[3];
  // Output the position of the tip of beam1
  positionInInertialFrame[0]=length/2;
  positionInInertialFrame[1]=0.0;
  positionInInertialFrame[2]=0.0;
  
  fromInertialToSpatialFrame(positionInInertialFrame, positionInSpatialFrame, q  );
  traj[0] = positionInSpatialFrame[0];
  traj[1] = positionInSpatialFrame[1];
  traj[2] = positionInSpatialFrame[2];
  
  
  // std::cout <<  "positionInSpatialFrame[0]" <<  positionInSpatialFrame[0]<<std::endl;
  // std::cout <<  "positionInSpatialFrame[1]" <<  positionInSpatialFrame[1]<<std::endl;
  // std::cout <<  "positionInSpatialFrame[2]" <<  positionInSpatialFrame[2]<<std::endl;
  
  positionInInertialFrame[0]=-length/2;
  fromInertialToSpatialFrame(positionInInertialFrame, positionInSpatialFrame, q  );
  traj[3]= positionInSpatialFrame[0];
  traj[4] = positionInSpatialFrame[1];
  traj[5] = positionInSpatialFrame[2];
}





int main(int argc, char* argv[])
{
  try
  {


    // ================= Creation of the model =======================

    // User-defined main parameters
    unsigned int nDof = 3;
    unsigned int qDim = 7;
    unsigned int nDim = 6;
    double t0 = 0;                   // initial computation time
    double T = 10.0;                  // final computation time
    double h = 0.001;                // time step
    int N = 1000;
    double L1 = 1.0;
    //double L2 = 1.0;
    double L3 = 1.0;
    //double theta = 1.0;              // theta for MoreauJeanOSI integrator
    double g = 9.81; // Gravity
    double m = 1.;

    // -------------------------
    // --- Dynamical systems ---
    // -------------------------

    FILE * pFile;
    pFile = fopen("data.h", "w");
    if (pFile == NULL)
    {
      printf("fopen exampleopen filed!\n");
      fclose(pFile);
    }


    cout << "====> Model loading ..." << endl << endl;

    // -- Initial positions and velocities --
    SP::SiconosVector q03(new SiconosVector(qDim));
    SP::SiconosVector v03(new SiconosVector(nDim));
    SP::SimpleMatrix I3(new SimpleMatrix(3, 3));
    v03->zero();
    I3->eye();
    I3->setValue(0, 0, 0.1);
    q03->zero();
    (*q03)(2) = -L1 * sqrt(2.0) - L1 / 2;

    double angle = M_PI / 2;
    SiconosVector V1(3);
    V1.zero();
    V1.setValue(0, 0);
    V1.setValue(1, 1);
    V1.setValue(2, 0);
    q03->setValue(3, cos(angle / 2));
    q03->setValue(4, V1.getValue(0)*sin(angle / 2));
    q03->setValue(5, V1.getValue(1)*sin(angle / 2));
    q03->setValue(6, V1.getValue(2)*sin(angle / 2));

    SP::NewtonEulerDS bouncingbeam(new NewtonEulerDS(q03, v03, m, I3));
    // -- Set external forces (weight) --
    SP::SiconosVector weight3(new SiconosVector(nDof));
    (*weight3)(2) = -m * g;
    bouncingbeam->setFExtPtr(weight3);

    // --------------------
    // --- Interactions ---
    // --------------------

    // Interaction with the floor
    double e = 0.1;
    SP::SimpleMatrix H(new SimpleMatrix(1, qDim));
    SP::SiconosVector eR(new SiconosVector(1));
    eR->setValue(0, 2.0);
    H->zero();
    (*H)(0, 2) = 1.0;
    SP::NonSmoothLaw nslaw0(new NewtonImpactNSL(e));
    SP::NewtonEulerR relation0(new NewtonEulerR());
    relation0->setJachq(H);
    relation0->setE(eR);
    cout << "main jacQH" << endl;
    relation0->jachq()->display();


    // Interactions
    // Building the prismatic joint for bouncingbeam
    // input  - the first concerned DS : bouncingbeam
    //        - an axis in the spatial frame (absolute frame)
    // SP::SimpleMatrix H4(new SimpleMatrix(PrismaticJointR::numberOfConstraints(), qDim));
    // H4->zero();

    SP::NonSmoothLaw nslaw4(new EqualityConditionNSL(PrismaticJointR::numberOfConstraints()));
    SP::SiconosVector axe1(new SiconosVector(3));
    axe1->zero();
    axe1->setValue(2, 1);
    
    SP::NewtonEulerR relation4(new PrismaticJointR(bouncingbeam, axe1));
    SP::Interaction inter4(new Interaction(PrismaticJointR::numberOfConstraints(), nslaw4, relation4));
    SP::Interaction interFloor(new Interaction(1, nslaw0, relation0));

    // -------------
    // --- Model ---
    // -------------
    SP::Model myModel(new Model(t0, T));
    // add the dynamical system in the non smooth dynamical system
    myModel->nonSmoothDynamicalSystem()->insertDynamicalSystem(bouncingbeam);
    // link the interaction and the dynamical system
    myModel->nonSmoothDynamicalSystem()->link(inter4, bouncingbeam);
    myModel->nonSmoothDynamicalSystem()->link(interFloor, bouncingbeam);
    // ------------------
    // --- Simulation ---
    // ------------------

    // -- (1) OneStepIntegrators --

    SP::D1MinusLinearOSI OSI3(new D1MinusLinearOSI(bouncingbeam));


    // -- (2) Time discretisation --
    SP::TimeDiscretisation t(new TimeDiscretisation(t0, h));

    // -- (3) one step non smooth problem
    
    SP::OneStepNSProblem impact(new MLCP());
    SP::OneStepNSProblem force(new MLCP());

    // -- (4) Simulation setup with (1) (2) (3)
    SP::TimeSteppingD1Minus s(new TimeSteppingD1Minus(t, 2));
    s->insertIntegrator(OSI3);
    s->insertNonSmoothProblem(impact, SICONOS_OSNSP_TS_VELOCITY);
    s->insertNonSmoothProblem(force, SICONOS_OSNSP_TS_VELOCITY + 1);
    //    s->setComputeResiduY(true);
    //  s->setUseRelativeConvergenceCriteron(false);



    // =========================== End of model definition ===========================

    // ================================= Computation =================================

    // --- Simulation initialization ---

    cout << "====> Initialisation ..." << endl << endl;
    myModel->initialize(s);


    // --- Get the values to be plotted ---
    // -> saved in a matrix dataPlot
    unsigned int outputSize = 15 + 7;
    SimpleMatrix dataPlot(N, outputSize);
    SimpleMatrix bouncingbeamPlot(2,3*N);

    SP::SiconosVector q3 = bouncingbeam->q();
    SP::SiconosVector y= interFloor->y(0);
    SP::SiconosVector ydot= interFloor->y(1);
    SP::SiconosVector lambda= interFloor->lambda(2);
    SP::SiconosVector lambda1= interFloor->lambda(1);
    std::cout << "computeH0\n";
    relation0->computeh(0., *interFloor);
    //    std::cout<<"computeH4\n";
    //    relation4->computeh(0.);

    // --- Time loop ---
    cout << "====> Start computation ... " << endl << endl;
    // ==== Simulation loop - Writing without explicit event handling =====
    int k = 0;
    boost::progress_display show_progress(N);

    boost::timer time;
    time.restart();
    SP::SiconosVector yAux(new SiconosVector(3));
    yAux->setValue(0, 1);
    SP::SimpleMatrix Jaux(new SimpleMatrix(3, 3));
    Index dimIndex(2);
    Index startIndex(4);
    fprintf(pFile, "double T[%d*%d]={", N + 1, outputSize);
    double beamTipTrajectories[6];
    
    for (k = 0; k < N; k++)
    {
      // solve ...
      //s->newtonSolve(1e-4, 50);
      s->advanceToEvent();


      // --- Get values to be plotted ---
      dataPlot(k, 0) =  s->nextTime();
      
      dataPlot(k, 1) = (*q3)(0);
      dataPlot(k, 2) = (*q3)(1);
      dataPlot(k, 3) = (*q3)(2);
      dataPlot(k, 4) = (*q3)(3);
      dataPlot(k, 5) = (*q3)(4);
      dataPlot(k, 6) = (*q3)(5);
      dataPlot(k, 7) = (*q3)(6);

      dataPlot(k, 8) = y->getValue(0);
      dataPlot(k, 9) = ydot->getValue(0);
      dataPlot(k, 10) = (*lambda)(0);
      dataPlot(k, 11) = (*lambda1)(0);



      tipTrajectories(q3,beamTipTrajectories,L3);
      bouncingbeamPlot(0,3*k) = beamTipTrajectories[0];
      bouncingbeamPlot(0,3*k+1) = beamTipTrajectories[1];
      bouncingbeamPlot(0,3*k+2) = beamTipTrajectories[2];
      bouncingbeamPlot(1,3*k) = beamTipTrajectories[3];
      bouncingbeamPlot(1,3*k+1) = beamTipTrajectories[4];
      bouncingbeamPlot(1,3*k+2) = beamTipTrajectories[5];
      
      //printf("reaction1:%lf \n", interFloor->lambda(1)->getValue(0));

      for (unsigned int jj = 0; jj < outputSize; jj++)
      {
        if ((k || jj))
          fprintf(pFile, ",");
        fprintf(pFile, "%f", dataPlot(k, jj));
      }
      fprintf(pFile, "\n");
      //s->nextStep(); 
      s->processEvents();
      ++show_progress;
    }
    fprintf(pFile, "};");
    cout << endl << "End of computation - Number of iterations done: " << k - 1 << endl;
    cout << "Computation Time " << time.elapsed()  << endl;

    // --- Output files ---
    cout << "====> Output file writing ..." << endl;
    ioMatrix::write("NE_BouncingBeam_D1MinusLinearOSI.dat", "ascii", dataPlot, "noDim");
    ioMatrix::write("NE_BouncingBeam_D1MinusLinearOSI_beam.dat", "ascii", bouncingbeamPlot, "noDim");

    // SimpleMatrix dataPlotRef(dataPlot);
    // dataPlotRef.zero();
    // ioMatrix::read("NE_BoundingBeam.ref", "ascii", dataPlotRef);
    // if ((dataPlot - dataPlotRef).normInf() > 1e-7)
    // {
    //   (dataPlot - dataPlotRef).display();
    //   std::cout << "Warning. The results is rather different from the reference file." << std::endl;
    //   return 1;
    // }

    fclose(pFile);
  }

  catch (SiconosException e)
  {
    cout << e.report() << endl;
  }
  catch (...)
  {
    cout << "Exception caught in NE_...cpp" << endl;
  }

}
