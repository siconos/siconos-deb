
/* Siconos-sample version 3.0.0, Copyright INRIA 2005-2008.
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

using namespace std;
int main(int argc, char* argv[])
{

  double t0 = 0.0;
  double T = 5.0e-3;        // Total simulation time
  double h_step = 1.0e-6;  // Time step
  double Lvalue = 1e-2;   // inductance
  double Cvalue = 1e-6;   // capacitance
  double Rvalue = 1e3;    // resistance
  double Vinit = 10.0;    // initial voltage
  string Modeltitle = "DiodeBridge";

  boost::timer time;
  time.restart();
  try
  {
    // --- Dynamical system specification ---
    SP::SimpleVector init_state(new SimpleVector(2));
    init_state->setValue(0, Vinit);

    SP::SimpleMatrix LS_A(new SimpleMatrix(2, 2));
    LS_A->setValue(0, 1, -1.0 / Cvalue);
    LS_A->setValue(1, 0, 1.0 / Lvalue);

    SP::FirstOrderLinearDS LSDiodeBridge(new FirstOrderLinearDS(init_state, LS_A));

    // --- Interaction between linear system and non smooth system ---
    SP::SiconosMatrix Int_C(new SimpleMatrix(4, 2));
    (*Int_C)(2, 0) = -1.0;
    (*Int_C)(3, 0) = 1.0;

    SP::SiconosMatrix Int_D(new SimpleMatrix(4, 4));
    (*Int_D)(0, 0) = 1.0 / Rvalue;
    (*Int_D)(0, 1) = 1.0 / Rvalue;
    (*Int_D)(0, 2) = -1.0;
    (*Int_D)(1, 0) = 1.0 / Rvalue;
    (*Int_D)(1, 1) = 1.0 / Rvalue;
    (*Int_D)(1, 3) = -1.0;
    (*Int_D)(2, 0) = 1.0;
    (*Int_D)(3, 1) = 1.0;

    SP::SiconosMatrix Int_B(new SimpleMatrix(2, 4));
    (*Int_B)(0, 2) = -1.0 / Cvalue ;
    (*Int_B)(0, 3) = 1.0 / Cvalue;

    SP::FirstOrderLinearTIR LTIRDiodeBridge(new FirstOrderLinearTIR(*Int_C, *Int_B));
    LTIRDiodeBridge->setDPtr(Int_D);

    SP::NonSmoothLaw nslaw(new ComplementarityConditionNSL(4));

    SP::Interaction InterDiodeBridge(new Interaction(4, nslaw, LTIRDiodeBridge, 1));
    InterDiodeBridge->insert(LSDiodeBridge);

    // --- Model creation ---
    SP::Model DiodeBridge(new Model(t0, T, Modeltitle));
    // add the dynamical system in the non smooth dynamical system
    DiodeBridge->nonSmoothDynamicalSystem()->insertDynamicalSystem(LSDiodeBridge);
    // link the interaction and the dynamical system
    DiodeBridge->nonSmoothDynamicalSystem()->link(InterDiodeBridge, LSDiodeBridge);

    // ------------------
    // --- Simulation ---
    // ------------------


    // -- (1) OneStepIntegrators --
    double theta = 0.5;
    double gamma = 0.5;

    SP::Moreau aOSI(new Moreau(LSDiodeBridge, theta, gamma));
    aOSI->setUseGammaForRelation(true);
    // -- (2) Time discretisation --
    SP::TimeDiscretisation aTiDisc(new TimeDiscretisation(t0, h_step));

    // -- (3) Non smooth problem
    SP::LCP aLCP(new LCP());

    // -- (4) Simulation setup with (1) (2) (3)
    SP::TimeStepping aTS(new TimeStepping(aTiDisc, aOSI, aLCP));

    // Initialization
    cout << "====> Initialisation ..." << endl << endl;
    DiodeBridge->initialize(aTS);
    cout << " ---> End of initialization." << endl;

    int k = 0;
    double h = aTS->timeStep();
    int N = (int)((T - t0) / h); // Number of time steps

    // --- Get the values to be plotted ---
    // -> saved in a matrix dataPlot
    SimpleMatrix dataPlot(N, 10);

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

    SP::SimpleMatrix LS_P(new SimpleMatrix(2, 2));
    LS_P->setValue(0, 0, Cvalue);
    LS_P->setValue(1, 1, Lvalue);

    SP::SimpleMatrix LS_Q(new SimpleMatrix(6, 6));
    double coef = 2.0 / Rvalue;
    LS_Q->setValue(2, 2, coef);
    LS_Q->setValue(2, 3, coef);
    LS_Q->setValue(3, 2, coef);
    LS_Q->setValue(3, 3, coef);

    SP::SiconosVector xlambda(new BlockVector(x, lambda));



    SP::SiconosVector tmp6(new SimpleVector(6));
    SP::SiconosVector tmp6bis(new SimpleVector(6));

    SP::SiconosVector tmp(new SimpleVector(2));


    prod(1 / 2.0, *LS_P, *x, *tmp, true);

    dataPlot(k, 7) = inner_prod(*x, *tmp);


    dataPlot(k, 8) = 0.0;

    dataPlot(k, 9) =  dataPlot(k, 7) +  dataPlot(k, 8);


    boost::timer t;
    t.restart();

    // --- Time loop  ---
    for (k = 1 ; k < N ; ++k)
    {
      // solve ...
      aTS->computeOneStep();
      //aLCP->display();
      //   char  sz[0];
      //   cin >> sz;
      // --- Get values to be plotted ---
      // time
      dataPlot(k, 0) = aTS->nextTime();

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


      prod(1 / 2.0, *LS_P, *x, *tmp, true);
      dataPlot(k, 7) = inner_prod(*x, *tmp);


      prod(1 / 2.0 * h, *LS_Q, *xlambda, *tmp6, true);
      *tmp6bis = *xlambda;
      dataPlot(k, 8) = inner_prod(*tmp6bis, *tmp6) +  dataPlot(k - 1, 8) ;

      dataPlot(k, 9) =  dataPlot(k, 7) +  dataPlot(k, 8);

      aTS->nextStep();

    }


    // --- elapsed time computing ---
    cout << "time = " << t.elapsed() << endl;

    // Number of time iterations
    cout << "Number of iterations done: " << k << endl;

    // dataPlot (ascii) output
    ioMatrix io("DiodeBridge.dat", "ascii");
    dataPlot.resize(k, 10);
    io.write(dataPlot, "noDim");
    // Comparison with a reference file
    SimpleMatrix dataPlotRef(dataPlot);
    dataPlotRef.zero();

    ioMatrix ref("DiodeBridge.ref", "ascii");
    ref.read(dataPlotRef);

    if ((dataPlot - dataPlotRef).normInf() > 1e-10)
    {
      std::cout << "Warning. The results is rather different from the reference file." << std::endl;
      return 1;
    }


  }

  // --- Exceptions handling ---
  catch (SiconosException e)
  {
    cout << e.report() << endl;
  }
  catch (...)
  {
    cout << "Exception caught " << endl;
  }
  cout << "Computation Time: " << time.elapsed()  << endl;
}