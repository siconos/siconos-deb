/*
   Use this command to compile the example:
   cl xp_example2.c api/gdxcc.c api/optcc.c api/gamsxcc.c -Iapi
   */

/*
   This program performs the following steps:
   1. Generate a gdx file with demand data
   2. Calls GAMS to solve a simple transportation model
   (The GAMS model writes the solution to a gdx file)
   3. The solution is read from the gdx file
   */

/* GAMS stuff */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>

#include "NumericsMatrix.h"
#include "FrictionContactProblem.h"

#ifdef HAVE_GAMS_C_API

#include "GAMSlink.h"

#include <math.h>

#include "sanitizer.h"

#define DEBUG_NOCOLOR
//#define DEBUG_STDOUT
//#define DEBUG_MESSAGES
#include "debug.h"

#define ETERMINATE 4242

#define TOTAL_TIME_USED 2

#define TOTAL_ITER 2
#define LAST_MODEL_STATUS 3
#define LAST_SOLVE_STATUS 4

//#define SMALL_APPROX

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

static void setDashedOptions(const char* optName, const char* optValue, const char* paramFileName)
{
  FILE* f = fopen(paramFileName, "a");
  if (f)
  {
    fprintf(f, "%s %s\n", optName, paramFileName);
    fclose(f);
  }
  else
  {
    printf("Failed to create option %s with value %s in %s\n", optName, optValue, paramFileName);
  }
}

void filename_datafiles(const int iter, const int solverId, const char* base_name, unsigned len, char* template_name, char* log_filename)
{
  char iterStr[40];
  snprintf(iterStr, sizeof(iterStr), "-i%d-%s", iter, idToName(solverId));
  if (base_name)
  {
    strncpy(template_name, base_name, len);
    strncpy(log_filename, base_name, len);
  }
  else
  {
    strncpy(template_name, "fc3d_avi-condensed", len);
    strncpy(log_filename, "fc3d_avi-condense-log", len);
  }

  strncat(template_name, iterStr, len - strlen(template_name) - 1);
  strncat(log_filename, iterStr, len - strlen(log_filename) - 1);
  strncat(log_filename, ".log", len - strlen(log_filename) - 1);
}



