#ifndef ADJOINTINPUT_H
#define ADJOINTINPUT_H

#include "SiconosKernel.hpp"

class adjointInput : public FirstOrderType2R
{
protected:
  SimpleMatrix   * K2 ;

public:
  adjointInput();
  virtual ~adjointInput() {};


  virtual void initialize(Interaction& inter);


  /** default function to compute h
   *  \param double : current time
   */
  virtual void computeh(double time, Interaction& inter) ;

  /** default function to compute g
   *  \param double time, Interaction& inter : current time
   */
  virtual void computeg(double time, Interaction& inter) ;

  /** default function to compute jacobianH
   *  \param double time, Interaction& inter : current time
   *  \param index for jacobian (0: jacobian according to x, 1 according to lambda)
   */
  virtual void computeJachx(double time, Interaction& inter);
  virtual void computeJachlambda(double time, Interaction& inter);

  /** default function to compute jacobianG according to lambda
   *  \param double time, Interaction& inter : current time
   *  \param index for jacobian: at the time only one possible jacobian => i = 0 is the default value .
   */
  virtual void computeJacgx(double time, Interaction& inter);
  virtual void computeJacglambda(double time, Interaction& inter);


  double source(double t);

  void beta(double t, SiconosVector& xvalue, SP::SiconosVector alpha);

  void JacobianXbeta(double t, SiconosVector& xvalue, SP::SiconosMatrix JacbetaX);

};

TYPEDEF_SPTR(adjointInput);

#endif