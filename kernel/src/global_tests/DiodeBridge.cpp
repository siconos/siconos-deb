
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
//-----------------------------------------------------------------------
//
//  DiodeBridge  : sample of an electrical circuit involving :
//  - a linear dynamical system consisting of an LC oscillator (1 �F , 10 mH)
//  - a non smooth system (a 1000 Ohm resistor supplied through a 4 diodes bridge) in parallel
//    with the oscillator
//
//  Expected behavior :
//  The initial state (Vc = 10 V , IL = 0) of the oscillator provides an initial energy.
//  The period is 2 Pi sqrt(LC) ~ 0,628 ms.
//      The non smooth system is a full wave rectifier :
//  each phase (positive and negative) of the oscillation allows current to flow
//  through the resistor in a constant direction, resulting in an energy loss :
//  the oscillation damps.
//
//  State variables :
//  - the voltage across the capacitor (or inductor)
//  - the current through the inductor
//
//  Since there is only one dynamical system, the interaction is defined by :
//  - complementarity laws between diodes current and voltage. Depending on
//        the diode position in the bridge, y stands for the reverse voltage across the diode
//    or for the diode current (see figure in the template file)
//  - a linear time invariant relation between the state variables and
//    y and lambda (derived from Kirchhoff laws)
//
//-----------------------------------------------------------------------

#include "SiconosKernel.hpp"

static bool DiodeBridge();


