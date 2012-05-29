/* Siconos-sample , Copyright INRIA 2005-2012.
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

/* !\file SMCExampleImplicitOT2.cpp
  \brief Two independent systems of dimension one controlled to slide
  on \f$x = 0\f$. An implicit scheme is used with a disturbance prediction
  O. Huber
  */

#include "SiconosKernel.hpp"
using namespace std;

// main program
int main(int argc, char* argv[])
{
  // User-defined parameters
  unsigned int ndof = 2;          // Number of degrees of freedom of your system
  double t0 = 0.0;                // Starting time
  double T = 1;                   // Total simulation time
  double h = 1.0e-4;              // Time step for simulation
  double hControl = 1.0e-2;       // Time step for control
  double Xinit = 1.0;
  double theta = 0.5;

  if (h > hControl)
  {
    RuntimeException::selfThrow("hControl must be bigger than h");
  }

  // ================= Creation of the model =======================
  // Steps:
  // - create a Dynamical System
  // - add a Simulation to the model

  // -------------------------
  // --- Dynamical systems ---
  // -------------------------

  // First System:
  // dx/dt = Ax + u(t) + r
  // x(0) = x0
  // Note: r = Blambda, B defines in relation below.

  // Matrix declaration
  SP::SiconosMatrix A(new SimpleMatrix(ndof, ndof, 0));
  SP::SiconosVector x0(new SimpleVector(ndof));
  (*x0)(0) = Xinit;
  (*x0)(1) = -Xinit;
  SP::SimpleMatrix sensorC(new SimpleMatrix(2, 2));
  sensorC->eye();
  SP::SimpleMatrix sensorD(new SimpleMatrix(2, 2, 0));
  SP::SimpleMatrix Csurface(new SimpleMatrix(1, 2, 0));
  (*Csurface)(0, 1) = 1;

  // Dynamical Systems
  SP::FirstOrderLinearDS processDS(new FirstOrderLinearDS(x0, A));
  processDS->setComputebFunction("RelayPlugin.so", "computeB");

  // -------------
  // --- Model process ---
  // -------------
  SP::Model process(new Model(t0, T));
  process->nonSmoothDynamicalSystem()->insertDynamicalSystem(processDS);

  // ------------------
  // --- Simulation ---
  // ------------------
  // TimeDiscretisation
  SP::TimeDiscretisation processTD(new TimeDiscretisation(t0, h));
  SP::TimeDiscretisation tSensor(new TimeDiscretisation(t0, hControl));
  SP::TimeDiscretisation tActuator(new TimeDiscretisation(t0, hControl));
  // == Creation of the Simulation ==
  SP::TimeStepping processSimulation(new TimeStepping(processTD, 0));
  processSimulation->setName("plant simulation");
  // -- OneStepIntegrators --
  SP::Moreau processIntegrator(new Moreau(processDS, theta));
  processSimulation->insertIntegrator(processIntegrator);

  // Control stuff
  SP::ControlManager control(new ControlManager(process));
  // use a controlSensor
  SP::LinearSensor sens(new LinearSensor(tSensor, processDS, sensorC, sensorD));
  control->addSensorPtr(sens);
  // add the sliding mode controller
  SP::LinearSMCOT2 act = static_pointer_cast<LinearSMCOT2>(control->addActuator(LINEAR_SMC_OT2, tActuator));
  act->setCsurfacePtr(Csurface);
  act->addSensorPtr(sens);

  // =========================== End of model definition ===========================

  // ================================= Computation =================================

  // --- Simulation initialization ---

  cout << "====> Simulation initialisation ..." << endl << endl;
  // initialise the process and the ControlManager
  process->initialize(processSimulation);
  control->initialize();

  // --- Get the values to be plotted ---
  unsigned int outputSize = 5; // number of required data
  unsigned int N = ceil((T - t0) / h) + 10; // Number of time steps

  SP::SiconosVector xProc = processDS->x();
  SP::SiconosVector uProc = processDS->z();
  // Save data in a matrix dataPlot
  SimpleMatrix dataPlot(N, outputSize);
  dataPlot(0, 0) = process->t0(); // Initial time of the model
  dataPlot(0, 1) = (*xProc)(0);
  dataPlot(0, 2) = (*xProc)(1);
  dataPlot(0, 3) = (*uProc)(0);
  dataPlot(0, 4) = (*uProc)(1);

  SP::EventsManager eventsManager = processSimulation->eventsManager();

  // ==== Simulation loop =====
  cout << "====> Start computation ... " << endl << endl;
  int k = 0; // Current step
  // Simulation loop
  boost::progress_display show_progress(N);
  boost::timer time;
  time.restart();
  SP::Event nextEvent;
  while (processSimulation->nextTime() < T)
  {
    processSimulation->computeOneStep();
    nextEvent = eventsManager->followingEvent(eventsManager->currentEvent());
    if (nextEvent->getType() == TD_EVENT)
    {
      k++;
      dataPlot(k, 0) = processSimulation->nextTime();
      dataPlot(k, 1) = (*xProc)(0);
      dataPlot(k, 2) = (*xProc)(1);
      dataPlot(k, 3) = (*uProc)(0);
      dataPlot(k, 4) = (*uProc)(1);
      ++show_progress;
    }
    processSimulation->nextStep();
  }
  cout << endl << "Computation Time " << time.elapsed()  << endl;

  // --- Output files ---
  cout << "====> Output file writing ..." << endl;
  ioMatrix io("SMCExampleImplicitOT2.dat", "ascii");
  dataPlot.resize(k, outputSize);
  io.write(dataPlot, "noDim");

  // Comparison with a reference file
  SimpleMatrix dataPlotRef(dataPlot);
  dataPlotRef.zero();
  ioMatrix ref("SMCExampleImplicitOT2.ref", "ascii");
  ref.read(dataPlotRef);
  std::cout << (dataPlot - dataPlotRef).normInf() << std::endl;

  if ((dataPlot - dataPlotRef).normInf() > 1e-12)
  {
    std::cout << "Warning. The results is rather different from the reference file." << std::endl;
    return 1;
  }

}