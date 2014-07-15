/*BHEADER**********************************************************************
 * Copyright (c) 2008,  Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * This file is part of HYPRE.  See file COPYRIGHT for details.
 *
 * HYPRE is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * $Revision$
 ***********************************************************************EHEADER*/

/******************************************************************************
 *
 * Member functions for hypre_StructMatrix class.
 *
 *****************************************************************************/

#include "_hypre_struct_mv.h"

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixMapDataIndex( hypre_StructMatrix *matrix,
                                hypre_Index         index )
{
   hypre_IndexRef dmap = hypre_StructMatrixDMap(matrix);
   HYPRE_Int      ndim = hypre_StructMatrixNDim(matrix);
   HYPRE_Int      d;

   if (hypre_StructMatrixDomainIsCoarse(matrix))
   {
      for (d = 0; d < ndim; d++)
      {
         if ((index[d]%dmap[d]) < 0)
         {
            index[d] -= dmap[d];
         }
         index[d] /= dmap[d];
      }
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixMapDataBox( hypre_StructMatrix *matrix,
                              hypre_Box          *box )
{
   hypre_StructMatrixMapDataIndex(matrix, hypre_BoxIMin(box));
   hypre_StructMatrixMapDataIndex(matrix, hypre_BoxIMax(box));

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * Returns a pointer to data in `matrix' coresponding to the stencil offset
 * specified by `index'. If index does not exist in the matrix stencil, the NULL
 * pointer is returned.
 *--------------------------------------------------------------------------*/
 
HYPRE_Complex *
hypre_StructMatrixExtractPointerByIndex( hypre_StructMatrix *matrix,
                                         HYPRE_Int           b,
                                         hypre_Index         index  )
{
   hypre_StructStencil   *stencil;
   HYPRE_Int              rank;

   stencil = hypre_StructMatrixStencil(matrix);
   rank = hypre_StructStencilElementRank( stencil, index );

   if ( rank >= 0 )
      return hypre_StructMatrixBoxData(matrix, b, rank);
   else
      return NULL;  /* error - invalid index */
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

hypre_StructMatrix *
hypre_StructMatrixCreate( MPI_Comm             comm,
                          hypre_StructGrid    *grid,
                          hypre_StructStencil *user_stencil )
{
   HYPRE_Int            ndim = hypre_StructGridNDim(grid);
   hypre_StructMatrix  *matrix;
   HYPRE_Int            i;

   matrix = hypre_CTAlloc(hypre_StructMatrix, 1);

   hypre_StructMatrixComm(matrix)        = comm;
   hypre_StructGridRef(grid, &hypre_StructMatrixGrid(matrix));
   hypre_StructGridRef(grid, &hypre_StructMatrixDomainGrid(matrix));
   hypre_StructMatrixUserStencil(matrix) =
      hypre_StructStencilRef(user_stencil);
   hypre_StructMatrixConstant(matrix) =
      hypre_CTAlloc(HYPRE_Int, hypre_StructStencilSize(user_stencil));
   hypre_SetIndex(hypre_StructMatrixRMap(matrix), 1);
   hypre_SetIndex(hypre_StructMatrixDMap(matrix), 1);
   hypre_StructMatrixDataAlloced(matrix) = 1;
   hypre_StructMatrixRefCount(matrix)    = 1;

   /* set defaults */
   hypre_StructMatrixDomainIsCoarse(matrix) = 0;
   hypre_StructMatrixSymmetric(matrix) = 0;
   hypre_StructMatrixConstantCoefficient(matrix) = 0;
   for (i = 0; i < 2*ndim; i++)
   {
      hypre_StructMatrixNumGhost(matrix)[i] = hypre_StructGridNumGhost(grid)[i];
   }

   return matrix;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

hypre_StructMatrix *
hypre_StructMatrixRef( hypre_StructMatrix *matrix )
{
   hypre_StructMatrixRefCount(matrix) ++;

   return matrix;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixDestroy( hypre_StructMatrix *matrix )
{
   HYPRE_Int  i;

   if (matrix)
   {
      hypre_StructMatrixRefCount(matrix) --;
      if (hypre_StructMatrixRefCount(matrix) == 0)
      {
         if (hypre_StructMatrixDataAlloced(matrix))
         {
            hypre_SharedTFree(hypre_StructMatrixData(matrix));
         }
         hypre_CommPkgDestroy(hypre_StructMatrixCommPkg(matrix));
         
         hypre_ForBoxI(i, hypre_StructMatrixDataSpace(matrix))
            hypre_TFree(hypre_StructMatrixDataIndices(matrix)[i]);
         hypre_TFree(hypre_StructMatrixDataIndices(matrix));
         
         hypre_BoxArrayDestroy(hypre_StructMatrixDataBoxes(matrix));
         hypre_BoxArrayDestroy(hypre_StructMatrixDataSpace(matrix));
         
         hypre_TFree(hypre_StructMatrixSymmElements(matrix));
         hypre_TFree(hypre_StructMatrixConstant(matrix));
         hypre_StructStencilDestroy(hypre_StructMatrixUserStencil(matrix));
         hypre_StructStencilDestroy(hypre_StructMatrixStencil(matrix));
         hypre_StructGridDestroy(hypre_StructMatrixGrid(matrix));
         hypre_StructGridDestroy(hypre_StructMatrixDomainGrid(matrix));
         
         hypre_TFree(matrix);
      }
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixSetDomainGrid(hypre_StructMatrix *matrix,
                                hypre_StructGrid   *domain_grid)
{
   /* This should only decrement a reference counter */
   hypre_StructGridDestroy(hypre_StructMatrixDomainGrid(matrix));
   hypre_StructGridRef(domain_grid, &hypre_StructMatrixDomainGrid(matrix));

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixSetRMap(hypre_StructMatrix *matrix,
                          HYPRE_Int          *rmap)
{
   hypre_CopyToIndex(rmap, hypre_StructMatrixNDim(matrix),
                     hypre_StructMatrixRMap(matrix));

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixSetDMap(hypre_StructMatrix *matrix,
                          HYPRE_Int          *dmap)
{
   hypre_CopyToIndex(dmap, hypre_StructMatrixNDim(matrix),
                     hypre_StructMatrixDMap(matrix));

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * NOTE: This currently assumes that either rmap or dmap (or both) is 1.
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixInitializeShell( hypre_StructMatrix *matrix )
{
   HYPRE_Int             ndim     = hypre_StructMatrixNDim(matrix);
   hypre_StructGrid     *grid     = hypre_StructMatrixGrid(matrix);
   HYPRE_Int            *constant = hypre_StructMatrixConstant(matrix);
   hypre_IndexRef        rmap     = hypre_StructMatrixRMap(matrix);
   hypre_IndexRef        dmap     = hypre_StructMatrixDMap(matrix);

   hypre_StructStencil  *user_stencil;
   hypre_StructStencil  *stencil;
   hypre_Index          *stencil_shape;
   HYPRE_Int             stencil_size;
   HYPRE_Int             num_values, num_cvalues;
   HYPRE_Int            *symm_elements;
   HYPRE_Int             domain_is_coarse;
                    
   HYPRE_Int            *num_ghost;
   HYPRE_Int             extra_ghost[2*HYPRE_MAXDIM];
 
   hypre_BoxArray       *data_boxes;
   hypre_BoxArray       *data_space;
   hypre_Box            *data_box;

   HYPRE_Int           **data_indices;
   HYPRE_Int             data_size;
   HYPRE_Int             data_box_volume;
                    
   HYPRE_Int             i, j, d;

   /*-----------------------------------------------------------------------
    * First, check consistency of rmap, dmap, and symmetric.
    *-----------------------------------------------------------------------*/

   /* domain_is_coarse={0,1,-1,-2} -> coarse grid={range,domain,neither,error} */
   domain_is_coarse = -1;
   for (d = 0; d < ndim; d++)
   {
      if (rmap[d] > dmap[d])
      {
         if ((domain_is_coarse == 1) || (rmap[d]%dmap[d] != 0))
         {
            domain_is_coarse = -2;
            break;
         }
         else
         {
            /* Range grid is a subset of domain grid */
            domain_is_coarse = 0;
         }
      }
      else if (dmap[d] > rmap[d])
      {
         if ((domain_is_coarse == 0) || (dmap[d]%rmap[d] != 0))
         {
            domain_is_coarse = -2;
            break;
         }
         else
         {
            /* Domain grid is a subset of range grid */
            domain_is_coarse = 1;
         }
      }
   }
   if (domain_is_coarse == -2)
   {
      hypre_error(HYPRE_ERROR_GENERIC);
      return hypre_error_flag;
   }
   /* If neither grid is coarse (a square matrix), use range-based storage */
   if (domain_is_coarse == -1)
   {
      domain_is_coarse = 0;
   }
   else
   {
      /* Can't have a symmetric stencil for a rectangular matrix */
      hypre_StructMatrixSymmetric(matrix) = 0;
   }
   hypre_StructMatrixDomainIsCoarse(matrix) = domain_is_coarse;

   /*-----------------------------------------------------------------------
    * Set up stencil, num_values, and num_cvalues:
    *
    * If the matrix is symmetric, then the stencil is a "symmetrized"
    * version of the user's stencil.  If the matrix is not symmetric,
    * then the stencil is the same as the user's stencil.
    * 
    * The `symm_elements' array is used to determine what data is explicitely
    * stored (symm_elements[i] < 0) and what data is not explicitely stored
    * (symm_elements[i] >= 0), but is instead stored as the transpose
    * coefficient at a neighboring grid point.
    *-----------------------------------------------------------------------*/

   if (hypre_StructMatrixStencil(matrix) == NULL)
   {
      user_stencil = hypre_StructMatrixUserStencil(matrix);
      symm_elements = NULL;

      if (hypre_StructMatrixSymmetric(matrix))
      {
         /* store only symmetric stencil entry data */
         hypre_StructStencilSymmetrize(user_stencil, &stencil, &symm_elements);
         stencil_size = hypre_StructStencilSize(stencil);
         constant = hypre_TReAlloc(constant, HYPRE_Int, stencil_size);
         for (i = 0; i < stencil_size; i++)
         {
            if (symm_elements[i] >= 0)
            {
               constant[i] = constant[symm_elements[i]];
            }
         }
      }
      else
      {
         /* store all stencil entry data */
         stencil = hypre_StructStencilRef(user_stencil);
         stencil_size = hypre_StructStencilSize(stencil);
         symm_elements = hypre_TAlloc(HYPRE_Int, stencil_size);
         for (i = 0; i < stencil_size; i++)
         {
            symm_elements[i] = -1;
         }
      }

      /* Compute number of stored constant and variables coeffs */
      num_values = 0;
      num_cvalues = 0;
      for (j = 0; j < stencil_size; j++)
      {
         if (symm_elements[j] < 0)
         {
            if (constant[j])
            {
               num_cvalues++;
            }
            else
            {
               num_values++;
            }
         }
      }

      hypre_StructMatrixStencil(matrix)      = stencil;
      hypre_StructMatrixConstant(matrix)     = constant;
      hypre_StructMatrixSymmElements(matrix) = symm_elements;
      hypre_StructMatrixNumValues(matrix)    = num_values;
      hypre_StructMatrixNumCValues(matrix)   = num_cvalues;
   }

   /*-----------------------------------------------------------------------
    * Incrememt ghost-layer size for symmetric storage (square matrices only).
    * All stencil coeffs are to be available at each point in the grid,
    * including the user-specified ghost layer.
    *-----------------------------------------------------------------------*/

   num_ghost     = hypre_StructMatrixNumGhost(matrix);
   stencil       = hypre_StructMatrixStencil(matrix);
   stencil_shape = hypre_StructStencilShape(stencil);
   stencil_size  = hypre_StructStencilSize(stencil);
   symm_elements = hypre_StructMatrixSymmElements(matrix);

   /* Initialize extra ghost size */
   for (d = 0; d < 2*ndim; d++)
   {
      extra_ghost[d] = 0;
   }

   /* Add extra ghost layers for symmetric storage */
   if (hypre_StructMatrixSymmetric(matrix))
   {
      for (i = 0; i < stencil_size; i++)
      {
         if (symm_elements[i] >= 0)
         {
            for (d = 0; d < ndim; d++)
            {
               j = hypre_IndexD(stencil_shape[i], d);
               extra_ghost[2*d]     = hypre_max(extra_ghost[2*d],    -j);
               extra_ghost[2*d + 1] = hypre_max(extra_ghost[2*d + 1], j);
            }
         }
      }
   }

   /* Increment number of ghost layers */
   for (d = 0; d < ndim; d++)
   {
      num_ghost[2*d]     += extra_ghost[2*d];
      num_ghost[2*d + 1] += extra_ghost[2*d + 1];
   }

   /*-----------------------------------------------------------------------
    * Set up data_boxes and data_space based on the range grid
    *-----------------------------------------------------------------------*/

   if (hypre_StructMatrixDataSpace(matrix) == NULL)
   {
      data_boxes = hypre_BoxArrayDuplicate(hypre_StructGridBoxes(grid));

      /* Add ghost layers */
      hypre_ForBoxI(i, data_boxes)
      {
         data_box = hypre_BoxArrayBox(data_boxes, i);
         for (d = 0; d < ndim; d++)
         {
            hypre_BoxIMinD(data_box, d) -= num_ghost[2*d];
            hypre_BoxIMaxD(data_box, d) += num_ghost[2*d + 1];
         }
      }
      hypre_StructMatrixDataBoxes(matrix) = data_boxes;

      /* If the domain is coarse, map the data space */
      data_space = hypre_BoxArrayDuplicate(data_boxes);
      if (domain_is_coarse)
      {
         hypre_ForBoxI(i, data_space)
         {
            data_box = hypre_BoxArrayBox(data_space, i);
            hypre_StructMatrixMapDataBox(matrix, data_box);
         }
      }
      hypre_StructMatrixDataSpace(matrix) = data_space;
   }

   /*-----------------------------------------------------------------------
    * Set up data_indices array and data_size
    *-----------------------------------------------------------------------*/

   if (hypre_StructMatrixDataIndices(matrix) == NULL)
   {
      data_space = hypre_StructMatrixDataSpace(matrix);
      data_indices = hypre_CTAlloc(HYPRE_Int *, hypre_BoxArraySize(data_space));

      /* Put constant coefficients at the beginning */
      data_size = stencil_size;
      hypre_StructMatrixVDataOffset(matrix) = data_size;
      hypre_ForBoxI(i, data_space)
      {
         data_box = hypre_BoxArrayBox(data_space, i);
         data_box_volume  = hypre_BoxVolume(data_box);

         data_indices[i] = hypre_CTAlloc(HYPRE_Int, stencil_size);

         /* set pointers for "stored" coefficients */
         for (j = 0; j < stencil_size; j++)
         {
            if (symm_elements[j] < 0)
            {
               if (constant[j])
               {
                  data_indices[i][j] = j;
               }
               else
               {
                  data_indices[i][j] = data_size;
                  data_size += data_box_volume;
               }
            }
         }

         /* set pointers for "symmetric" coefficients */
         for (j = 0; j < stencil_size; j++)
         {
            if (symm_elements[j] >= 0)
            {
               if (constant[j])
               {
                  data_indices[i][j] = data_indices[i][symm_elements[j]];
               }
               else
               {
                  data_indices[i][j] = data_indices[i][symm_elements[j]] +
                     hypre_BoxOffsetDistance(data_box, stencil_shape[j]);
               }
            }
         }
      }

      /* If data space is of size zero, reset a few things */
      if (hypre_BoxArraySize(data_space) == 0)
      {
         data_size    = 0;
         data_indices = NULL;
      }

      hypre_StructMatrixDataSize(matrix)    = data_size;
      hypre_StructMatrixDataIndices(matrix) = data_indices;
   }

   /*-----------------------------------------------------------------------
    * Set total number of nonzero coefficients
    * For constant coefficients, this is unrelated to the amount of data
    * actually stored.
    *-----------------------------------------------------------------------*/

   hypre_StructMatrixGlobalSize(matrix) =
      hypre_StructGridGlobalSize(grid) * stencil_size;

   /*-----------------------------------------------------------------------
    * Return
    *-----------------------------------------------------------------------*/

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixInitializeData( hypre_StructMatrix *matrix,
                                  HYPRE_Complex      *data   )
{
   hypre_StructMatrixData(matrix) = data;
   hypre_StructMatrixDataAlloced(matrix) = 0;

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixInitialize( hypre_StructMatrix *matrix )
{
   HYPRE_Complex *data;

   hypre_StructMatrixInitializeShell(matrix);

   data = hypre_StructMatrixData(matrix);
   data = hypre_SharedCTAlloc(HYPRE_Complex, hypre_StructMatrixDataSize(matrix));
   hypre_StructMatrixInitializeData(matrix, data);
   hypre_StructMatrixDataAlloced(matrix) = 1;

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * (action > 0): add-to values
 * (action = 0): set values
 * (action < 0): get values
 * (action =-2): get values and zero out
 *
 * Should not be called to set a constant-coefficient part of the matrix.
 * Call hypre_StructMatrixSetConstantValues instead.
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixSetValues( hypre_StructMatrix *matrix,
                             hypre_Index         grid_index,
                             HYPRE_Int           num_stencil_indices,
                             HYPRE_Int          *stencil_indices,
                             HYPRE_Complex      *values,
                             HYPRE_Int           action,
                             HYPRE_Int           boxnum,
                             HYPRE_Int           outside )
{
   HYPRE_Int           *constant      = hypre_StructMatrixConstant(matrix);
   HYPRE_Int           *symm_elements = hypre_StructMatrixSymmElements(matrix);
   hypre_BoxArray      *grid_boxes;
   hypre_Box           *grid_box;
   hypre_BoxArray      *data_boxes;
   hypre_BoxArray      *data_space;
   hypre_Box           *data_box;
   HYPRE_Complex       *matp;
   HYPRE_Int            i, j, s, istart, istop;
 
   /*-----------------------------------------------------------------------
    * Initialize some things
    *-----------------------------------------------------------------------*/

   if (outside > 0)
   {
      grid_boxes = hypre_StructMatrixDataBoxes(matrix);
   }
   else
   {
      grid_boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(matrix));
   }
   data_boxes = hypre_StructMatrixDataBoxes(matrix);
   data_space = hypre_StructMatrixDataSpace(matrix);

   if (boxnum < 0)
   {
      istart = 0;
      istop  = hypre_BoxArraySize(grid_boxes);
   }
   else
   {
      istart = boxnum;
      istop  = istart + 1;
   }

   /*-----------------------------------------------------------------------
    * Set the matrix coefficients
    *-----------------------------------------------------------------------*/

   for (i = istart; i < istop; i++)
   {
      grid_box = hypre_BoxArrayBox(grid_boxes, i);
      data_box = hypre_BoxArrayBox(data_boxes, i);

      if (hypre_IndexInBox(grid_index, grid_box))
      {
         hypre_StructMatrixMapDataIndex(matrix, grid_index);

         for (s = 0; s < num_stencil_indices; s++)
         {
            j = stencil_indices[s];

            /* only set stored stencil values */
            if (symm_elements[j] < 0)
            {
               matp = hypre_StructMatrixBoxData(matrix, i, j);
               if (constant[j])
               {
                  /* call SetConstantValues instead */
                  hypre_error(HYPRE_ERROR_GENERIC);
               }
               else
               {
                  matp += hypre_BoxIndexRank(data_box, grid_index);
               }

               if (action > 0)
               {
                  *matp += values[s];
               }
               else if (action > -1)
               {
                  *matp = values[s];
               }
               else /* action < 0 */
               {
                  values[s] = *matp;
                  if (action == -2)
                  {
                     *matp = 0;
                  }
               }
            }
         }
      }
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * (action > 0): add-to values
 * (action = 0): set values
 * (action < 0): get values
 * (action =-2): get values and zero out
 *
 * Should not be called to set a constant-coefficient part of the matrix.
 * Call hypre_StructMatrixSetConstantValues instead.
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixSetBoxValues( hypre_StructMatrix *matrix,
                                hypre_Box          *set_box,
                                hypre_Box          *value_box,
                                HYPRE_Int           num_stencil_indices,
                                HYPRE_Int          *stencil_indices,
                                HYPRE_Complex      *values,
                                HYPRE_Int           action,
                                HYPRE_Int           boxnum,
                                HYPRE_Int           outside )
{
   HYPRE_Int           *constant      = hypre_StructMatrixConstant(matrix);
   HYPRE_Int           *symm_elements = hypre_StructMatrixSymmElements(matrix);
   hypre_BoxArray      *grid_boxes;
   hypre_Box           *grid_box;
   hypre_Box           *int_box;
                   
   hypre_BoxArray      *data_space;
   hypre_Box           *data_box;
   hypre_IndexRef       data_start;
   hypre_Index          data_stride;
   HYPRE_Int            datai;
   HYPRE_Complex       *datap;
                   
   hypre_Box           *dval_box;
   hypre_Index          dval_start;
   hypre_Index          dval_stride;
   HYPRE_Int            dvali;
                   
   hypre_Index          loop_size;
   HYPRE_Int            i, j, s, istart, istop;

   /*-----------------------------------------------------------------------
    * Initialize some things
    *-----------------------------------------------------------------------*/

   if (outside > 0)
   {
      grid_boxes = hypre_StructMatrixDataBoxes(matrix);
   }
   else
   {
      grid_boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(matrix));
   }
   data_space = hypre_StructMatrixDataSpace(matrix);

   if (boxnum < 0)
   {
      istart = 0;
      istop  = hypre_BoxArraySize(grid_boxes);
   }
   else
   {
      istart = boxnum;
      istop  = istart + 1;
   }

   /*-----------------------------------------------------------------------
    * Set the matrix coefficients
    *-----------------------------------------------------------------------*/

   hypre_SetIndex(data_stride, 1);

   int_box = hypre_BoxCreate(hypre_StructMatrixNDim(matrix));
   dval_box = hypre_BoxDuplicate(value_box);
   hypre_BoxIMinD(dval_box, 0) *= num_stencil_indices;
   hypre_BoxIMaxD(dval_box, 0) *= num_stencil_indices;
   hypre_BoxIMaxD(dval_box, 0) += num_stencil_indices - 1;
   hypre_SetIndex(dval_stride, 1);
   hypre_IndexD(dval_stride, 0) = num_stencil_indices;

   for (i = istart; i < istop; i++)
   {
      grid_box = hypre_BoxArrayBox(grid_boxes, i);
      data_box = hypre_BoxArrayBox(data_space, i);

      hypre_IntersectBoxes(set_box, grid_box, int_box);

      /* if there was an intersection */
      if (hypre_BoxVolume(int_box))
      {
         hypre_StructMatrixMapDataBox(matrix, int_box);

         data_start = hypre_BoxIMin(int_box);
         hypre_CopyIndex(data_start, dval_start);
         hypre_IndexD(dval_start, 0) *= num_stencil_indices;

         for (s = 0; s < num_stencil_indices; s++)
         {
            j = stencil_indices[s];

            /* only set stored stencil values */
            if (symm_elements[j] < 0)
            {
               datap = hypre_StructMatrixBoxData(matrix, i, j);

               if (constant[j])
               {
                  /* call SetConstantValues instead */
                  hypre_error(HYPRE_ERROR_GENERIC);
                  dvali = hypre_BoxIndexRank(dval_box, dval_start);

                  if (action > 0)
                  {
                     *datap += values[dvali];
                  }
                  else if (action > -1)
                  {
                     *datap = values[dvali];
                  }
                  else
                  {
                     values[dvali] = *datap;
                     if (action == -2)
                     {
                        *datap = 0;
                     }
                  }
               }
               else
               {
                  hypre_BoxGetSize(int_box, loop_size);

                  if (action > 0)
                  {
                     hypre_BoxLoop2Begin(hypre_StructMatrixNDim(matrix), loop_size,
                                         data_box, data_start, data_stride, datai,
                                         dval_box, dval_start, dval_stride, dvali);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,datai,dvali) HYPRE_SMP_SCHEDULE
#endif
                     hypre_BoxLoop2For(datai, dvali)
                     {
                        datap[datai] += values[dvali];
                     }
                     hypre_BoxLoop2End(datai, dvali);
                  }
                  else if (action > -1)
                  {
                     hypre_BoxLoop2Begin(hypre_StructMatrixNDim(matrix), loop_size,
                                         data_box, data_start, data_stride, datai,
                                         dval_box, dval_start, dval_stride, dvali);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,datai,dvali) HYPRE_SMP_SCHEDULE
#endif
                     hypre_BoxLoop2For(datai, dvali)
                     {
                        datap[datai] = values[dvali];
                     }
                     hypre_BoxLoop2End(datai, dvali);
                  }
                  else
                  {
                     hypre_BoxLoop2Begin(hypre_StructMatrixNDim(matrix), loop_size,
                                         data_box, data_start, data_stride, datai,
                                         dval_box, dval_start, dval_stride, dvali);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,datai,dvali) HYPRE_SMP_SCHEDULE
#endif
                     hypre_BoxLoop2For(datai, dvali)
                     {
                        values[dvali] = datap[datai];
                        if (action == -2)
                        {
                           datap[datai] = 0;
                        }
                     }
                     hypre_BoxLoop2End(datai, dvali);
                  }
               }
            } /* end if (symm_elements) */

            hypre_IndexD(dval_start, 0) ++;
         }
      }
   }

   hypre_BoxDestroy(int_box);
   hypre_BoxDestroy(dval_box);

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * (action > 0): add-to values
 * (action = 0): set values
 * (action < 0): get values
 * (action =-2): get values and zero out
 *
 * Should be called to set a constant-coefficient part of the matrix.
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixSetConstantValues( hypre_StructMatrix *matrix,
                                     HYPRE_Int           num_stencil_indices,
                                     HYPRE_Int          *stencil_indices,
                                     HYPRE_Complex      *values,
                                     HYPRE_Int           action )
{
   HYPRE_Int           *constant      = hypre_StructMatrixConstant(matrix);
   HYPRE_Int           *symm_elements = hypre_StructMatrixSymmElements(matrix);
   HYPRE_Complex       *matp;
   HYPRE_Int            j, s;
 
   /*-----------------------------------------------------------------------
    * Set the matrix coefficients
    *-----------------------------------------------------------------------*/

   for (s = 0; s < num_stencil_indices; s++)
   {
      j = stencil_indices[s];

      /* only set stored stencil values */
      if (symm_elements[j] < 0)
      {
         matp = hypre_StructMatrixBoxData(matrix, 0, j);
         if (!constant[j])
         {
            hypre_error(HYPRE_ERROR_GENERIC);
         }

         if (action > 0)
         {
            *matp += values[s];
         }
         else if (action > -1)
         {
            *matp = values[s];
         }
         else /* action < 0 */
         {
            values[s] = *matp;
            if (action == -2)
            {
               *matp = 0;
            }
         }
      }
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * (outside > 0): clear values possibly outside of the grid extents
 * (outside = 0): clear values only inside the grid extents
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixClearValues( hypre_StructMatrix *matrix,
                               hypre_Index         grid_index,
                               HYPRE_Int           num_stencil_indices,
                               HYPRE_Int          *stencil_indices,
                               HYPRE_Int           boxnum,
                               HYPRE_Int           outside )
{
   hypre_BoxArray      *grid_boxes;
   hypre_Box           *grid_box;
   hypre_BoxArray      *data_space;
   hypre_BoxArray      *data_boxes;
   hypre_Box           *data_box;

   HYPRE_Complex       *matp;

   HYPRE_Int            i, s, istart, istop;
 
   /*-----------------------------------------------------------------------
    * Initialize some things
    *-----------------------------------------------------------------------*/

   if (outside > 0)
   {
      grid_boxes = hypre_StructMatrixDataBoxes(matrix);
   }
   else
   {
      grid_boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(matrix));
   }
   data_boxes = hypre_StructMatrixDataBoxes(matrix);
   data_space = hypre_StructMatrixDataSpace(matrix);

   if (boxnum < 0)
   {
      istart = 0;
      istop  = hypre_BoxArraySize(grid_boxes);
   }
   else
   {
      istart = boxnum;
      istop  = istart + 1;
   }

   /*-----------------------------------------------------------------------
    * Clear the matrix coefficients
    *-----------------------------------------------------------------------*/

   for (i = istart; i < istop; i++)
   {
      grid_box = hypre_BoxArrayBox(grid_boxes, i);
      data_box = hypre_BoxArrayBox(data_boxes, i);

      if (hypre_IndexInBox(grid_index, grid_box))
      {
         hypre_StructMatrixMapDataIndex(matrix, grid_index);

         for (s = 0; s < num_stencil_indices; s++)
         {
            matp = hypre_StructMatrixBoxData(matrix, i, stencil_indices[s]) +
               hypre_BoxIndexRank(data_box, grid_index);
            *matp = 0.0;
         }
      }
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * (outside > 0): clear values possibly outside of the grid extents
 * (outside = 0): clear values only inside the grid extents
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixClearBoxValues( hypre_StructMatrix *matrix,
                                  hypre_Box          *clear_box,
                                  HYPRE_Int           num_stencil_indices,
                                  HYPRE_Int          *stencil_indices,
                                  HYPRE_Int           boxnum,
                                  HYPRE_Int           outside )
{
   hypre_BoxArray      *grid_boxes;
   hypre_Box           *grid_box;
   hypre_Box           *int_box;
                   
   HYPRE_Int           *symm_elements;
   hypre_BoxArray      *data_space;
   hypre_Box           *data_box;
   hypre_IndexRef       data_start;
   hypre_Index          data_stride;
   HYPRE_Int            datai;
   HYPRE_Complex       *datap;
                   
   hypre_Index          loop_size;
                   
   HYPRE_Int            i, s, istart, istop;

   /*-----------------------------------------------------------------------
    * Initialize some things
    *-----------------------------------------------------------------------*/

   if (outside > 0)
   {
      grid_boxes = hypre_StructMatrixDataBoxes(matrix);
   }
   else
   {
      grid_boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(matrix));
   }
   data_space = hypre_StructMatrixDataSpace(matrix);

   if (boxnum < 0)
   {
      istart = 0;
      istop  = hypre_BoxArraySize(grid_boxes);
   }
   else
   {
      istart = boxnum;
      istop  = istart + 1;
   }

   /*-----------------------------------------------------------------------
    * Clear the matrix coefficients
    *-----------------------------------------------------------------------*/

   hypre_SetIndex(data_stride, 1);

   symm_elements = hypre_StructMatrixSymmElements(matrix);

   int_box = hypre_BoxCreate(hypre_StructMatrixNDim(matrix));

   for (i = istart; i < istop; i++)
   {
      grid_box = hypre_BoxArrayBox(grid_boxes, i);
      data_box = hypre_BoxArrayBox(data_space, i);

      hypre_IntersectBoxes(clear_box, grid_box, int_box);

      /* if there was an intersection */
      if (hypre_BoxVolume(int_box))
      {
         hypre_StructMatrixMapDataBox(matrix, int_box);

         data_start = hypre_BoxIMin(int_box);

         for (s = 0; s < num_stencil_indices; s++)
         {
            /* only clear stencil entries that are explicitly stored */
            if (symm_elements[stencil_indices[s]] < 0)
            {
               datap = hypre_StructMatrixBoxData(matrix, i,
                                                 stencil_indices[s]);
               
               hypre_BoxGetSize(int_box, loop_size);
               
               hypre_BoxLoop1Begin(hypre_StructMatrixNDim(matrix), loop_size,
                                   data_box,data_start,data_stride,datai);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,datai) HYPRE_SMP_SCHEDULE
#endif
               hypre_BoxLoop1For(datai)
               {
                  datap[datai] = 0.0;
               }
               hypre_BoxLoop1End(datai);
            }
         }
      }
   }

   hypre_BoxDestroy(int_box);

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixAssemble( hypre_StructMatrix *matrix )
{
   HYPRE_Int              ndim         = hypre_StructMatrixNDim(matrix);
   HYPRE_Int              num_values   = hypre_StructMatrixNumValues(matrix);
   HYPRE_Complex         *matrix_vdata = hypre_StructMatrixVData(matrix);
   HYPRE_Int             *num_ghost    = hypre_StructMatrixNumGhost(matrix);
   HYPRE_Int              constant_coefficient;
   hypre_CommInfo        *comm_info;
   hypre_CommPkg         *comm_pkg;
   hypre_CommHandle      *comm_handle;

   /* BEGIN - variables for ghost layer identity code below */
   hypre_StructGrid      *grid;
   hypre_BoxArray        *boxes;
   hypre_BoxManager      *boxman;
   hypre_BoxArray        *data_space;
   hypre_BoxArrayArray   *boundary_boxes;
   hypre_BoxArray        *boundary_box_a;
   hypre_BoxArray        *entry_box_a;
   hypre_BoxArray        *tmp_box_a;
   hypre_Box             *data_box;
   hypre_Box             *boundary_box;
   hypre_Box             *entry_box;
   hypre_BoxManEntry    **entries;
   hypre_IndexRef         periodic;
   hypre_Index            loop_size;
   hypre_Index            index;
   hypre_IndexRef         start;
   hypre_Index            stride;
   HYPRE_Complex         *datap;
   HYPRE_Int              i, j, ei, datai;
   HYPRE_Int              num_entries;
   /* End - variables for ghost layer identity code below */

   constant_coefficient = hypre_StructMatrixConstantCoefficient( matrix );

   /*-----------------------------------------------------------------------
    * Set ghost zones along the domain boundary to the identity to enable code
    * simplifications elsewhere in hypre (e.g., CyclicReduction).
    *
    * Intersect each data box with the BoxMan to get neighbors, then subtract
    * the neighbors from the box to get the boundary boxes.
    *-----------------------------------------------------------------------*/

   /* RDF TODO: Rewrite to execute when diagonal coefficient is not constant, or
    * fix CyclicReduction to avoid division by zero along boundary. */
   if ( constant_coefficient!=1 )
   {
      data_space = hypre_StructMatrixDataSpace(matrix);
      grid       = hypre_StructMatrixGrid(matrix);
      boxes      = hypre_StructGridBoxes(grid);
      boxman     = hypre_StructGridBoxMan(grid);
      periodic   = hypre_StructGridPeriodic(grid);

      boundary_boxes = hypre_BoxArrayArrayCreate(
         hypre_BoxArraySize(data_space), ndim);
      entry_box_a    = hypre_BoxArrayCreate(0, ndim);
      tmp_box_a      = hypre_BoxArrayCreate(0, ndim);
      hypre_ForBoxI(i, data_space)
      {
         /* copy data box to boundary_box_a */
         boundary_box_a = hypre_BoxArrayArrayBoxArray(boundary_boxes, i);
         hypre_BoxArraySetSize(boundary_box_a, 1);
         boundary_box = hypre_BoxArrayBox(boundary_box_a, 0);
         hypre_CopyBox(hypre_BoxArrayBox(data_space, i), boundary_box);

         hypre_BoxManIntersect(boxman,
                               hypre_BoxIMin(boundary_box),
                               hypre_BoxIMax(boundary_box),
                               &entries , &num_entries);

         /* put neighbor boxes into entry_box_a */
         hypre_BoxArraySetSize(entry_box_a, num_entries);
         for (ei = 0; ei < num_entries; ei++)
         {
            entry_box = hypre_BoxArrayBox(entry_box_a, ei);
            hypre_BoxManEntryGetExtents(entries[ei],
                                        hypre_BoxIMin(entry_box),
                                        hypre_BoxIMax(entry_box));
         }
         hypre_TFree(entries);

         /* subtract neighbor boxes (entry_box_a) from data box (boundary_box_a) */
         hypre_SubtractBoxArrays(boundary_box_a, entry_box_a, tmp_box_a);
      }
      hypre_BoxArrayDestroy(entry_box_a);
      hypre_BoxArrayDestroy(tmp_box_a);

      /* set boundary ghost zones to the identity equation */

      hypre_SetIndex(index, 0);
      hypre_SetIndex(stride, 1);
      data_space = hypre_StructMatrixDataSpace(matrix);
      hypre_ForBoxI(i, data_space)
      {
         datap = hypre_StructMatrixExtractPointerByIndex(matrix, i, index);

         if (datap)
         {
            data_box = hypre_BoxArrayBox(data_space, i);
            boundary_box_a = hypre_BoxArrayArrayBoxArray(boundary_boxes, i);
            hypre_ForBoxI(j, boundary_box_a)
            {
               boundary_box = hypre_BoxArrayBox(boundary_box_a, j);
               start = hypre_BoxIMin(boundary_box);

               hypre_BoxGetSize(boundary_box, loop_size);

               hypre_BoxLoop1Begin(hypre_StructMatrixNDim(matrix), loop_size,
                                   data_box, start, stride, datai);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,datai) HYPRE_SMP_SCHEDULE
#endif
               hypre_BoxLoop1For(datai)
               {
                  datap[datai] = 1.0;
               }
               hypre_BoxLoop1End(datai);
            }
         }
      }

      hypre_BoxArrayArrayDestroy(boundary_boxes);
   }

   /*-----------------------------------------------------------------------
    * If the CommPkg has not been set up, set it up
    *-----------------------------------------------------------------------*/

   comm_pkg = hypre_StructMatrixCommPkg(matrix);

   if (!comm_pkg)
   {
      hypre_CreateCommInfoFromNumGhost(hypre_StructMatrixGrid(matrix),
                                       num_ghost, &comm_info);
      hypre_CommPkgCreate(comm_info,
                          hypre_StructMatrixDataSpace(matrix),
                          hypre_StructMatrixDataSpace(matrix),
                          num_values, NULL, 0,
                          hypre_StructMatrixComm(matrix), &comm_pkg);
      hypre_CommInfoDestroy(comm_info);

      hypre_StructMatrixCommPkg(matrix) = comm_pkg;
   }

   /*-----------------------------------------------------------------------
    * Update the ghost data
    * This takes care of the communication needs of all known functions
    * referencing the matrix.
    *
    * At present this is the only place where matrix data gets communicated.
    * However, comm_pkg is kept as long as the matrix is, in case some
    * future version hypre has a use for it - e.g. if the user replaces
    * a matrix with a very similar one, we may not want to recompute comm_pkg.
    *-----------------------------------------------------------------------*/

   if (constant_coefficient != 1)
   {
      hypre_InitializeCommunication(comm_pkg, matrix_vdata, matrix_vdata, 0, 0,
                                    &comm_handle);
      hypre_FinalizeCommunication(comm_handle);
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixSetNumGhost( hypre_StructMatrix *matrix,
                               HYPRE_Int          *num_ghost )
{
   HYPRE_Int  d, ndim = hypre_StructMatrixNDim(matrix);

   for (d = 0; d < ndim; d++)
   {
      hypre_StructMatrixNumGhost(matrix)[2*d]     = num_ghost[2*d];
      hypre_StructMatrixNumGhost(matrix)[2*d + 1] = num_ghost[2*d + 1];
   }

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * nentries - number of stencil entries
 * entries  - array of stencil entries
 *
 * The following three possibilites are supported for backward compatibility:
 * - no entries constant                 (constant_coefficient==0)
 * - all entries constant                (constant_coefficient==1)
 * - all but the diagonal entry constant (constant_coefficient==2)
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixSetConstantEntries( hypre_StructMatrix *matrix,
                                      HYPRE_Int           nentries,
                                      HYPRE_Int          *entries )
{
   hypre_StructStencil *stencil       = hypre_StructMatrixUserStencil(matrix);
   HYPRE_Int           *constant      = hypre_StructMatrixConstant(matrix);
   HYPRE_Int            stencil_size  = hypre_StructStencilSize(stencil);
   hypre_Index          diag_index;
   HYPRE_Int            constant_coefficient, diag_rank, i, j, nconst;

   /* By counting the nonzeros in constant, and by checking whether its diagonal
      entry is nonzero, we can distinguish between the three legal values of
      constant_coefficient, and detect input errors.  We do not need to treat
      duplicates in 'entries' as an error condition. */

   for (i = 0; i < nentries; i++)
   {
      constant[entries[i]] = 1;
   }
   nconst = 0;
   for (j = 0; j < stencil_size; j++)
   {
      nconst += constant[j];
   }

   if (nconst == 0)
   {
      constant_coefficient = 0;
   }
   else if (nconst >= stencil_size)
   {
      constant_coefficient = 1;
   }
   else
   {
      hypre_SetIndex(diag_index, 0);
      diag_rank = hypre_StructStencilElementRank(stencil, diag_index);
      if (constant[diag_rank] == 0)
      {
         constant_coefficient = 2;
         if (nconst != (stencil_size-1))
         {
            hypre_error(HYPRE_ERROR_GENERIC);
         }
      }
      else
      {
         constant_coefficient = 0;
         hypre_error(HYPRE_ERROR_GENERIC);
      }
   }

   hypre_StructMatrixConstantCoefficient(matrix) = constant_coefficient;

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixClearGhostValues( hypre_StructMatrix *matrix )
{
   HYPRE_Int             ndim = hypre_StructMatrixNDim(matrix);
                        
   HYPRE_Int             mi;
   HYPRE_Complex        *mp;

   hypre_StructStencil  *stencil;
   HYPRE_Int            *symm_elements;
   hypre_BoxArray       *grid_boxes;
   hypre_BoxArray       *data_space;
   hypre_Box            *data_box;
   hypre_Box            *grid_data_box;
   hypre_BoxArray       *diff_boxes;
   hypre_Box            *diff_box;
   hypre_Index           loop_size;
   hypre_IndexRef        start;
   hypre_Index           unit_stride;
                        
   HYPRE_Int             i, j, s;

   /*-----------------------------------------------------------------------
    * Set the matrix coefficients
    *-----------------------------------------------------------------------*/

   hypre_SetIndex(unit_stride, 1);
 
   grid_data_box = hypre_BoxCreate(ndim);

   stencil = hypre_StructMatrixStencil(matrix);
   symm_elements = hypre_StructMatrixSymmElements(matrix);
   grid_boxes = hypre_StructGridBoxes(hypre_StructMatrixGrid(matrix));
   data_space = hypre_StructMatrixDataBoxes(matrix);
   diff_boxes = hypre_BoxArrayCreate(0, ndim);
   hypre_ForBoxI(i, grid_boxes)
   {
      hypre_CopyBox(hypre_BoxArrayBox(grid_boxes, i), grid_data_box);
      data_box = hypre_BoxArrayBox(data_space, i);
      hypre_BoxArraySetSize(diff_boxes, 0);
      hypre_StructMatrixMapDataBox(matrix, grid_data_box);
      hypre_SubtractBoxes(data_box, grid_data_box, diff_boxes);

      for (s = 0; s < hypre_StructStencilSize(stencil); s++)
      {
         /* only clear stencil entries that are explicitly stored */
         if (symm_elements[s] < 0)
         {
            mp = hypre_StructMatrixBoxData(matrix, i, s);
            hypre_ForBoxI(j, diff_boxes)
            {
               diff_box = hypre_BoxArrayBox(diff_boxes, j);
               start = hypre_BoxIMin(diff_box);
                     
               hypre_BoxGetSize(diff_box, loop_size);
                     
               hypre_BoxLoop1Begin(ndim, loop_size,
                                   data_box, start, unit_stride, mi);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,mi ) HYPRE_SMP_SCHEDULE
#endif
               hypre_BoxLoop1For(mi)
               {
                  mp[mi] = 0.0;
               }
               hypre_BoxLoop1End(mi);
            }
         }
      }
   }
   hypre_BoxArrayDestroy(diff_boxes);
   hypre_BoxDestroy(grid_data_box);

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixPrint( const char         *filename,
                         hypre_StructMatrix *matrix,
                         HYPRE_Int           all      )
{
   FILE                 *file;
   char                  new_filename[255];

   hypre_StructGrid     *grid;
   hypre_BoxArray       *boxes;
   hypre_StructStencil  *stencil;
   hypre_Index          *stencil_shape;
   HYPRE_Int             stencil_size;
   hypre_BoxArray       *data_space;
   HYPRE_Int            *symm_elements, *value_ids, *cvalue_ids;
   HYPRE_Int             ndim, num_values, num_cvalues;
   HYPRE_Int             i, d, ci, vi;
   HYPRE_Int             myid;
   HYPRE_Complex         value;

   /*----------------------------------------
    * Open file
    *----------------------------------------*/

   hypre_MPI_Comm_rank(hypre_StructMatrixComm(matrix), &myid);

   hypre_sprintf(new_filename, "%s.%05d", filename, myid);
 
   if ((file = fopen(new_filename, "w")) == NULL)
   {
      hypre_printf("Error: can't open output file %s\n", new_filename);
      exit(1);
   }

   /*----------------------------------------
    * Print header info
    *----------------------------------------*/

   hypre_fprintf(file, "StructMatrix\n");

   hypre_fprintf(file, "\nSymmetric: %d\n", hypre_StructMatrixSymmetric(matrix));
   hypre_fprintf(file, "\nConstantCoefficient: %d\n",
                 hypre_StructMatrixConstantCoefficient(matrix));

   /* print grid info */
   hypre_fprintf(file, "\nGrid:\n");
   grid = hypre_StructMatrixGrid(matrix);
   hypre_StructGridPrint(file, grid);

   /* print stencil info */
   hypre_fprintf(file, "\nStencil:\n");
   stencil = hypre_StructMatrixStencil(matrix);
   stencil_shape = hypre_StructStencilShape(stencil);

   ndim = hypre_StructMatrixNDim(matrix);
   num_values = hypre_StructMatrixNumValues(matrix);
   num_cvalues = hypre_StructMatrixNumCValues(matrix);
   hypre_fprintf(file, "%d\n", (num_values + num_cvalues));

   vi = 0;
   ci = 0;
   value_ids = hypre_TAlloc(HYPRE_Int, num_values);
   cvalue_ids = hypre_TAlloc(HYPRE_Int, num_cvalues);
   symm_elements = hypre_StructMatrixSymmElements(matrix);
   stencil_size = hypre_StructStencilSize(stencil);
   for (i = 0; i < stencil_size; i++)
   {
      if (symm_elements[i] < 0)
      {
         /* Print line of the form: "%d: %d %d %d\n" */
         hypre_fprintf(file, "%d:", i);
         for (d = 0; d < ndim; d++)
         {
            hypre_fprintf(file, " %d", hypre_IndexD(stencil_shape[i], d));
         }
         hypre_fprintf(file, "\n");

         if (hypre_StructMatrixConstEntry(matrix, i))
         {
            cvalue_ids[ci++] = i;
         }
         else
         {
            value_ids[vi++] = i;
         }
      }
   }

   /*----------------------------------------
    * Print data
    *----------------------------------------*/

   data_space = hypre_StructMatrixDataSpace(matrix);
 
   if (all)
   {
      boxes = data_space;
   }
   else
   {
      boxes = hypre_StructGridBoxes(grid);
   }
 
   hypre_fprintf(file, "\nConstant Data:\n");
   if (hypre_StructMatrixDataSize(matrix) > 0)
   {
      for (ci = 0; ci < num_cvalues; ci++)
      {
         i = cvalue_ids[ci];
         value = *hypre_StructMatrixBoxData(matrix, 0, i);
#ifdef HYPRE_COMPLEX
         hypre_fprintf(file, "*: (*; %d) %.14e, %.14e\n",
                       i, hypre_creal(value), hypre_cimag(value));
#else
         hypre_fprintf(file, "*: (*; %d) %.14e\n", i, value);
#endif
      }
   }

   hypre_fprintf(file, "\nData:\n");
   hypre_PrintBoxArrayData(file, boxes, data_space, num_values, value_ids, ndim,
                           hypre_StructMatrixVData(matrix));

   /*----------------------------------------
    * Clean up
    *----------------------------------------*/

   hypre_TFree(value_ids);
   hypre_TFree(cvalue_ids);
 
   fflush(file);
   fclose(file);

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 * RDF TODO: Fix this to use new matrix structure
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_StructMatrixMigrate( hypre_StructMatrix *from_matrix,
                           hypre_StructMatrix *to_matrix   )
{
   hypre_CommInfo        *comm_info;
   hypre_CommPkg         *comm_pkg;
   hypre_CommHandle      *comm_handle;

   HYPRE_Int              constant_coefficient, comm_num_values;
   HYPRE_Int              stencil_size, mat_num_values;
   hypre_StructStencil   *stencil;
   HYPRE_Int              data_initial_offset = 0;
   HYPRE_Complex         *matrix_data_from = hypre_StructMatrixData(from_matrix);
   HYPRE_Complex         *matrix_data_to = hypre_StructMatrixData(to_matrix);
   HYPRE_Complex         *matrix_data_comm_from = matrix_data_from;
   HYPRE_Complex         *matrix_data_comm_to = matrix_data_to;

   /*------------------------------------------------------
    * Set up hypre_CommPkg
    *------------------------------------------------------*/

   constant_coefficient = hypre_StructMatrixConstantCoefficient( from_matrix );
   hypre_assert( constant_coefficient == hypre_StructMatrixConstantCoefficient( to_matrix ) );

   mat_num_values = hypre_StructMatrixNumValues(from_matrix);
   hypre_assert( mat_num_values = hypre_StructMatrixNumValues(to_matrix) );

   if ( constant_coefficient==0 ) 
   {
      comm_num_values = mat_num_values;
   }
   else if ( constant_coefficient==1 ) 
   {
      comm_num_values = 0;
   }
   else /* constant_coefficient==2 */ 
   {
      comm_num_values = 1;
      stencil = hypre_StructMatrixStencil(from_matrix);
      stencil_size = hypre_StructStencilSize(stencil);
      hypre_assert(stencil_size ==
                   hypre_StructStencilSize( hypre_StructMatrixStencil(to_matrix) ) );
      data_initial_offset = stencil_size;
      matrix_data_comm_from = &( matrix_data_from[data_initial_offset] );
      matrix_data_comm_to = &( matrix_data_to[data_initial_offset] );
   }

   hypre_CreateCommInfoFromGrids(hypre_StructMatrixGrid(from_matrix),
                                 hypre_StructMatrixGrid(to_matrix),
                                 &comm_info);
   hypre_CommPkgCreate(comm_info,
                       hypre_StructMatrixDataSpace(from_matrix),
                       hypre_StructMatrixDataSpace(to_matrix),
                       comm_num_values, NULL, 0,
                       hypre_StructMatrixComm(from_matrix), &comm_pkg);
   hypre_CommInfoDestroy(comm_info);
   /* is this correct for periodic? */

   /*-----------------------------------------------------------------------
    * Migrate the matrix data
    *-----------------------------------------------------------------------*/
 
   if ( constant_coefficient!=1 )
   {
      hypre_InitializeCommunication( comm_pkg,
                                     matrix_data_comm_from,
                                     matrix_data_comm_to, 0, 0,
                                     &comm_handle );
      hypre_FinalizeCommunication( comm_handle );
   }
   hypre_CommPkgDestroy(comm_pkg);

   return hypre_error_flag;
}

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------*/

hypre_StructMatrix *
hypre_StructMatrixRead( MPI_Comm    comm,
                        const char *filename,
                        HYPRE_Int  *num_ghost )
{
   FILE                 *file;
   char                  new_filename[255];
                      
   hypre_StructMatrix   *matrix;

   hypre_StructGrid     *grid;
   hypre_BoxArray       *boxes;
   HYPRE_Int             ndim;

   hypre_StructStencil  *stencil;
   hypre_Index          *stencil_shape;
   HYPRE_Int             stencil_size, real_stencil_size;

   HYPRE_Int             num_values;

   hypre_BoxArray       *data_space;

   HYPRE_Int             symmetric;
   HYPRE_Int             constant_coefficient;
                       
   HYPRE_Int             i, d, idummy;
                       
   HYPRE_Int             myid;

   /*----------------------------------------
    * Open file
    *----------------------------------------*/

   hypre_MPI_Comm_rank(comm, &myid );

   hypre_sprintf(new_filename, "%s.%05d", filename, myid);
 
   if ((file = fopen(new_filename, "r")) == NULL)
   {
      hypre_printf("Error: can't open output file %s\n", new_filename);
      exit(1);
   }

   /*----------------------------------------
    * Read header info
    *----------------------------------------*/

   hypre_fscanf(file, "StructMatrix\n");

   hypre_fscanf(file, "\nSymmetric: %d\n", &symmetric);
   hypre_fscanf(file, "\nConstantCoefficient: %d\n", &constant_coefficient);

   /* read grid info */
   hypre_fscanf(file, "\nGrid:\n");
   hypre_StructGridRead(comm,file,&grid);

   /* read stencil info */
   hypre_fscanf(file, "\nStencil:\n");
   ndim = hypre_StructGridNDim(grid);
   hypre_fscanf(file, "%d\n", &stencil_size);
   if (symmetric) { real_stencil_size = 2*stencil_size-1; }
   else { real_stencil_size = stencil_size; }
   /* ... real_stencil_size is the stencil size of the matrix after it's fixed up
      by the call (if any) of hypre_StructStencilSymmetrize from
      hypre_StructMatrixInitializeShell.*/
   stencil_shape = hypre_CTAlloc(hypre_Index, stencil_size);
   for (i = 0; i < stencil_size; i++)
   {
      /* Read line of the form: "%d: %d %d %d\n" */
      hypre_fscanf(file, "%d:", &idummy);
      for (d = 0; d < ndim; d++)
      {
         hypre_fscanf(file, " %d", &hypre_IndexD(stencil_shape[i], d));
      }
      hypre_fscanf(file, "\n");
   }
   stencil = hypre_StructStencilCreate(ndim, stencil_size, stencil_shape);

   /*----------------------------------------
    * Initialize the matrix
    *----------------------------------------*/

   matrix = hypre_StructMatrixCreate(comm, grid, stencil);
   hypre_StructMatrixSymmetric(matrix) = symmetric;
   hypre_StructMatrixConstantCoefficient(matrix) = constant_coefficient;
   hypre_StructMatrixSetNumGhost(matrix, num_ghost);
   hypre_StructMatrixInitialize(matrix);

   /*----------------------------------------
    * Read data
    *----------------------------------------*/

   boxes      = hypre_StructGridBoxes(grid);
   data_space = hypre_StructMatrixDataSpace(matrix);
   num_values = hypre_StructMatrixNumValues(matrix);
 
   hypre_fscanf(file, "\nData:\n");
   if ( constant_coefficient==0 )
   {
      hypre_ReadBoxArrayData(file, boxes, data_space, num_values,
                             hypre_StructGridNDim(grid),
                             hypre_StructMatrixData(matrix));
   }
   else
   {
      hypre_assert( constant_coefficient<=2 );
      hypre_ReadBoxArrayData_CC( file, boxes, data_space,
                                 stencil_size, real_stencil_size,
                                 constant_coefficient,
                                 hypre_StructGridNDim(grid),
                                 hypre_StructMatrixData(matrix));
   }

   /*----------------------------------------
    * Assemble the matrix
    *----------------------------------------*/

   hypre_StructMatrixAssemble(matrix);

   /*----------------------------------------
    * Close file
    *----------------------------------------*/
 
   fclose(file);

   return matrix;
}
/*--------------------------------------------------------------------------
 * clears matrix stencil coefficients reaching outside of the physical boundaries
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_StructMatrixClearBoundary( hypre_StructMatrix *matrix)
{
   HYPRE_Int            ndim = hypre_StructMatrixNDim(matrix);
   HYPRE_Complex       *data;
   hypre_BoxArray      *grid_boxes;
   hypre_BoxArray      *data_space;
   /*hypre_Box           *box;*/
   hypre_Box           *grid_box;
   hypre_Box           *data_box;
   hypre_Box           *tmp_box;
   hypre_Index         *shape;
   hypre_Index          stencil_element;
   hypre_Index          loop_size;
   hypre_IndexRef       start;
   hypre_Index          stride;
   hypre_StructGrid    *grid;
   hypre_StructStencil *stencil;
   hypre_BoxArray      *boundary;

   HYPRE_Int           i, i2, ixyz, j;

   /*-----------------------------------------------------------------------
    * Set the matrix coefficients
    *-----------------------------------------------------------------------*/

   grid = hypre_StructMatrixGrid(matrix);
   stencil = hypre_StructMatrixStencil(matrix);
   grid_boxes = hypre_StructGridBoxes(grid);
   ndim = hypre_StructStencilNDim(stencil);
   data_space = hypre_StructMatrixDataSpace(matrix);
   hypre_SetIndex(stride, 1);
   shape = hypre_StructStencilShape(stencil);

   for (j = 0; j < hypre_StructStencilSize(stencil); j++)
   {
      hypre_CopyIndex(shape[j],stencil_element);
      if (!hypre_IndexEqual(stencil_element, 0, ndim))
      {
         hypre_ForBoxI(i, grid_boxes)
         {
            grid_box = hypre_BoxArrayBox(grid_boxes, i);
            data_box = hypre_BoxArrayBox(data_space, i);
            boundary = hypre_BoxArrayCreate( 0, ndim );
            hypre_GeneralBoxBoundaryIntersect(grid_box, grid, stencil_element,
                boundary);
            data = hypre_StructMatrixBoxData(matrix, i, j);
            hypre_ForBoxI(i2, boundary)
            {
               tmp_box = hypre_BoxArrayBox(boundary, i2);
               hypre_StructMatrixMapDataBox(matrix, tmp_box);
               hypre_BoxGetSize(tmp_box, loop_size);
               start = hypre_BoxIMin(tmp_box);
               hypre_BoxLoop1Begin(ndim, loop_size, data_box, start, stride, ixyz);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(HYPRE_BOX_PRIVATE,ixyz) HYPRE_SMP_SCHEDULE
#endif
               hypre_BoxLoop1For(ixyz)
               {
                  data[ixyz] = 0.0;
               }
               hypre_BoxLoop1End(ixyz);
            }
            hypre_BoxArrayDestroy(boundary);
         }
      }
   }

   return hypre_error_flag;
}


