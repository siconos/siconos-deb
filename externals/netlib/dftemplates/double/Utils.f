*
*  This file contains routines used by Jacobi, SOR, and Chebyshev:
*
*  Jacobi/SOR:
*
*     MATSPLIT  calls specific matrix splitting routine
*     JACSPLIT
*     SORSPLIT
*     BACKSOLVE
*
*  Chebyshev:
*
*     GETEIG    computes eigenvalue of iteration matrix. 
*
*     =========================================================== 
      SUBROUTINE MATSPLIT( OMEGA, B, WORK, LDW, METHOD, FLAG )
*
      INTEGER            LDW
      DOUBLE PRECISION   OMEGA, B( * ), WORK( LDW,* )
      CHARACTER          METHOD*1, FLAG*1
      LOGICAL            LSAME
*     ..
*     .. Parameters ..
*
*     MAXDIM2 = MAXDIM*MAXDIM.
*
      INTEGER             MAXDIM, MAXDIM2
      PARAMETER         ( MAXDIM = 200, MAXDIM2 = 40000 )
*
*     .. Common Blocks ..
      INTEGER             N, LDA
      DOUBLE PRECISION    A, M
*
      COMMON            / SYSTEM / A( MAXDIM2 ), M( MAXDIM ),
     $                  / MATDIM / N, LDA
*
      IF ( LSAME( METHOD,'JACOBI' ) ) THEN
         CALL JACSPLIT( N, A, LDA, WORK, LDW, FLAG )
      ELSE IF ( LSAME( METHOD, 'SOR' ) ) THEN
         CALL SORSPLIT( OMEGA, N, A, LDA, B, WORK, LDW, FLAG )
      ELSE 
         WRITE(*,*) 'ERROR: UNKNOW METHOD. QUITTING...'
         STOP
      ENDIF
*
      RETURN
*
      END
*
*     ===========================================================
      SUBROUTINE JACSPLIT( N, A, LDA, WORK, LDW, FLAG )
*
      INTEGER            I, J, N, LDA, LDW
      DOUBLE PRECISION   ZERO, ONE, A( LDA,* ), WORK( LDW,* )
      CHARACTER          FLAG*1
      LOGICAL            LSAME
      PARAMETER        ( ZERO = 0.0D+0, ONE = 1.0D+0 )
*
      IF ( LSAME( FLAG,'SPLIT' ) ) THEN
         DO 20 I = 1, N
            WORK( I,1 ) = ONE / A( I,I )
            A( I,I ) = ZERO
            DO 10 J = 1, N
               A( I,J ) = -A( I,J )
   10       CONTINUE
   20    CONTINUE 
      ELSE IF( LSAME( FLAG,'RECONSTRUCT' ) ) THEN
         DO 40 I = 1, N
            DO 30 J = 1, N
               A( I,J ) = -A( I,J )
   30       CONTINUE
            A( I,I ) = ONE / WORK( I,1 )
   40    CONTINUE
      ELSE
         WRITE(*,*) 'UNKNOWN SPLITTING OPTION. QUITTING...'
         STOP
      ENDIF
*
      RETURN
*
      END
*
*     ===========================================================
      SUBROUTINE SORSPLIT( OMEGA, N, A, LDA, B, WORK, LDW, FLAG )
*  
      INTEGER            I, J, N, LDA, LDW
      DOUBLE PRECISION   OMEGA, ZERO, ONE,
     $                   B( * ), A( LDA,* ), WORK( LDW,* )
      CHARACTER          FLAG*3
      LOGICAL            LSAME
      PARAMETER        ( ZERO = 0.0D+0, ONE = 1.0D+0 )
*
      IF ( LSAME( FLAG,'SPLIT' ) ) THEN
*
*        Set M.
*
         DO 20 I = 1, N
            WORK( I,I ) = A( I,I )
            DO 10 J = 1, I-1
               WORK( I,J ) = OMEGA * A( I,J )
   10       CONTINUE
   20    CONTINUE
*
*        Set NN and B.
*
*        Temporarily store the matrix A in order to reconstruct 
*        the original matrix. Because the lower triangular portion
*        of A must be zeroed, this is the easiest way to deal with it. 
*        This causes the requirement that WORK be N x (2N+3).
*
         DO 40 I = 1, N
            DO 30 J = 1, N
               WORK( I,J+N+3 ) = A( I,J )
   30       CONTINUE
   40    CONTINUE 
*
         DO 60 I = 1, N
            B( I ) = OMEGA * B( I )
            A( I,I ) = ( ONE-OMEGA ) * A( I,I )
            DO 50 J = I+1, N
               A( I,J ) = -OMEGA * A( I,J )
   50       CONTINUE
   60    CONTINUE
*
         DO 80 I = 2, N
            DO 70 J = 1, I-1
               A( I,J ) = ZERO
   70       CONTINUE
   80    CONTINUE
