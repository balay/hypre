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
 * Routine for computing residuals in the SMG code
 *
 *****************************************************************************/

#ifdef HYPRE_USE_PTHREADS
#include "box_pthreads.h"
#endif
#include "headers.h"

/*--------------------------------------------------------------------------
 * hypre_SMGResidualData data structure
 *--------------------------------------------------------------------------*/

typedef struct
{
   hypre_Index          base_index;
   hypre_Index          base_stride;

   hypre_StructMatrix  *A;
   hypre_StructVector  *x;
   hypre_StructVector  *b;
   hypre_StructVector  *r;
   hypre_BoxArray      *base_points;
   hypre_ComputePkg    *compute_pkg;

   int                  time_index;
   int                  flops;

} hypre_SMGResidualData;

/*--------------------------------------------------------------------------
 * hypre_SMGResidualInitialize
 *--------------------------------------------------------------------------*/

void *
hypre_SMGResidualInitialize( )
{
   hypre_SMGResidualData *residual_data;

   residual_data = hypre_CTAlloc(hypre_SMGResidualData, 1);

#ifdef HYPRE_USE_PTHREADS
   hypre_TimingThreadWrapper({
      (residual_data -> time_index)  = hypre_InitializeTiming("SMGResidual");
   });
#else
   (residual_data -> time_index)  = hypre_InitializeTiming("SMGResidual");
#endif
   /* set defaults */
   hypre_SetIndex((residual_data -> base_index), 0, 0, 0);
   hypre_SetIndex((residual_data -> base_stride), 1, 1, 1);

   return (void *) residual_data;
}

/*--------------------------------------------------------------------------
 * hypre_SMGResidualSetup
 *--------------------------------------------------------------------------*/