bool DiodeBridge()
{
  std::cout << " **************************************" <<std::endl;
  std::cout << " ******** Start diode bridge *********" <<std::endl <<std::endl <<std::endl;

  double t0 = 0.0;
  double T = 5.0e-3;        // Total simulation time
  double h_step = 1.0e-6;  // Time step
  double Lvalue = 1e-2;   // inductance
  double Cvalue = 1e-6;   // capacitance
  double Rvalue = 1e3;    // resistance
  double Vinit = 10.0;    // initial voltage
  std::string Modeltitle = "DiodeBridge";

  bool res = false;

  try
  {
    // --- Dynamical system specification ---
    SP::SiconosVector init_state;
    init_state.reset(new SiconosVector(2));
    init_state->setValue(0, Vinit);

    SP::SimpleMatrix LS_A;
    LS_A.reset(new SimpleMatrix(2, 2));
    LS_A->setValue(0, 1, -1.0 / Cvalue);
    LS_A->setValue(1, 0, 1.0 / Lvalue);

    SP::DynamicalSystem LSDiodeBridge(new FirstOrderLinearDS(init_state, LS_A));

    // --- Interaction between linear system and non smooth system ---
    
    SP::SimpleMatrix Int_C(new SimpleMatrix(4, 2));
    (*Int_C)(2, 0) = -1.0;
    (*Int_C)(3, 0) = 1.0;

    SP::SimpleMatrix Int_D(new SimpleMatrix(4, 4));
    (*Int_D)(0, 0) = 1.0 / Rvalue;
    (*Int_D)(0, 1) = 1.0 / Rvalue;
    (*Int_D)(0, 2) = -1.0;
    (*Int_D)(1, 0) = 1.0 / Rvalue;
    (*Int_D)(1, 1) = 1.0 / Rvalue;
    (*Int_D)(1, 3) = -1.0;
    (*Int_D)(2, 0) = 1.0;
    (*Int_D)(3, 1) = 1.0;

    SP::SimpleMatrix Int_B(new SimpleMatrix(2, 4));
    (*Int_B)(0, 2) = -1.0 / Cvalue ;
    (*Int_B)(0, 3) = 1.0 / Cvalue;

    SP::FirstOrderLinearTIR LTIRDiodeBridge(new FirstOrderLinearTIR(Int_C, Int_B));
    LTIRDiodeBridge->setDPtr(Int_D);

    SP::NonSmoothLaw nslaw(new ComplementarityConditionNSL(4));
    SP::Interaction InterDiodeBridge(new Interaction(4, nslaw, LTIRDiodeBridge));
    
    // --- Model creation ---
    SP::Model DiodeBridge(new Model(t0, T, Modeltitle));

    SP::MoreauJeanOSI OSI_RLCD(new MoreauJeanOSI());
    DiodeBridge->nonSmoothDynamicalSystem()->insertDynamicalSystem(LSDiodeBridge);
    DiodeBridge->nonSmoothDynamicalSystem()->topology()->setOSI(LSDiodeBridge, OSI_RLCD);
    DiodeBridge->nonSmoothDynamicalSystem()->link(InterDiodeBridge, LSDiodeBridge);

    // --- Simulation specification---

    SP::TimeDiscretisation TiDiscRLCD(new TimeDiscretisation(t0, h_step));
    TiDiscRLCD->display();
    SP::TimeStepping StratDiodeBridge(new TimeStepping(TiDiscRLCD));


    // One Step Integrator
    StratDiodeBridge->insertIntegrator(OSI_RLCD);

    // One Step non smooth problem
    //    IntParameters iparam(5);
    //    iparam[0] = 1001; // Max number of iteration
    //    DoubleParameters dparam(5);
    //    dparam[0] = 0.0001; // Tolerance
    //    string solverName = "Lemke" ;
    //SP::NonSmoothSolver mySolver(new NonSmoothSolver(solverName,iparam,dparam));

    SP::LCP LCP_RLCD(new LCP());
    StratDiodeBridge->insertNonSmoothProblem(LCP_RLCD);

    // Initialization of the model
    DiodeBridge->initialize(StratDiodeBridge);

    int k = 0;
    // dataPlot (ascii) output
    SP::SimpleMatrix dataRef(new SimpleMatrix("refDiodeBridge.dat", true));
    int N = dataRef->size(0) ;

    // --- Get the values to be plotted ---
    // -> saved in a matrix dataPlot
    SimpleMatrix dataPlot(N, 7);

    SP::SiconosVector x = LSDiodeBridge->x();
    SP::SiconosVector y = InterDiodeBridge->y(0);
    SP::SiconosVector lambda = InterDiodeBridge->lambda(0);

    // For the initial time step:
    // time
    dataPlot(k, 0) = t0;

    // inductor voltage
    dataPlot(k, 1) = (*x)(0);

    // inductor current
    dataPlot(k, 2) = (*x)(1);

    // diode R1 current
    dataPlot(k, 3) = (*y)(0);

    // diode R1 voltage
    dataPlot(k, 4) = -(*lambda)(0);

    // diode F2 voltage
    dataPlot(k, 5) = -(*lambda)(1);

    // diode F1 current
    dataPlot(k, 6) = (*lambda)(2);

    // --- Time loop  ---
    for (k = 1 ; k < N ; ++k)
    {
      // solve ...
      StratDiodeBridge->computeOneStep();

      // --- Get values to be plotted ---
      // time
      dataPlot(k, 0) = StratDiodeBridge->nextTime();

      // inductor voltage
      dataPlot(k, 1) = (*x)(0);

      // inductor current
      dataPlot(k, 2) = (*x)(1);

      // diode R1 current
      dataPlot(k, 3) = (*y)(0);

      // diode R1 voltage
      dataPlot(k, 4) = -(*lambda)(0);

      // diode F2 voltage
      dataPlot(k, 5) = -(*lambda)(1);

      // diode F1 current
      dataPlot(k, 6) = (*lambda)(2);

      StratDiodeBridge->nextStep();

    }

    double tol = 1e-9;
    double norm = (dataPlot - (*dataRef)).normInf() ;
    std::cout <<std::endl <<std::endl ;
    if (norm < tol)
    {
      std::cout << " ******** DiodeBridge test ended with success ********" <<std::endl;
      res = true;
    }
    else
    {
      std::cout << " ******** DiodeBridge test failed, results differ from those of reference file. ********" <<std::endl;
      res = false;
    }

    std::cout <<std::endl <<std::endl;
  }

  // --- Exceptions handling ---
  catch (SiconosException e)
  {
    std::cout << e.report() <<std::endl;
  }
  catch (...)
  {
    std::cout << "Exception caught " <<std::endl;
  }

  return res;
}
