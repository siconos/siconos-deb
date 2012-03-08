/* Siconos-Kernel, Copyright INRIA 2005-2011.
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

/*! \file CommonSMC.hpp
  \brief General interface to define a sliding mode actuator
  */

#ifndef CommonSMC_H
#define CommonSMC_H

#include "SimulationTools.hpp"
#include "ModelingTools.hpp"
#include "Actuator.hpp"
#include <boost/circular_buffer.hpp>

#ifndef ControlSensor_H
DEFINE_SPTR(ControlSensor)
#endif

class CommonSMC : public Actuator
{
private:
  /** serialization hooks */
  ACCEPT_SERIALIZATION(CommonSMC);

protected:
  /** default constructor */
  CommonSMC() {};

  /** codimension size */
  unsigned int _sDim;

  /** index for saving data */
  unsigned int _indx;

  /** control variable */
  SP::SimpleVector _u;

  /** the vector defining the surface (\f$ s = Cx \f$) */
  SP::SiconosMatrix _Csurface;

  /** the sensor that feed the controller */
  SP::ControlSensor _sensor;

  /** boolean to determined if the controller has been correctly initialized */
  bool _initDone;

  /** current \f$\Delta t\f$ (or timeStep) */
  double _curDeltaT;

  /** matrix describing the relation between the control value and sgn(s) */
  SP::SimpleMatrix _B;

  /** matrix describing the influence of \f$lambda\f$ on s */
  SP::SimpleMatrix _D;

  /** the Relation for the Controller */
  SP::FirstOrderLinearR _relationSMC;

  /** the NonSmoothLaw for the controller */
  SP::NonSmoothLaw _sign;

  /** */
  SP::Interaction _interactionSMC;

  /** easy access to lambda */
  SP::SiconosVector _lambda;

  /** easy access to the state */
  SP::SiconosVector _xController;

public:

  /** Constructor with a TimeDiscretisation and a Model.
   * \param name the type of the SMC Actuator
   * \param t the SP::TimeDiscretisation (/!\ it should not be used elsewhere !)
   * \param ds the SP::DynamicalSystem we are controlling
   */
  CommonSMC(int name, SP::TimeDiscretisation t, SP::DynamicalSystem ds): Actuator(name, t, ds) {}

  /** Constructor with a TimeDiscretisation, a Model and a set of Sensor.
   * \param name the type of the SMC Actuator
   * \param t the SP::TimeDiscretisation (/!\ it should not be used elsewhere !)
   * \param ds the SP::DynamicalSystem we are controlling
   * \param sensorList the set of Sensor linked to this Actuator.
   */
  CommonSMC(int name, SP::TimeDiscretisation t, SP::DynamicalSystem ds, const Sensors& sensorList): Actuator(name, t, ds, sensorList) {}

  /** Compute the new control law at each event
   */
  virtual void actuate() = 0;

  /** Initialization
   * \param m a SP::Model
   */
  virtual void initialize(SP::Model m);

  /** Set the value of _Csurface to newValue
    * * \param newValue the new value for _Csurface
    */
  void setCsurface(const SiconosMatrix& newValue);

  /** Set _Csurface to pointer newPtr
   * \param newPtr a SP::SiconosMatrix containing the new value for _Csurface
   */
  void setCsurfacePtr(SP::SiconosMatrix newPtr);

};
DEFINE_SPTR(CommonSMC)
#endif