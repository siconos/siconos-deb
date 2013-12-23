#!/usr/bin/env python

from Siconos.Kernel import SiconosVector

v1 = SiconosVector([1, 2, 3])

v2 = SiconosVector()

def test_serialization1():

    v2.xml_import(v1.xml_export())

    assert(v2.getValue(0) == 1)
    assert(v2.getValue(1) == 2)
    assert(v2.getValue(2) == 3)

def test_serialization2():

    v2.text_import(v1.text_export())

    assert(v2.getValue(0) == 1)
    assert(v2.getValue(1) == 2)
    assert(v2.getValue(2) == 3)


def test_serialization2():

    v2.binary_import(v1.binary_export())

    assert(v2.getValue(0) == 1)
    assert(v2.getValue(1) == 2)
    assert(v2.getValue(2) == 3)    

def test_serialization3():
    from Siconos.Kernel import LagrangianLinearTIDS, NewtonImpactNSL, \
        LagrangianLinearTIR, Interaction, Model, Moreau, TimeDiscretisation, LCP, TimeStepping

    from numpy import array, eye, empty

    t0 = 0       # start time
    T = 10       # end time
    h = 0.005    # time step
    r = 0.1      # ball radius
    g = 9.81     # gravity
    m = 1        # ball mass
    e = 0.9      # restitution coeficient
    theta = 0.5  # theta scheme

    #
    # dynamical system
    #
    x = array([1, 0, 0])  # initial position
    v = array([0, 0, 0])  # initial velocity
    mass = eye(3)         # mass matrix
    mass[2, 2] = 3./5 * r * r

    # the dynamical system
    ball = LagrangianLinearTIDS(x, v, mass)

    # set external forces
    weight = array([-m * g, 0, 0])
    ball.setFExtPtr(weight)

    #
    # Interactions
    #

    # ball-floor
    H = array([[1, 0, 0]])

    nslaw = NewtonImpactNSL(e)
    relation = LagrangianLinearTIR(H)
    inter = Interaction(1, nslaw, relation)

    #
    # Model
    #
    first_bouncingBall = Model(t0, T)

    # add the dynamical system to the non smooth dynamical system
    first_bouncingBall.nonSmoothDynamicalSystem().insertDynamicalSystem(ball)

    # link the interaction and the dynamical system
    first_bouncingBall.nonSmoothDynamicalSystem().link(inter, ball)

    #
    # Simulation
    #

    # (1) OneStepIntegrators
    OSI = Moreau(theta)
    OSI.insertDynamicalSystem(ball)

    # (2) Time discretisation --
    t = TimeDiscretisation(t0, h)

    # (3) one step non smooth problem
    osnspb = LCP()

    # (4) Simulation setup with (1) (2) (3)
    s = TimeStepping(t)
    s.insertIntegrator(OSI)
    s.insertNonSmoothProblem(osnspb)

    # end of model definition

    #
    # computation
    #

    # simulation initialization
    first_bouncingBall.initialize(s)

    #
    # save and load data from xml and .dat
    #
    from Siconos.IO import save, load
    save(first_bouncingBall, "bouncingBall.xml")

    bouncingBall = load("bouncingBall.xml")

    # the number of time steps
    N = (T-t0)/h+1

    # Get the values to be plotted
    # ->saved in a matrix dataPlot

    dataPlot = empty((N, 5))

    #
    # numpy pointers on dense Siconos vectors
    #
    q = ball.q()
    v = ball.velocity()
    p = ball.p(1)
    lambda_ = inter.lambda_(1)

    #
    # initial data
    #
    dataPlot[0, 0] = t0
    dataPlot[0, 1] = q[0]
    dataPlot[0, 2] = v[0]
    dataPlot[0, 3] = p[0]
    dataPlot[0, 4] = lambda_[0]

    k = 1

    # time loop
    while(s.hasNextEvent()):
        s.computeOneStep()

        dataPlot[k, 0] = s.nextTime()
        dataPlot[k, 1] = q[0]
        dataPlot[k, 2] = v[0]
        dataPlot[k, 3] = p[0]
        dataPlot[k, 4] = lambda_[0]

        k += 1
        print(s.nextTime())
        s.nextStep()

    #
    # comparison with the reference file
    #
    from Siconos.Kernel import SimpleMatrix, getMatrix
    from numpy.linalg import norm

    ref = getMatrix(SimpleMatrix("result.ref"))

    assert (norm(dataPlot - ref) < 1e-12)
