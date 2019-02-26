*
      SUBROUTINE GMRES( N, B, X, RESTRT, WORK, LDW, WORK2, LDW2, 
     $                  ITER, RESID, MATVEC, PSOLVE, INFO )
*
*  -- Iterative template routine --
*     Univ. of Tennessee and Oak Ridge National Laboratory
*     October 1, 1993
*     Details of this algorithm are described in "Templates for the
*     Solution of Linear Systems: Building Blocks for Iterative
*     Methods", Barrett, Berry, Chan, Demmel, Donato, Dongarra,
*     EiITERkhout, Pozo, Romine, and van der Vorst, SIAM Publications,
*     1993. (ftp netlib2.cs.utk.edu; cd linalg; get templates.ps).
*
*     .. Scalar Arguments ..
      INTEGER            N, RESTRT, LDW, LDW2, ITER, INFO
      DOUBLE PRECISION   RESID
*     ..
*     .. Array Arguments ..
      DOUBLE PRECISION   B( * ), X( * ), WORK( * ), WORK2( * )
*     ..
*     .. Function Arguments ..
*
      EXTERNAL           MATVEC, PSOLVE
*
*  Purpose
*  =======
*
*  GMRES solves the linear system Ax = b using the
*  Generalized Minimal Residual iterative method with preconditioning.
*
*  Convergence test: ( norm( b - A*x ) / norm( b ) ) < TOL.
*  For other measures, see the above reference.
*
*  Arguments
*  =========
*
*  N       (input) INTEGER. 
*          On entry, the dimension of the matrix.
*          Unchanged on exit.
* 
*  B       (input) DOUBLE PRECISION array, dimension N.
*          On entry, right hand side vector B.
*          Unchanged on exit.
*
*  X       (input/output) DOUBLE PRECISION array, dimension N.
*          On input, the initial guess; on exit, the iterated solution.
*
*  RESTRT  (input) INTEGER
*          Restart parameter, <= N. This parameter controls the amount
*          of memory required for matrix WORK2.
*
*  WORK    (workspace) DOUBLE PRECISION array, dimension (LDW,5).
*          Note that if the initial guess is the zero vector, then 
*          storing the initial residual is not necessary.
*
*  LDW     (input) INTEGER
*          The leading dimension of the array WORK. LDW >= max(1,N).
*
*  WORK2   (workspace) DOUBLE PRECISION array, dimension (LDW2,2*RESTRT+2).
*          This workspace is used for constructing and storing the
*          upper Hessenberg matrix. The two extra columns are used to
*          store the Givens rotation matrices.
*
*  LDW2    (input) INTEGER
*          The leading dimension of the array WORK2.
*          LDW2 >= max(1,RESTRT).
*
*  ITER    (input/output) INTEGER
*          On input, the maximum iterations to be performed.
*          On output, actual number of iterations performed.
*
*  RESID   (input/output) DOUBLE PRECISION
*          On input, the allowable error tolerance.
*          On ouput, the norm of the residual vector if solution
*          approximated to tolerance, otherwise reset to input
*          tolerance.
*
*  INFO    (output) INTEGER
*          =  0:  successful exit
*          =  1:  maximum number of iterations performed;
*                 convergence not achieved.
*
*  MATVEC  (external subroutine)
*          The user must provide a subroutine to perform the
*          matrix-vector product A*x = y.
*          Vector x must remain unchanged. The solution is
*          over-written on vector y.
*
*          The call is:
*
*             CALL MATVEC( X, Y )
*
*  ============================================================
*     ..
*     .. Parameters ..
      INTEGER            OFSET
      PARAMETER        ( OFSET = 1000 )
*
*This variable used to communicate requests between GMRES & GMRESREVCOM
*GMRES -> GMRESREVCOM: 1 = init, 
*                      2 = use saved state to resume flow.
*GMRESREVCOM -> GMRES: -1 = done, return to main, 
*                       1 = matvec using SCLR1/2, NDX1/2 
*                       2 = solve using NDX1/2
*                       3 = matvec using WORK2 NDX1 & WORK NDX2
      INTEGER          IJOB
      LOGICAL          FTFLG
*
*     Arg/Result indices into WORK[].
      INTEGER NDX1, NDX2
*     Scalars passed from GMRESREVCOM to GMRES
      DOUBLE PRECISION SCLR1, SCLR2
*     Vars reqd for STOPTEST2
      DOUBLE PRECISION TOL, BNRM2
*     ..
*     .. External subroutines ..
      EXTERNAL         GMRESREVCOM, STOPTEST2
*     ..
*     .. Executable Statements ..
*
      INFO = 0
*
*     Test the input parameters.
*
      IF ( N.LT.0 ) THEN
         INFO = -1
      ELSE IF ( LDW.LT.MAX( 1, N ) ) THEN
         INFO = -2
      ELSE IF ( ITER.LE.0 ) THEN
         INFO = -3
      ELSE IF ( LDW2.LT.RESTRT ) THEN
         INFO = -4
      ENDIF
      IF ( INFO.NE.0 ) RETURN
*
*     Stop test may need some indexing info from REVCOM
*     use the init call to send the request across. REVCOM
*     will note these requests, and everytime it asks for
*     stop test to be done, it will provide the indexing info.
*     Note: V, and GIV contain # of history vectors each.
*     To access the i'th vector in V, add i to V*OFSET 1<=i<=RESTRT
*     To access the i'th vector in GIV, add i to GIV*OFSET 1<=i<=RESTRT
*
*     1 == R; 2 == S; 3 == W; 4 == Y; 5 == AV; 6 == H; 7*OFSET+i == V;
*     8*OFSET+i == GIV; -1 == ignore; any other == error
      NDX1 = 1
      NDX2 = -1
      TOL = RESID
      FTFLG = .TRUE.
*
*     First time call always init.
*
      IJOB = 1

 1    CONTINUE

          CALL GMRESREVCOM(N, B, X, RESTRT, WORK, LDW, WORK2, LDW2, 
     $                     ITER, RESID, INFO, NDX1, NDX2, SCLR1, 
     $                     SCLR2, IJOB)


*         On a return from REVCOM() we use the table
*         to decode IJOB.
          IF (IJOB .eq. -1) THEN
*           revcom wants to terminate, so do it.
            GOTO 2
          ELSEIF (IJOB .eq. 1) THEN
*           call matvec directly with X.
            CALL MATVEC(SCLR1, X, SCLR2, WORK(NDX2))
          ELSEIF (IJOB .eq. 2) THEN
*           call solve.
            CALL PSOLVE(WORK(NDX1), WORK(NDX2))
          ELSEIF (IJOB .eq. 3) THEN
*           call matvec.
            CALL MATVEC(SCLR1, WORK(NDX1), SCLR2, WORK(NDX2))
         ELSEIF (IJOB .EQ. 4) THEN
*           do stopping test 2
*           if first time, set INFO so that BNRM2 is computed.
            IF( FTFLG ) INFO = -1
            CALL STOPTEST2(N, WORK(NDX1), B, BNRM2, RESID, TOL, INFO)
            FTFLG = .FALSE.
         ENDIF
*
*         done what revcom asked, set IJOB & go back to it.
          IJOB = 2
          GOTO 1
*
*     come here to terminate
 2    CONTINUE
*
      RETURN
*
*     End of GMRES.f
*
      END
*