int SN_gams_solve(unsigned iter, optHandle_t Optr, char* sysdir, char* model, const char* base_name, SolverOptions* options, SN_GAMS_gdx* gdx_data)
{
  assert(gdx_data);
  SN_GAMS_NM_gdx* mat_for_gdx = gdx_data->mat_for_gdx;
  SN_GAMS_NV_gdx* vec_for_gdx = gdx_data->vec_for_gdx;
  SN_GAMS_NV_gdx* vec_from_gdx = gdx_data->vec_from_gdx;

  char msg[GMS_SSSIZE];
  int status;
  gamsxHandle_t Gptr = NULL;
  idxHandle_t Xptr = NULL;
  gmoHandle_t gmoPtr = NULL;
  double infos[4] = {0.};
  /* Create objects */

  DEBUG_PRINT("FC3D_AVI_GAMS :: creating gamsx object\n");
  if (! gamsxCreateD (&Gptr, sysdir, msg, sizeof(msg))) {
    fprintf(stderr, "Could not create gamsx object: %s\n", msg);
    return 1;
  }

  DEBUG_PRINT("FC3D_AVI_GAMS :: creating gdx object\n");
  if (! idxCreateD (&Xptr, sysdir, msg, sizeof(msg))) {
    fprintf(stderr, "Could not create gdx object: %s\n", msg);
    return 1;
  }

  DEBUG_PRINT("FC3D_AVI_GAMS :: creating gmo object\n");
  if (! gmoCreateD (&gmoPtr, sysdir, msg, sizeof(msg))) {
    fprintf(stderr, "Could not create gmo object: %s\n", msg);
    return 1;
  }

  /* create input and output gdx names*/
  char gdxFileName[GMS_SSSIZE];
  char solFileName[GMS_SSSIZE];
//  char paramFileName[GMS_SSSIZE];

  strncpy(gdxFileName, base_name, sizeof(gdxFileName));
  strncpy(solFileName, base_name, sizeof(solFileName));
  strncat(solFileName, "_sol", sizeof(solFileName) - strlen(solFileName) - 1);

  strncat(gdxFileName, ".gdx", sizeof(gdxFileName) - strlen(gdxFileName) - 1);
  strncat(solFileName, ".gdx", sizeof(solFileName) - strlen(solFileName) - 1);
//  strncat(paramFileName, ".txt", sizeof(paramFileName));

  /* XXX ParmFile is not a string option */
//  optSetStrStr(Optr, "ParmFile", paramFileName);
//  setDashedOptions("filename", gdxFileName, paramFileName);
   optSetStrStr(Optr, "User1", gdxFileName);
   optSetStrStr(Optr, "User2", solFileName);

   idxOpenWrite(Xptr, gdxFileName, "Siconos/Numerics NM_to_GDX", &status);
   if (status)
     idxerrorR(status, "idxOpenWrite");
   DEBUG_PRINT("FC3D_AVI_GAMS :: fc3d_avi-condensed.gdx opened\n");

   while (mat_for_gdx)
   {
     char mat_descr[30];
     assert(mat_for_gdx->name);
     assert(mat_for_gdx->mat);
     snprintf(mat_descr, sizeof(mat_descr), "%s matrix", mat_for_gdx->name);
     if ((status=NM_to_GDX(Xptr, mat_for_gdx->name, mat_descr, mat_for_gdx->mat))) {
       fprintf(stderr, "Model data for matrix %s not written\n", mat_for_gdx->name);
       infos[1] = (double)-ETERMINATE;
       goto fail;
     }
     DEBUG_PRINTF("GAMSlink :: %s matrix written\n", mat_for_gdx->name);
     mat_for_gdx = mat_for_gdx->next;
   }

   while (vec_for_gdx)
   {
     char vec_descr[30];
     assert(vec_for_gdx->name);
     assert(vec_for_gdx->vec);
     assert(vec_for_gdx->size > 0);
     snprintf(vec_descr, sizeof(vec_descr), "%s vector", vec_for_gdx->name);

     if ((status=NV_to_GDX(Xptr, vec_for_gdx->name, vec_descr, vec_for_gdx->vec, vec_for_gdx->size))) {
       fprintf(stderr, "Model data for vector %s not written\n", vec_for_gdx->name);
       infos[1] = (double)-ETERMINATE;
       goto fail;
     }
     DEBUG_PRINTF("FC3D_AVI_GAMS :: %s vector written\n", vec_for_gdx->name);
     vec_for_gdx = vec_for_gdx->next;

   }

  if (idxClose(Xptr))
    idxerrorR(idxGetLastError(Xptr), "idxClose");


//   cp(gdxFileName, "fc3d_avi-condensed.gdx");


  if ((status=CallGams(Gptr, Optr, sysdir, model))) {
    fprintf(stderr, "Call to GAMS failed\n");
    infos[1] = (double)-ETERMINATE;
    goto fail;
  }


  /************************************************
   * Read back solution
   ************************************************/
  idxOpenRead(Xptr, solFileName, &status);
  if (status)
    idxerrorR(status, "idxOpenRead");

  while (vec_from_gdx)
  {
    assert(vec_from_gdx->name);
    assert(vec_from_gdx->vec);
    assert(vec_from_gdx->size > 0);
    double* data = vec_from_gdx->vec;
    unsigned size = vec_from_gdx->size;
    /* GAMS does not set a value to 0 ... --xhub */
    memset(data, 0, size*sizeof(double));
    if ((status=GDX_to_NV(Xptr, vec_from_gdx->name, data, size))) {
      fprintf(stderr, "Model data %s could not be read\n", vec_from_gdx->name);
      infos[1] = (double)-ETERMINATE;
      goto fail;
    }
    vec_from_gdx = vec_from_gdx->next;
  }

  if ((status=GDX_to_NV(Xptr, "infos", infos, sizeof(infos)/sizeof(double)))) {
    fprintf(stderr, "infos could not be read\n");
    infos[1] = (double)-ETERMINATE;
    goto fail;
  }

  if (idxClose(Xptr))
    idxerrorR(idxGetLastError(Xptr), "idxClose");

  options->iparam[TOTAL_ITER] += (int)infos[2];
  options->iparam[LAST_MODEL_STATUS] = (int)infos[0];
  options->iparam[LAST_SOLVE_STATUS] = (int)infos[1];
  options->dparam[TOTAL_TIME_USED] += infos[3];
  printf("SolveStat = %d, ModelStat = %d\n", (int)infos[1], (int)infos[0]);
  gmoGetModelStatusTxt(gmoPtr, (int)infos[0], msg);
  DEBUG_PRINTF("%s\n", msg);
  gmoGetSolveStatusTxt(gmoPtr, (int)infos[1], msg);
  DEBUG_PRINTF("%s\n", msg);

fail:
  idxFree(&Xptr);
  gamsxFree(&Gptr);
  gmoFree(&gmoPtr);
  return (int)infos[1];
}

/*
static void FC3D_gams_generate_first_constraints(NumericsMatrix* Akmat, double* mus)
{
  unsigned nb_contacts = (unsigned)Akmat->size1/3;
  assert(nb_contacts*3 == (unsigned)Akmat->size1);
  unsigned nb_approx = (unsigned)Akmat->size0/nb_contacts;
  assert(nb_approx*nb_contacts == (unsigned)Akmat->size0);
  unsigned offset_row = 0;
  CSparseMatrix* triplet_mat = Akmat->matrix2->triplet;

  double angle = 2*M_PI/(NB_APPROX + 1);
  DEBUG_PRINTF("angle: %g\n", angle);

  for (unsigned j = 0; j < nb_contacts; ++j)
  {
    double mu = mus[j];
    for (unsigned i = 0; i < nb_approx; ++i)
    {
      cs_entry(triplet_mat, i + offset_row, 3*j, mu);
      cs_entry(triplet_mat, i + offset_row, 3*j + 1, cos(i*angle));
      cs_entry(triplet_mat, i + offset_row, 3*j + 2, sin(i*angle));
    }
    offset_row += nb_approx;
  }
}
*/
#endif
