/*============================================================================
 * Manage the (generic) evaluation of extended definitions
 *============================================================================*/

/*
  This file is part of Code_Saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2017 EDF S.A.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
  Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*----------------------------------------------------------------------------*/

#include "cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include <bft_mem.h>

#include "cs_defs.h"
#include "cs_xdef.h"
#include "cs_mesh_location.h"
#include "cs_reco.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_xdef_eval.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Local Macro definitions and structure definitions
 *============================================================================*/

/*============================================================================
 * Private function prototypes
 *============================================================================*/

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a scalar-valued quantity for a list of elements
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_scalar_by_val(cs_lnum_t                    n_elts,
                           const cs_lnum_t             *elt_ids,
                           bool                         compact,
                           const cs_mesh_t             *mesh,
                           const cs_cdo_connect_t      *connect,
                           const cs_cdo_quantities_t   *quant,
                           const cs_time_step_t        *ts,
                           void                        *input,
                           cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(quant);
  CS_UNUSED(connect);
  CS_UNUSED(ts);
  assert(eval != NULL);

  const cs_real_t  *constant_val = (cs_real_t *)input;

  if (elt_ids != NULL && !compact) {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++)
      eval[elt_ids[i]] = constant_val[0];

  }
  else {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++)
      eval[i] = constant_val[0];

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a scalar-valued quantity by a cellwise process
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_scalar_by_val(const cs_cell_mesh_t     *cm,
                              const cs_time_step_t     *ts,
                              void                     *input,
                              cs_real_t                *eval)
{
  CS_UNUSED(cm);
  CS_UNUSED(ts);

  cs_real_t  *constant_val = (cs_real_t *)input;
  *eval = constant_val[0];
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a vector-valued quantity for a list of elements
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_vector_by_val(cs_lnum_t                    n_elts,
                           const cs_lnum_t             *elt_ids,
                           bool                         compact,
                           const cs_mesh_t             *mesh,
                           const cs_cdo_connect_t      *connect,
                           const cs_cdo_quantities_t   *quant,
                           const cs_time_step_t        *ts,
                           void                        *input,
                           cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(quant);
  CS_UNUSED(connect);
  CS_UNUSED(ts);
  assert(eval != NULL);

  const cs_real_t  *constant_val = (cs_real_t *)input;

  if (elt_ids != NULL && !compact) {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      const cs_lnum_t  id = elt_ids[i];
      eval[3*id  ] = constant_val[0];
      eval[3*id+1] = constant_val[1];
      eval[3*id+2] = constant_val[2];
    }

  }
  else {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      eval[3*i  ] = constant_val[0];
      eval[3*i+1] = constant_val[1];
      eval[3*i+2] = constant_val[2];
    }

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a vector-valued quantity by a cellwise process
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_vector_by_val(const cs_cell_mesh_t     *cm,
                              const cs_time_step_t     *ts,
                              void                     *input,
                              cs_real_t                *eval)
{
  CS_UNUSED(cm);
  CS_UNUSED(ts);

  const cs_real_t  *constant_val = (cs_real_t *)input;

  eval[0] = constant_val[0];
  eval[1] = constant_val[1];
  eval[2] = constant_val[2];
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a tensor-valued quantity for a list of elements
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_tensor_by_val(cs_lnum_t                    n_elts,
                           const cs_lnum_t             *elt_ids,
                           bool                         compact,
                           const cs_mesh_t             *mesh,
                           const cs_cdo_connect_t      *connect,
                           const cs_cdo_quantities_t   *quant,
                           const cs_time_step_t        *ts,
                           void                        *input,
                           cs_real_t                   *eval)
{
  CS_UNUSED(quant);
  CS_UNUSED(mesh);
  CS_UNUSED(connect);
  CS_UNUSED(ts);
  assert(eval != NULL);

  const cs_real_3_t  *constant_val = (const cs_real_3_t *)input;

  if (elt_ids != NULL && !compact) {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {

      const cs_lnum_t  id = elt_ids[i];
      cs_real_t  *shift_eval = eval + 9*id;
      for (int ki = 0; ki < 3; ki++)
        for (int kj = 0; kj < 3; kj++)
          shift_eval[3*ki+kj] = constant_val[ki][kj];

    }

  }
  else {

#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {

      cs_real_t  *shift_eval = eval + 9*i;
      for (int ki = 0; ki < 3; ki++)
        for (int kj = 0; kj < 3; kj++)
          shift_eval[3*ki+kj] = constant_val[ki][kj];

    }

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a tensor-valued quantity by a cellwise process
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_tensor_by_val(const cs_cell_mesh_t     *cm,
                              const cs_time_step_t     *ts,
                              void                     *input,
                              cs_real_t                *eval)
{
  CS_UNUSED(cm);
  CS_UNUSED(ts);

  const cs_real_3_t  *constant_val = (const cs_real_3_t *)input;
  for (int ki = 0; ki < 3; ki++)
    for (int kj = 0; kj < 3; kj++)
      eval[3*ki+kj] = constant_val[ki][kj];
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity defined at cells using an analytic function
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_at_cells_by_analytic(cs_lnum_t                    n_elts,
                                  const cs_lnum_t             *elt_ids,
                                  bool                         compact,
                                  const cs_mesh_t             *mesh,
                                  const cs_cdo_connect_t      *connect,
                                  const cs_cdo_quantities_t   *quant,
                                  const cs_time_step_t        *ts,
                                  void                        *input,
                                  cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(connect);

  cs_analytic_func_t *ana = (cs_analytic_func_t *)input;

  /* Evaluate the property for this time at the cell center */
  ana(ts->t_cur, n_elts, elt_ids, quant->cell_centers, compact, eval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity defined at vertices using an array
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_at_vertices_by_array(cs_lnum_t                    n_elts,
                                  const cs_lnum_t             *elt_ids,
                                  bool                         compact,
                                  const cs_mesh_t             *mesh,
                                  const cs_cdo_connect_t      *connect,
                                  const cs_cdo_quantities_t   *quant,
                                  const cs_time_step_t        *ts,
                                  void                        *input,
                                  cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(connect);
  CS_UNUSED(quant);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  assert(array_input->stride == 1); // other cases not managed up to now

  if (cs_test_flag(array_input->loc, cs_cdo_primal_vtx)) {

    if (elt_ids != NULL && !compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  v_id = elt_ids[i];
        eval[v_id] = array_input->values[v_id];
      }

    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++)
        eval[i] = array_input->values[elt_ids[i]];

    }
    else {

      assert(elt_ids == NULL);
      memcpy(eval, (const cs_real_t *)array_input->values, n_elts);

    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity defined at vertices using an analytic function
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_at_vertices_by_analytic(cs_lnum_t                    n_elts,
                                     const cs_lnum_t             *elt_ids,
                                     bool                         compact,
                                     const cs_mesh_t             *mesh,
                                     const cs_cdo_connect_t      *connect,
                                     const cs_cdo_quantities_t   *quant,
                                     const cs_time_step_t        *ts,
                                     void                        *input,
                                     cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(connect);

  cs_analytic_func_t *ana = (cs_analytic_func_t *)input;

  /* Evaluate the property for this time at the cell center */
  ana(ts->t_cur, n_elts, elt_ids, quant->vtx_coord, compact, eval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity defined using an analytic function by a
 *         cellwise process (usage of a cs_cell_mesh_t structure)
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_cell_by_analytic(const cs_cell_mesh_t       *cm,
                                 const cs_time_step_t       *ts,
                                 void                       *input,
                                 cs_real_t                  *eval)
{
  cs_analytic_func_t *ana = (cs_analytic_func_t *)input;

  /* Evaluate the property for this time at the cell center */
  ana(ts->t_cur, 1, NULL, cm->xc, true, eval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a scalar-valued quantity at cells defined by an array.
 *         Array is assumed to be interlaced.
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_scalar_at_cells_by_array(cs_lnum_t                    n_elts,
                                      const cs_lnum_t             *elt_ids,
                                      bool                         compact,
                                      const cs_mesh_t             *mesh,
                                      const cs_cdo_connect_t      *connect,
                                      const cs_cdo_quantities_t   *quant,
                                      const cs_time_step_t        *ts,
                                      void                        *input,
                                      cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(ts);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  assert(array_input->stride == 1);

  if ((array_input->loc & cs_cdo_primal_cell) == cs_cdo_primal_cell) {

    if (elt_ids != NULL && !compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        eval[c_id] = array_input->values[c_id];
      }

    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++)
        eval[i] = array_input->values[elt_ids[i]];

    }
    else {

      assert(elt_ids == NULL);
      memcpy(eval, (const cs_real_t *)array_input->values, n_elts);

    }

  }
  else if ((array_input->loc & cs_cdo_primal_vtx) == cs_cdo_primal_vtx) {

    if (elt_ids != NULL && !compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        cs_reco_pv_at_cell_center(c_id,
                                  connect->c2v,
                                  quant,
                                  array_input->values,
                                  eval + c_id);
      }

    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++)
        cs_reco_pv_at_cell_center(elt_ids[i],
                                  connect->c2v,
                                  quant,
                                  array_input->values,
                                  eval + i);

    }
    else {

      assert(elt_ids == NULL);
      for (cs_lnum_t i = 0; i < n_elts; i++)
        cs_reco_pv_at_cell_center(i,
                                  connect->c2v,
                                  quant,
                                  array_input->values,
                                  eval + i);

    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a nd-valued quantity at cells defined by an array.
 *         Array is assumed to be interlaced.
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_nd_at_cells_by_array(cs_lnum_t                    n_elts,
                                  const cs_lnum_t             *elt_ids,
                                  bool                         compact,
                                  const cs_mesh_t             *mesh,
                                  const cs_cdo_connect_t      *connect,
                                  const cs_cdo_quantities_t   *quant,
                                  const cs_time_step_t        *ts,
                                  void                        *input,
                                  cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(ts);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  const int  stride = array_input->stride;
  assert(stride > 1);

  if (cs_test_flag(array_input->loc, cs_cdo_primal_cell)) {

    if (elt_ids != NULL && !compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        for (int k = 0; k < stride; k++)
          eval[stride*c_id + k] = array_input->values[stride*c_id + k];
      }

    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        for (int k = 0; k < stride; k++)
          eval[stride*i + k] = array_input->values[stride*c_id + k];
      }

    }
    else {

      assert(elt_ids == NULL);
      memcpy(eval, (const cs_real_t *)array_input->values, stride*n_elts);

    }

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_dual_face_byc)) {

    assert(stride == 3);
    assert(array_input->index == connect->c2e->idx);

    if (elt_ids != NULL && !compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        cs_reco_dfbyc_at_cell_center(c_id,
                                     connect->c2e,
                                     quant,
                                     array_input->values,
                                     eval + c_id*stride);
      }

    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++)
        cs_reco_dfbyc_at_cell_center(elt_ids[i],
                                     connect->c2e,
                                     quant,
                                     array_input->values,
                                     eval + i*stride);

    }
    else {

      for (cs_lnum_t i = 0; i < n_elts; i++)
        cs_reco_dfbyc_at_cell_center(i,
                                     connect->c2e,
                                     quant,
                                     array_input->values,
                                     eval + i*stride);

    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid case for the input array", __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a vector-valued quantity at all vertices defined by an
 *         array.
 *         Array is assumed to be interlaced.
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_3_at_all_vertices_by_array(cs_lnum_t                   n_elts,
                                        const cs_lnum_t            *elt_ids,
                                        bool                        compact,
                                        const cs_mesh_t            *mesh,
                                        const cs_cdo_connect_t     *connect,
                                        const cs_cdo_quantities_t  *quant,
                                        const cs_time_step_t       *ts,
                                        void                       *input,
                                        cs_real_t                  *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(ts);
  CS_UNUSED(compact);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  const int  stride = array_input->stride;

  if (elt_ids != NULL || n_elts < quant->n_vertices)
    bft_error(__FILE__, __LINE__, 0, " %s: Invalid case\n", __func__);

  double  *dc_vol = NULL;
  BFT_MALLOC(dc_vol, quant->n_vertices, double);

# pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
  for (cs_lnum_t i = 0; i < quant->n_vertices; i++)
    dc_vol[i] = 0;

  if (cs_test_flag(array_input->loc, cs_cdo_primal_cell)) {

    assert(stride == 3);
    for (cs_lnum_t c_id = 0; c_id < quant->n_cells; c_id++) {

      /* Retrieve the cell vector */
      cs_real_3_t  cell_vector;
      for (int k = 0; k < stride; k++)
        cell_vector[k] = array_input->values[stride*c_id + k];

      /* Interpolate with a weighting related to the vertex volume in each
         cell */
      const cs_lnum_t  *c2v_idx = connect->c2v->idx + c_id;
      const cs_lnum_t  *c2v_ids = connect->c2v->ids + c2v_idx[0];
      const double  *vol_vc = quant->dcell_vol + c2v_idx[0];

      for (short int v = 0; v < c2v_idx[1]-c2v_idx[0]; v++) {

        const cs_lnum_t  v_id = c2v_ids[v];

        dc_vol[v_id] += vol_vc[v];
        cs_real_t  *v_val = eval + 3*v_id;
        for (int k = 0; k < 3; k++) v_val[k] += vol_vc[v] * cell_vector[k];

      } // Loop on cell vertices

    } // Loop on cells

#   pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++) {

      const double  inv_dcvol = 1/dc_vol[v_id];
      cs_real_t *v_val = eval + 3*v_id;
      for (int k = 0; k < 3; k++) v_val[k] *= inv_dcvol;

    } // Loop on vertices

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_dual_face_byc)) {

    for (cs_lnum_t c_id = 0; c_id < quant->n_cells; c_id++) {

      /* Compute a estimated cell vector */
      cs_real_3_t  cell_vector;
      cs_reco_dfbyc_at_cell_center(c_id,
                                   connect->c2e,
                                   quant,
                                   array_input->values,
                                   cell_vector);

      /* Interpolate with a weighting related to the vertex volume in each
         cell */
      const cs_lnum_t  *c2v_idx = connect->c2v->idx + c_id;
      const cs_lnum_t  *c2v_ids = connect->c2v->ids + c2v_idx[0];
      const double  *vol_vc = quant->dcell_vol + c2v_idx[0];

      for (short int v = 0; v < c2v_idx[1]-c2v_idx[0]; v++) {

        const cs_lnum_t  v_id = c2v_ids[v];

        dc_vol[v_id] += vol_vc[v];
        cs_real_t  *v_val = eval + 3*v_id;
        for (int k = 0; k < 3; k++) v_val[k] += vol_vc[v] * cell_vector[k];

      } // Loop on cell vertices

    } // Loop on cells

#   pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++) {

      const double  inv_dcvol = 1/dc_vol[v_id];
      cs_real_t *v_val = eval + 3*v_id;
      for (int k = 0; k < 3; k++) v_val[k] *= inv_dcvol;

    } // Loop on vertices

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid case for the input array", __func__);

  /* Free temporary buffer */
  BFT_FREE(dc_vol);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity at cells defined by an array.
 *         Array is assumed to be interlaced.
 *         Variation using a cs_cell_mesh_t structure
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_cell_by_array(const cs_cell_mesh_t      *cm,
                              const cs_time_step_t      *ts,
                              void                      *input,
                              cs_real_t                 *eval)
{
  CS_UNUSED(ts);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  const int  stride = array_input->stride;

  /* array is assumed to be interlaced */
  if (cs_test_flag(array_input->loc, cs_cdo_primal_cell)) {

    for (int k = 0; k < stride; k++)
      eval[k] = array_input->values[stride*cm->c_id + k];

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_primal_vtx)) {

    /* Sanity checks */
    assert(cs_test_flag(cm->flag, CS_CDO_LOCAL_PVQ));

    /* Reconstruct (or interpolate) value at the current cell center */
    for (short int v = 0; v < cm->n_vc; v++) {
      for (int k = 0; k < stride; k++)
        eval[k] += cm->wvc[v] * array_input->values[stride*cm->v_ids[v] + k];
    }

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_dual_face_byc)) {

    assert(array_input->index != NULL);

    /* Reconstruct (or interpolate) value at the current cell center */
    cs_reco_dfbyc_in_cell(cm,
                          array_input->values + array_input->index[cm->c_id],
                          eval);

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity inside a cell defined using a field
 *
 * \param[in]  n_elts    number of elements to consider
 * \param[in]  elt_ids   list of element ids
 * \param[in]  compact   true:no indirection, false:indirection for output
 * \param[in]  mesh      pointer to a cs_mesh_t structure
 * \param[in]  connect   pointer to a cs_cdo_connect_t structure
 * \param[in]  quant     pointer to a cs_cdo_quantities_t structure
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cell_by_field(cs_lnum_t                    n_elts,
                           const cs_lnum_t             *elt_ids,
                           bool                         compact,
                           const cs_mesh_t             *mesh,
                           const cs_cdo_connect_t      *connect,
                           const cs_cdo_quantities_t   *quant,
                           const cs_time_step_t        *ts,
                           void                        *input,
                           cs_real_t                   *eval)
{
  CS_UNUSED(mesh);
  CS_UNUSED(ts);

  cs_field_t  *field = (cs_field_t *)input;
  assert(field != NULL);
  cs_real_t  *values = field->val;
  int  c_ml_id = cs_mesh_location_get_id_by_name(N_("cells"));
  int  v_ml_id = cs_mesh_location_get_id_by_name(N_("vertices"));

  if (field->location_id == c_ml_id) {

    if (elt_ids != NULL && !compact) {
      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        for (int k = 0; k < field->dim; k++)
          eval[field->dim*c_id + k] = values[field->dim*c_id + k];
      }
    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {
        const cs_lnum_t  c_id = elt_ids[i];
        for (int k = 0; k < field->dim; k++)
          eval[field->dim*i + k] = values[field->dim*c_id + k];
      }

    }
    else {

      assert(elt_ids == NULL);
      memcpy(eval, (const cs_real_t *)values, field->dim*n_elts);

    }

  }
  else if (field->location_id == v_ml_id) {

    assert(field->dim == 1);
    if (elt_ids != NULL && !compact) {
      for (cs_lnum_t i = 0; i < n_elts; i++) {

        const cs_lnum_t  c_id = elt_ids[i];
        cs_reco_pv_at_cell_center(c_id,
                                  connect->c2v,
                                  quant,
                                  values,
                                  eval + c_id);

      }
    }
    else if (elt_ids != NULL && compact) {

      for (cs_lnum_t i = 0; i < n_elts; i++) {

        const cs_lnum_t  c_id = elt_ids[i];
        cs_reco_pv_at_cell_center(c_id,
                                  connect->c2v,
                                  quant,
                                  values,
                                  eval + i);

      }

    }
    else {

      assert(elt_ids == NULL);
      for (cs_lnum_t i = 0; i < n_elts; i++) {
        cs_reco_pv_at_cell_center(i,
                                  connect->c2v,
                                  quant,
                                  values,
                                  eval + i);

      }

    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid case for the input array", __func__);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate a quantity inside a cell defined using a field
 *         Variation using a cs_cell_mesh_t structure
 *
 * \param[in]  cm       pointer to a cs_cell_mesh_t structure
 * \param[in]  ts       pointer to a cs_time_step_t structure
 * \param[in]  input    pointer to an input structure
 * \param[out] eval     value of the property at the cell center
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_cell_by_field(const cs_cell_mesh_t        *cm,
                              const cs_time_step_t        *ts,
                              void                        *input,
                              cs_real_t                   *eval)
{
  CS_UNUSED(ts);

  cs_field_t  *field = (cs_field_t *)input;
  assert(field != NULL);
  cs_real_t  *values = field->val;
  int  c_ml_id = cs_mesh_location_get_id_by_name(N_("cells"));
  int  v_ml_id = cs_mesh_location_get_id_by_name(N_("vertices"));

  if (field->location_id == c_ml_id) {

    for (int k = 0; k < field->dim; k++)
      eval[k] = values[field->dim*cm->c_id + k];

  }
  else if (field->location_id == v_ml_id) {

    /* Sanity checks */
    assert(field->dim == 1);
    assert(cs_test_flag(cm->flag, CS_CDO_LOCAL_PVQ));

    /* Reconstruct (or interpolate) value at the current cell center */
    for (short int v = 0; v < cm->n_vc; v++)
      eval[0] += cm->wvc[v] * values[cm->v_ids[v]];


  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer for evaluating a quantity defined by analytic
 *         function at a precise location inside a cell
 *         Use of a cs_cell_mesh_t structure.
 *
 * \param[in]  cm        pointer to a cs_cell_mesh_t structure
 * \param[in]  n_points  number of points where to compute the evaluation
 * \param[in]  xyz       where to compute the evaluation
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_at_xyz_by_analytic(const cs_cell_mesh_t       *cm,
                                   cs_lnum_t                   n_points,
                                   const cs_real_t            *xyz,
                                   const cs_time_step_t       *ts,
                                   void                       *input,
                                   cs_real_t                  *eval)
{
  CS_UNUSED(cm);

  cs_analytic_func_t *ana = (cs_analytic_func_t *)input;

  /* Evaluate the property for this time at the cell center */
  ana(ts->t_cur, n_points, NULL, xyz, true, eval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer for evaluating a quantity defined by analytic
 *         function at a precise location inside a cell
 *         Use of a cs_cell_mesh_t structure.
 *
 * \param[in]  cm        pointer to a cs_cell_mesh_t structure
 * \param[in]  n_points  number of points where to compute the evaluation
 * \param[in]  xyz       where to compute the evaluation
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_vector_at_xyz_by_val(const cs_cell_mesh_t       *cm,
                                     cs_lnum_t                   n_points,
                                     const cs_real_t            *xyz,
                                     const cs_time_step_t       *ts,
                                     void                       *input,
                                     cs_real_t                  *eval)
{
  CS_UNUSED(cm);
  CS_UNUSED(xyz);
  CS_UNUSED(ts);

  const cs_real_t  *constant_val = (cs_real_t *)input;

  for (int i = 0; i < n_points; i++) {
    eval[3*i    ] = constant_val[0];
    eval[3*i + 1] = constant_val[1];
    eval[2*i + 2] = constant_val[2];
  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer for evaluating a quantity defined by analytic
 *         function at a precise location inside a cell
 *         Use of a cs_cell_mesh_t structure.
 *
 * \param[in]  cm        pointer to a cs_cell_mesh_t structure
 * \param[in]  n_points  number of points where to compute the evaluation
 * \param[in]  xyz       where to compute the evaluation
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_3_at_xyz_by_array(const cs_cell_mesh_t       *cm,
                                  cs_lnum_t                   n_points,
                                  const cs_real_t            *xyz,
                                  const cs_time_step_t       *ts,
                                  void                       *input,
                                  cs_real_t                  *eval)
{
  CS_UNUSED(xyz);
  CS_UNUSED(ts);

  cs_xdef_array_input_t  *array_input = (cs_xdef_array_input_t *)input;

  const int  stride = array_input->stride;

  /* array is assumed to be interlaced */
  if (cs_test_flag(array_input->loc, cs_cdo_primal_cell)) {

    assert(stride == 3);
    cs_real_3_t  cell_vector;
    for (int k = 0; k < stride; k++)
      cell_vector[k] = array_input->values[stride*cm->c_id + k];
    for (int i = 0; i < n_points; i++) {
      eval[3*i    ] = cell_vector[0];
      eval[3*i + 1] = cell_vector[1];
      eval[2*i + 2] = cell_vector[2];
    }

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_primal_vtx)) {

    /* Sanity checks */
    assert(cs_test_flag(cm->flag, CS_CDO_LOCAL_PVQ));
    assert(stride == 3);

    /* Reconstruct (or interpolate) value at the current cell center */
    for (int k = 0; k < stride; k++) {

      for (short int v = 0; v < cm->n_vc; v++) {
        eval[k] += cm->wvc[v] * array_input->values[stride*cm->v_ids[v] + k];

      }

    }

  }
  else if (cs_test_flag(array_input->loc, cs_cdo_dual_face_byc)) {

    assert(array_input->index != NULL);

    /* Reconstruct (or interpolate) value at the current cell center */
    cs_real_3_t  cell_vector;
    cs_reco_dfbyc_in_cell(cm,
                          array_input->values + array_input->index[cm->c_id],
                          cell_vector);

    for (int i = 0; i < n_points; i++) {
      eval[3*i    ] = cell_vector[0];
      eval[3*i + 1] = cell_vector[1];
      eval[3*i + 2] = cell_vector[2];
    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer for evaluating a quantity defined by a field
 *         at a precise location inside a cell
 *         Use of a cs_cell_mesh_t structure.
 *
 * \param[in]  cm        pointer to a cs_cell_mesh_t structure
 * \param[in]  n_points  number of points where to compute the evaluation
 * \param[in]  xyz       where to compute the evaluation
 * \param[in]  ts        pointer to a cs_time_step_t structure
 * \param[in]  input     pointer to an input structure
 * \param[out] eval      result of the evaluation
 */
/*----------------------------------------------------------------------------*/

void
cs_xdef_eval_cw_3_at_xyz_by_field(const cs_cell_mesh_t       *cm,
                                  cs_lnum_t                   n_points,
                                  const cs_real_t            *xyz,
                                  const cs_time_step_t       *ts,
                                  void                       *input,
                                  cs_real_t                  *eval)
{
  CS_UNUSED(xyz);
  CS_UNUSED(ts);

  cs_field_t  *field = (cs_field_t *)input;
  const cs_real_t  *values = field->val;

  assert(field != NULL);
  assert(field->dim == 3);

  const int  c_ml_id = cs_mesh_location_get_id_by_name(N_("cells"));
  const int  v_ml_id = cs_mesh_location_get_id_by_name(N_("vertices"));

  /* array is assumed to be interlaced */
  if (field->location_id == c_ml_id) {

    cs_real_3_t  cell_vector;
    for (int k = 0; k < 3; k++)
      cell_vector[k] = values[3*cm->c_id + k];
    for (int i = 0; i < n_points; i++) { // No interpolation
      eval[3*i    ] = cell_vector[0];
      eval[3*i + 1] = cell_vector[1];
      eval[3*i + 2] = cell_vector[2];
    }

  }
  else if (field->location_id == v_ml_id) {

    /* Sanity check */
    assert(cs_test_flag(cm->flag, CS_CDO_LOCAL_PVQ));

    /* Reconstruct (or interpolate) value at the current cell center */
    for (int k = 0; k < 3; k++) {

      for (short int v = 0; v < cm->n_vc; v++) {
        eval[k] += cm->wvc[v] * values[3*cm->v_ids[v] + k];
      }

    }

  }
  else
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid support for the input array", __func__);

}

/*----------------------------------------------------------------------------*/

END_C_DECLS