int
hypre_SMGResidualSetup( void               *residual_vdata,
                        hypre_StructMatrix *A,
                        hypre_StructVector *x,
                        hypre_StructVector *b,
                        hypre_StructVector *r              )
{
   int ierr = 0;

   hypre_SMGResidualData  *residual_data = residual_vdata;

   hypre_IndexRef          base_index  = (residual_data -> base_index);
   hypre_IndexRef          base_stride = (residual_data -> base_stride);
   hypre_Index             unit_stride;

   hypre_StructGrid       *grid;
   hypre_StructStencil    *stencil;
                       
   hypre_BoxArrayArray    *send_boxes;
   hypre_BoxArrayArray    *recv_boxes;
   int                   **send_processes;
   int                   **recv_processes;
   hypre_BoxArrayArray    *indt_boxes;
   hypre_BoxArrayArray    *dept_boxes;
                       
   hypre_BoxArray         *base_points;
   hypre_ComputePkg       *compute_pkg;

   /*----------------------------------------------------------
    * Set up base points and the compute package
    *----------------------------------------------------------*/

   grid    = hypre_StructMatrixGrid(A);
   stencil = hypre_StructMatrixStencil(A);

   hypre_SetIndex(unit_stride, 1, 1, 1);

   base_points = hypre_DuplicateBoxArray(hypre_StructGridBoxes(grid));
   hypre_ProjectBoxArray(base_points, base_index, base_stride);

   hypre_GetComputeInfo(&send_boxes, &recv_boxes,
                        &send_processes, &recv_processes,
                        &indt_boxes, &dept_boxes,
                        grid, stencil);

   hypre_ProjectBoxArrayArray(indt_boxes, base_index, base_stride);
   hypre_ProjectBoxArrayArray(dept_boxes, base_index, base_stride);

   compute_pkg = hypre_NewComputePkg(send_boxes, recv_boxes,
                                     unit_stride, unit_stride,
                                     send_processes, recv_processes,
                                     indt_boxes, dept_boxes,
                                     base_stride, grid,
                                     hypre_StructVectorDataSpace(x), 1);

   /*----------------------------------------------------------
    * Set up the residual data structure
    *----------------------------------------------------------*/

   (residual_data -> A)           = A;
   (residual_data -> x)           = x;
   (residual_data -> b)           = b;
   (residual_data -> r)           = r;
   (residual_data -> base_points) = base_points;
   (residual_data -> compute_pkg) = compute_pkg;

   /*-----------------------------------------------------
    * Compute flops
    *-----------------------------------------------------*/

   (residual_data -> flops) =
      (hypre_StructMatrixGlobalSize(A) + hypre_StructVectorGlobalSize(x)) /
      (hypre_IndexX(base_stride) *
       hypre_IndexY(base_stride) *
       hypre_IndexZ(base_stride)  );

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SMGResidual
 *--------------------------------------------------------------------------*/

int
hypre_SMGResidual( void               *residual_vdata,
                   hypre_StructMatrix *A,
                   hypre_StructVector *x,
                   hypre_StructVector *b,
                   hypre_StructVector *r              )
{
   int ierr = 0;

   hypre_SMGResidualData  *residual_data = residual_vdata;

   hypre_IndexRef          base_stride = (residual_data -> base_stride);
   hypre_BoxArray         *base_points = (residual_data -> base_points);
   hypre_ComputePkg       *compute_pkg = (residual_data -> compute_pkg);

   hypre_CommHandle       *comm_handle;
                       
   hypre_BoxArrayArray    *compute_box_aa;
   hypre_BoxArray         *compute_box_a;
   hypre_Box              *compute_box;
                       
   hypre_Box              *A_data_box;
   hypre_Box              *x_data_box;
   hypre_Box              *b_data_box;
   hypre_Box              *r_data_box;
                       
   int                     Ai;
   int                     xi;
   int                     bi;
   int                     ri;
                         
   double                 *Ap;
   double                 *xp;
   double                 *bp;
   double                 *rp;
                       
   hypre_Index             loop_size;
   hypre_IndexRef          start;
                       
   hypre_StructStencil    *stencil;
   hypre_Index            *stencil_shape;
   int                     stencil_size;

   int                     compute_i, i, j, si;
   int                     loopi, loopj, loopk;

#ifdef HYPRE_USE_PTHREADS
   hypre_TimingThreadWrapper({
      hypre_BeginTiming(residual_data -> time_index);
   });
#else
   hypre_BeginTiming(residual_data -> time_index);
#endif

   /*-----------------------------------------------------------------------
    * Compute residual r = b - Ax
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

            /*----------------------------------------
             * Copy b into r
             *----------------------------------------*/

            compute_box_a = base_points;
            hypre_ForBoxI(i, compute_box_a)
               {
                  compute_box = hypre_BoxArrayBox(compute_box_a, i);
                  start = hypre_BoxIMin(compute_box);

                  b_data_box =
                     hypre_BoxArrayBox(hypre_StructVectorDataSpace(b), i);
                  r_data_box =
                     hypre_BoxArrayBox(hypre_StructVectorDataSpace(r), i);

                  bp = hypre_StructVectorBoxData(b, i);
                  rp = hypre_StructVectorBoxData(r, i);

                  hypre_GetStrideBoxSize(compute_box, base_stride, loop_size);
                  hypre_BoxLoop2(loopi, loopj, loopk, loop_size,
                                 b_data_box, start, base_stride, bi,
                                 r_data_box, start, base_stride, ri,
                                 {
                                    rp[ri] = bp[bi];
                                 });
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
       * Compute r -= A*x
       *--------------------------------------------------------------------*/

      hypre_ForBoxArrayI(i, compute_box_aa)
         {
            compute_box_a = hypre_BoxArrayArrayBoxArray(compute_box_aa, i);

            A_data_box = hypre_BoxArrayBox(hypre_StructMatrixDataSpace(A), i);
            x_data_box = hypre_BoxArrayBox(hypre_StructVectorDataSpace(x), i);
            r_data_box = hypre_BoxArrayBox(hypre_StructVectorDataSpace(r), i);

            rp = hypre_StructVectorBoxData(r, i);

            hypre_ForBoxI(j, compute_box_a)
               {
                  compute_box = hypre_BoxArrayBox(compute_box_a, j);

                  start  = hypre_BoxIMin(compute_box);

                  for (si = 0; si < stencil_size; si++)
                  {
                     Ap = hypre_StructMatrixBoxData(A, i, si);
                     xp = hypre_StructVectorBoxData(x, i) +
                        hypre_BoxOffsetDistance(x_data_box, stencil_shape[si]);

                     hypre_GetStrideBoxSize(compute_box, base_stride,
                                            loop_size);
                     hypre_BoxLoop3(loopi, loopj, loopk, loop_size,
                                    A_data_box, start, base_stride, Ai,
                                    x_data_box, start, base_stride, xi,
                                    r_data_box, start, base_stride, ri,
                                    {
                                       rp[ri] -= Ap[Ai] * xp[xi];
                                    });
                  }
               }
         }
   }
   
   /*-----------------------------------------------------------------------
    * Return
    *-----------------------------------------------------------------------*/

#ifdef HYPRE_USE_PTHREADS
   hypre_TimingThreadWrapper({
      hypre_IncFLOPCount(residual_data -> flops);
      hypre_EndTiming(residual_data -> time_index);
   });
#else
   hypre_IncFLOPCount(residual_data -> flops);
   hypre_EndTiming(residual_data -> time_index);
#endif

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SMGResidualSetBase
 *--------------------------------------------------------------------------*/
 
int
hypre_SMGResidualSetBase( void        *residual_vdata,
                          hypre_Index  base_index,
                          hypre_Index  base_stride )
{
   hypre_SMGResidualData *residual_data = residual_vdata;
   int                    d;
   int                    ierr = 0;
 
   for (d = 0; d < 3; d++)
   {
      hypre_IndexD((residual_data -> base_index),  d)
         = hypre_IndexD(base_index,  d);
      hypre_IndexD((residual_data -> base_stride), d)
         = hypre_IndexD(base_stride, d);
   }
 
   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SMGResidualFinalize
 *--------------------------------------------------------------------------*/

int
hypre_SMGResidualFinalize( void *residual_vdata )
{
   int ierr = 0;

   hypre_SMGResidualData *residual_data = residual_vdata;

   if (residual_data)
   {
      hypre_FreeBoxArray(residual_data -> base_points);
      hypre_FreeComputePkg(residual_data -> compute_pkg );
#ifdef HYPRE_USE_PTHREADS
      hypre_TimingThreadWrapper({
         hypre_FinalizeTiming(residual_data -> time_index);
      });
#else
      hypre_FinalizeTiming(residual_data -> time_index);
#endif
      hypre_TFree(residual_data);
   }

   return ierr;
}

