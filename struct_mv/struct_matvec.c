/*BHEADER**********************************************************************
 * (c) 1997   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision$
 *********************************************************************EHEADER*/
/******************************************************************************
 *
 * Structured matrix-vector multiply routine
 *
 *****************************************************************************/

#include "headers.h"
#ifdef HYPRE_USE_PTHREADS
#include "box_pthreads.h"
#endif

/*--------------------------------------------------------------------------
 * hypre_StructMatvecData data structure
 *--------------------------------------------------------------------------*/

typedef struct
{
   hypre_StructMatrix  *A;
   hypre_StructVector  *x;
   hypre_ComputePkg    *compute_pkg;

} hypre_StructMatvecData;

/*--------------------------------------------------------------------------
 * hypre_StructMatvecInitialize
 *--------------------------------------------------------------------------*/

void *
hypre_StructMatvecInitialize( )
{
   hypre_StructMatvecData *matvec_data;

   matvec_data = hypre_CTAlloc(hypre_StructMatvecData, 1);

   return (void *) matvec_data;
}

/*--------------------------------------------------------------------------
 * hypre_StructMatvecSetup
 *--------------------------------------------------------------------------*/

int
hypre_StructMatvecSetup( void               *matvec_vdata,
                         hypre_StructMatrix *A,
                         hypre_StructVector *x            )
{
   int ierr = 0;

   hypre_StructMatvecData  *matvec_data = matvec_vdata;
                          
   hypre_StructGrid        *grid;
   hypre_StructStencil     *stencil;
                          
   hypre_BoxArrayArray     *send_boxes;
   hypre_BoxArrayArray     *recv_boxes;
   int                    **send_processes;
   int                    **recv_processes;
   hypre_BoxArrayArray     *indt_boxes;
   hypre_BoxArrayArray     *dept_boxes;

   hypre_Index              unit_stride;
                       
   hypre_ComputePkg        *compute_pkg;

   /*----------------------------------------------------------
    * Set up the compute package
    *----------------------------------------------------------*/

   grid    = hypre_StructMatrixGrid(A);
   stencil = hypre_StructMatrixStencil(A);

   hypre_GetComputeInfo(&send_boxes, &recv_boxes,
                        &send_processes, &recv_processes,
                        &indt_boxes, &dept_boxes,
                        grid, stencil);

   hypre_SetIndex(unit_stride, 1, 1, 1);
   compute_pkg = hypre_NewComputePkg(send_boxes, recv_boxes,
                                     unit_stride, unit_stride,
                                     send_processes, recv_processes,
                                     indt_boxes, dept_boxes,
                                     unit_stride,
                                     grid, hypre_StructVectorDataSpace(x), 1);

   /*----------------------------------------------------------
    * Set up the matvec data structure
    *----------------------------------------------------------*/

   (matvec_data -> A)           = A;
   (matvec_data -> x)           = x;
   (matvec_data -> compute_pkg) = compute_pkg;

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_StructMatvecCompute
 *--------------------------------------------------------------------------*/

int
hypre_StructMatvecCompute( void               *matvec_vdata,
                           double              alpha,
                           hypre_StructMatrix *A,
                           hypre_StructVector *x,
                           double              beta,
                           hypre_StructVector *y            )
{
   int ierr = 0;

   hypre_StructMatvecData  *matvec_data = matvec_vdata;
                          
   hypre_ComputePkg        *compute_pkg;
                          
   hypre_CommHandle        *comm_handle;
                          
   hypre_BoxArrayArray     *compute_box_aa;
   hypre_BoxArray          *compute_box_a;
   hypre_Box               *compute_box;
                          
   hypre_Box               *A_data_box;
   hypre_Box               *x_data_box;
   hypre_Box               *y_data_box;
                          
   int                      Ai;
   int                      xi, xoffset;
   int                      yi;
                          
   double                  *Ap;
   double                  *xp;
   double                  *yp;
                          
   hypre_BoxArray          *boxes;
   hypre_Box               *box;
   hypre_Index              loop_size;
   hypre_IndexRef           start;
   hypre_IndexRef           stride;
                          
   hypre_StructStencil     *stencil;
   hypre_Index             *stencil_shape;
   int                      stencil_size;
                          
   double                   temp;
   int                      compute_i, i, j, si;
   int                      loopi, loopj, loopk;

   /*-----------------------------------------------------------------------
    * Initialize some things
    *-----------------------------------------------------------------------*/

   compute_pkg = (matvec_data -> compute_pkg);

   stride = hypre_ComputePkgStride(compute_pkg);

   /*-----------------------------------------------------------------------
    * Do (alpha == 0.0) computation
    *-----------------------------------------------------------------------*/

   if (alpha == 0.0)
   {
      boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(A));
      hypre_ForBoxI(i, boxes)
         {
            box   = hypre_BoxArrayBox(boxes, i);
            start = hypre_BoxIMin(box);

            y_data_box = hypre_BoxArrayBox(hypre_StructVectorDataSpace(y), i);
            yp = hypre_StructVectorBoxData(y, i);

            hypre_GetBoxSize(box, loop_size);
            hypre_BoxLoop1(loopi, loopj, loopk, loop_size,
                           y_data_box, start, stride, yi,
                           {
                              yp[yi] *= beta;
                           });
         }

      return ierr;
   }

   /*-----------------------------------------------------------------------
    * Do (alpha != 0.0) computation
    *-----------------------------------------------------------------------*/

   stencil       = hypre_StructMatrixStencil(A);
   stencil_shape = hypre_StructStencilShape(stencil);
   stencil_size  = hypre_StructStencilSize(stencil);

   for (compute_i = 0; compute_i < 2; compute_i++)
   {
      switch(compute_i)
      {
         case 0:
         {
            xp = hypre_StructVectorData(x);
            comm_handle = hypre_InitializeIndtComputations(compute_pkg, xp);
            compute_box_aa = hypre_ComputePkgIndtBoxes(compute_pkg);

            /*--------------------------------------------------------------
             * initialize y= (beta/alpha)*y
             *--------------------------------------------------------------*/

            temp = beta / alpha;
            if (temp != 1.0)
            {
               boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(A));
               hypre_ForBoxI(i, boxes)
                  {
                     box   = hypre_BoxArrayBox(boxes, i);
                     start = hypre_BoxIMin(box);

                     y_data_box =
                        hypre_BoxArrayBox(hypre_StructVectorDataSpace(y), i);
                     yp = hypre_StructVectorBoxData(y, i);

                     if (temp == 0.0)
                     {
                        hypre_GetBoxSize(box, loop_size);
                        hypre_BoxLoop1(loopi, loopj, loopk, loop_size,
                                       y_data_box, start, stride, yi,
                                       {
                                          yp[yi] = 0.0;
                                       });
                     }
                     else
                     {
                        hypre_GetBoxSize(box, loop_size);
                        hypre_BoxLoop1(loopi, loopj, loopk, loop_size,
                                       y_data_box, start, stride, yi,
                                       {
                                          yp[yi] *= temp;
                                       });
                     }
                  }
            }
         }
         break;

         case 1:
         {
            hypre_FinalizeIndtComputations(comm_handle);
            compute_box_aa = hypre_ComputePkgDeptBoxes(compute_pkg);
         }
         break;
      }

      /*--------------------------------------------------------------------
       * y += A*x
       *--------------------------------------------------------------------*/

      hypre_ForBoxArrayI(i, compute_box_aa)
         {
            compute_box_a = hypre_BoxArrayArrayBoxArray(compute_box_aa, i);

            A_data_box = hypre_BoxArrayBox(hypre_StructMatrixDataSpace(A), i);
            x_data_box = hypre_BoxArrayBox(hypre_StructVectorDataSpace(x), i);
            y_data_box = hypre_BoxArrayBox(hypre_StructVectorDataSpace(y), i);

            xp = hypre_StructVectorBoxData(x, i);
            yp = hypre_StructVectorBoxData(y, i);

            hypre_ForBoxI(j, compute_box_a)
               {
                  compute_box = hypre_BoxArrayBox(compute_box_a, j);

                  hypre_GetBoxSize(compute_box, loop_size);
                  start  = hypre_BoxIMin(compute_box);

                  for (si = 0; si < stencil_size; si++)
                  {
                     Ap = hypre_StructMatrixBoxData(A, i, si);

                     xoffset = hypre_BoxOffsetDistance(x_data_box,
                                                       stencil_shape[si]);

                     hypre_BoxLoop3(loopi, loopj, loopk, loop_size,
                                    A_data_box, start, stride, Ai,
                                    x_data_box, start, stride, xi,
                                    y_data_box, start, stride, yi,
                                    {
                                       yp[yi] += Ap[Ai] * xp[xi + xoffset];
                                    });
                  }

                  if (alpha != 1.0)
                  {
                     hypre_BoxLoop1(loopi, loopj, loopk, loop_size,
                                    y_data_box, start, stride, yi,
                                    {
                                       yp[yi] *= alpha;
                                    });
                  }
               }
         }
   }
   
   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_StructMatvecFinalize
 *--------------------------------------------------------------------------*/

int
hypre_StructMatvecFinalize( void *matvec_vdata )
{
   int ierr = 0;

   hypre_StructMatvecData *matvec_data = matvec_vdata;

   if (matvec_data)
   {
      hypre_FreeComputePkg(matvec_data -> compute_pkg );
      hypre_TFree(matvec_data);
   }

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_StructMatvec
 *--------------------------------------------------------------------------*/

int
hypre_StructMatvec( double              alpha,
                    hypre_StructMatrix *A,
                    hypre_StructVector *x,
                    double              beta,
                    hypre_StructVector *y     )
{
   int ierr = 0;

   void *matvec_data;

   matvec_data = hypre_StructMatvecInitialize();
   ierr = hypre_StructMatvecSetup(matvec_data, A, x);
   ierr = hypre_StructMatvecCompute(matvec_data, alpha, A, x, beta, y);
   ierr = hypre_StructMatvecFinalize(matvec_data);

   return ierr;
}