*
      ELSE IF( LSAME( FLAG,'RECONSTRUCT' ) ) THEN
         DO 100 I = 1, N
            B( I ) = B( I ) / OMEGA
            DO 90 J = 1, N
               A( I,J ) = WORK( I,J+N+3 )
   90       CONTINUE
  100    CONTINUE
*
      ELSE
         WRITE(*,*) 'UNKNOWN SPLITTING OPTION. QUITTING...'
         STOP
      ENDIF

      RETURN
*
      END
*
*     ========================================================
      SUBROUTINE BACKSOLVE( N, A, LDA, X )
*
*     .. Argument Declarations ..
      INTEGER             N, LDA
      DOUBLE PRECISION    X( * ), A( LDA,* )
*
*     Mask to BLAS routine. X overwritten with inv(A)*X
*
      CALL DTRSV( 'LOWER','NOTRANS','NONUNIT', N, A, LDA, X, 1 )
*
      RETURN
*
      END
*
*     ===========================================================
      SUBROUTINE GETEIG( WORK, LDW, EIGMAX, EIGMIN )
*
*     .. Argument Declarations ..
      INTEGER            LDW
      DOUBLE PRECISION   EIGMAX, EIGMIN, WORK( LDW,* )
*
*     This routine using an LAPACK routine for computing all the
*     eigenvalues of the matrix A. This is for testing purposes only,
*     as this is more expensive than a direct dense solver for the
*     linear system. This is for testing purposes only.
*     ..
*     .. Parameters ..
*
*     MAXDIM2 = MAXDIM*MAXDIM.
*
      INTEGER             MAXDIM, MAXDIM2
      PARAMETER         ( MAXDIM = 200, MAXDIM2 = 40000 )
*
*     .. Common Blocks ..
      INTEGER             N, LDA
      DOUBLE PRECISION    A, M
      CHARACTER           CURPFORM*5
*
      COMMON            / SYSTEM / A( MAXDIM2 ), M( MAXDIM ),
     $                  / MATDIM / N, LDA
     $                  / FORMS  / CURPFORM
*     ..
*     .. Local Scalars ..
      INTEGER            I, INFO
      DOUBLE PRECISION   ZERO, MATNORM, DLAMCH
      PARAMETER        ( ZERO = 0.0D+0 )
      LOGICAL            LSAMEN
*
*     .. Executable Statements ..
*
*     As the matrix A is overwritten in the following routine, we
*     copy it to temporary workspace.
*
      CALL MATCOPY( N, A, LDA, WORK, LDW )

      IF ( LSAMEN( 3, CURPFORM,'JACBI' ) ) THEN
         DO 30 I = 1, N
            WORK( I,I ) = WORK( I,I ) / M( I )
   30    CONTINUE
      ENDIF
*
*     Call LAPACK eigenvalue routine.
*
      CALL DSYEV('NO_VEC','UPPER', N, WORK, LDW, WORK( 1,N+1 ), 
     $            WORK( 1,N+2 ), (3*N)-1, INFO )
*
      IF ( INFO.NE.0 ) THEN
         WRITE(*,*) 
         WRITE(*,*) 'CHEBYSHEV WARNING: DSYEV COULD NOT COMPUTE ALL EIGE
     $NVALUES.'
         WRITE(*,*) 'SETTING EIGMIN/MAX TO DEFAULT VALUES EPS AND |A|'
         WRITE(*,*) 
         EIGMIN = DLAMCH('EPS')
         EIGMAX = MATNORM( N, A, LDA )
         RETURN
      ELSE
         EIGMIN = WORK( 1,N+1 )
         EIGMAX = WORK( N,N+1 )
      ENDIF
*
*     Eigenvalues should be positive.
*
      IF ( EIGMIN.LT.ZERO ) THEN
         WRITE(*,*) 'CHEBYSHEV WARNING: COMPUTED MIN EIGENVALUE <= 0: SE
     $T TO EPSILON'
         EIGMIN = DLAMCH('EPS')
      ENDIF
      IF ( EIGMAX.LT.EIGMIN ) THEN
         WRITE(*,*) 'CHEBYSHEV WARNING: MAX EIGENVALUE < MIN: SET TO |A|
     $'
         EIGMAX = MATNORM( N, A, LDA )
      ENDIF
*
      RETURN
*
      END
*
*     ================================================================
      SUBROUTINE MATCOPY( N, A, LDA, B, LDB )
*
      INTEGER            N, LDA, LDB
      DOUBLE PRECISION   A( LDA,* ), B( LDB,* )
*
      INTEGER            I, J
*
      DO 20 J = 1, N
         DO 10 I = 1, N
            B( I,J ) = A( I,J )
   10    CONTINUE
   20 CONTINUE
*
      RETURN
*
      END
