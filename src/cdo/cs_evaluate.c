/*============================================================================
 * Functions and structures to deal with evaluation of quantities
 *============================================================================*/

/*
  This file is part of Code_Saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2018 EDF S.A.

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

#include "cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include <bft_mem.h>

#include "cs_halo.h"
#include "cs_math.h"
#include "cs_mesh.h"
#include "cs_parall.h"
#include "cs_range_set.h"
#include "cs_volume_zone.h"
#include "cs_quadrature.h"

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_evaluate.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*! \cond DOXYGEN_SHOULD_SKIP_THIS */

/*=============================================================================
 * Local Macro definitions and structure definitions
 *============================================================================*/

/* Pointer to shared structures (owned by a cs_domain_t structure) */
static const cs_cdo_quantities_t  *cs_cdo_quant;
static const cs_cdo_connect_t  *cs_cdo_connect;
static const cs_time_step_t  *cs_time_step;

static const char _err_empty_array[] =
  " %s: Array storing the evaluation should be allocated before the call"
  " to this function.";
static const char _err_not_handled[] = " %s: Case not handled yet.";
static const char _err_quad[] = " %s: Invalid quadrature type.";

/*============================================================================
 * Private function prototypes
 *============================================================================*/
/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over dual cells of a scalar density field
 *         defined by an analytical function on a cell
 *
 * \param[in]      cm                pointer to a cs_cell_mesh_t structure
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_cellwise_dcsd_by_analytic(const cs_cell_mesh_t           *cm,
                           cs_analytic_func_t             *ana,
                           void                           *input,
                           cs_quadrature_tetra_integral_t *compute_integral,
                           double                          values[])
{
  const double  tcur = cs_time_step->t_cur;
  const double  vol = cm->vol_c;

  for (short int f = 0; f < cm->n_fc; f++) {

    const double  *xf = cm->face[f].center;
    const short int  n_ef = cm->f2e_idx[f+1] - cm->f2e_idx[f];
    const short int  *e_ids = cm->f2e_ids + cm->f2e_idx[f];

    for (short int i = 0; i < n_ef; i++) {

      const short int  e = e_ids[i];
      const short int  v1 = cm->e2v_ids[2*e];
      const short int  v2 = cm->e2v_ids[2*e+1];
      const double  *xv1 = cm->xv + 3*v1, *xv2 = cm->xv + 3*v2;
      const double  *xe = cm->edge[e].center;

      compute_integral(tcur, xv1, xe, xf, cm->xc, vol*cm->wvc[v1],
                       ana, input, values + v1);
      compute_integral(tcur, xv2, xe, xf, cm->xc, vol*cm->wvc[v2],
                       ana, input, values + v2);

    } // Loop on face edges

  } // Loop on cell faces

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over dual cells of a scalar density field
 *         defined by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_elts            number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_dcsd_by_analytic(cs_analytic_func_t              *ana,
                  void                            *input,
                  const cs_lnum_t                  n_elts,
                  const cs_lnum_t                 *elt_ids,
                  cs_quadrature_tetra_integral_t  *compute_integral,
                  double                           values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_cdo_connect_t  *connect = cs_cdo_connect;
  const cs_adjacency_t  *c2f = connect->c2f;
  const cs_adjacency_t  *f2e = connect->f2e;
  const double  tcur = cs_time_step->t_cur;

  /* Compute dual volumes */
  for (cs_lnum_t  id = 0; id < n_elts; id++) {

    const cs_lnum_t  c_id = (elt_ids == NULL) ? id : elt_ids[id];
    const cs_real_t  *xc = quant->cell_centers + 3*c_id;

    for (cs_lnum_t i = c2f->idx[c_id]; i < c2f->idx[c_id+1]; i++) {

      const cs_lnum_t  f_id = c2f->ids[i];
      const cs_real_t  *xf = cs_quant_set_face_center(f_id, quant);

      for (cs_lnum_t j = f2e->idx[f_id]; j < f2e->idx[f_id+1]; j++) {

        const cs_lnum_t  e_id = f2e->ids[j];
        const cs_lnum_t  v1 = connect->e2v->ids[2*e_id];
        const cs_lnum_t  v2 = connect->e2v->ids[2*e_id+1];
        const cs_real_t  *xv1 = quant->vtx_coord + 3*v1;
        const cs_real_t  *xv2 = quant->vtx_coord + 3*v2;

        cs_real_3_t  xe;
        for (int k = 0; k < 3; k++)
          xe[k] = 0.5 * (xv1[k] + xv2[k]);

        compute_integral(tcur, xv1, xe, xf, xc, quant->dcell_vol[v1],
                         ana, input, values + v1);
        compute_integral(tcur, xv2, xe, xf, xc, quant->dcell_vol[v2],
                         ana, input, values + v2);
      } // Loop on edges

    } // Loop on faces

  } // Loop on cells

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over primal cells of a scalar density field
 *         defined by an analytical function on a cell
 *
 * \param[in]      cm                pointer to a cs_cell_mesh_t structure
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      compute_integral  function pointer
 *
 * \return the value of the corresponding integral
 */
/*----------------------------------------------------------------------------*/

static double
_cellwise_pcsd_by_analytic(const cs_cell_mesh_t            *cm,
                           cs_analytic_func_t              *ana,
                           void                            *input,
                           cs_quadrature_tetra_integral_t  *compute_integral)
{
  const double  tcur = cs_time_step->t_cur;

  double  retval = 0.;

  if (cs_cdo_connect->cell_type[cm->c_id] == FVM_CELL_TETRA) {

    compute_integral(tcur, cm->xv, cm->xv + 3, cm->xv + 6, cm->xv + 9,
                     cm->vol_c, ana, input, &retval);

  }
  else {

    for (short int f = 0; f < cm->n_fc; f++) {

      const double  hf_coef = cs_math_onethird * cm->hfc[f];
      const short int  start = cm->f2e_idx[f];
      const short int  n_ef  = cm->f2e_idx[f+1] - start;
      const short int *e_ids = cm->f2e_ids + cm->f2e_idx[f];

      if (n_ef == 3) { // Current face is a triangle --> simpler

        short int  v0, v1, v2;
        cs_cell_mesh_get_next_3_vertices(e_ids, cm->e2v_ids, &v0, &v1, &v2);

        const double *xv0 = cm->xv+3*v0, *xv1 = cm->xv+3*v1, *xv2 = cm->xv+3*v2;

        compute_integral(tcur, xv0, xv1, xv2, cm->xc, hf_coef*cm->face[f].meas,
                         ana, input, &retval);

      }
      else {

        const double  *xf = cm->face[f].center;
        const double  *tef = cm->tef + start;

        for (short int i = 0; i < n_ef; i++) {

          const short int  _2e = 2*e_ids[i];
          const double  *xv1 = cm->xv + 3*cm->e2v_ids[_2e];
          const double  *xv2 = cm->xv + 3*cm->e2v_ids[_2e+1];

          compute_integral(tcur, xv1, xv2, xf, cm->xc, hf_coef*tef[i],
                           ana, input, &retval);

        } // Loop on face edges

      } // Current face is triangle or not ?

    } // Loop on cell faces

  } // Not a tetrahedron

  return retval;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over primal cells of a scalar density field
 *         defined by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_elts            number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pcsd_by_analytic(cs_analytic_func_t              *ana,
                  void                            *input,
                  const cs_lnum_t                  n_elts,
                  const cs_lnum_t                 *elt_ids,
                  cs_quadrature_tetra_integral_t  *compute_integral,
                  double                           values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_real_t  *xv = quant->vtx_coord;
  const cs_cdo_connect_t  *connect = cs_cdo_connect;
  const cs_adjacency_t  *c2f = connect->c2f;
  const cs_adjacency_t  *f2e = connect->f2e;
  const double  tcur = cs_time_step->t_cur;

  for (cs_lnum_t  id = 0; id < n_elts; id++) {

    const cs_lnum_t  c_id = (elt_ids == NULL) ? id : elt_ids[id];
    if (connect->cell_type[c_id] == FVM_CELL_TETRA) {

      const cs_lnum_t  *v_ids = connect->c2v->ids + connect->c2v->idx[c_id];

      compute_integral(tcur,
                    xv+3*v_ids[0], xv+3*v_ids[1], xv+3*v_ids[2], xv+3*v_ids[3],
                       quant->cell_vol[c_id],
                       ana, input, values + c_id);


    }
    else {

      const cs_real_t  *xc = quant->cell_centers + 3*c_id;

      for (cs_lnum_t i = c2f->idx[c_id]; i < c2f->idx[c_id+1]; i++) {

        const cs_lnum_t  f_id = c2f->ids[i];
        const cs_quant_t  pfq = cs_quant_set_face(f_id, quant);
        const double hfc = cs_math_3_dot_product(pfq.unitv,
                                                 quant->dedge_vector+3*f_id);
        const cs_lnum_t start = f2e->idx[f_id],
                          end = f2e->idx[f_id+1],
                         n_ef = end - start;

        if (n_ef == 3) {

          cs_lnum_t v0, v1, v2;
          cs_connect_get_next_3_vertices(connect->f2e->ids, connect->e2v->ids,
                                         start, &v0, &v1, &v2);
          compute_integral(tcur,xv + 3*v0, xv + 3*v1, xv + 3*v2, xc,
                           hfc * pfq.meas,
                           ana, input, values + c_id);
        }
        else {

          for (cs_lnum_t j = start; j < end; j++) {

            const cs_lnum_t  _2e = 2*f2e->ids[j];
            const cs_lnum_t  v1 = connect->e2v->ids[_2e];
            const cs_lnum_t  v2 = connect->e2v->ids[_2e+1];

            compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center, xc,
                             hfc*cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                             ana, input, values + c_id);

          } // Loop on edges

        } // Current face is triangle or not ?

      } // Loop on faces

    } /* Not a tetrahedron */

  } // Loop on cells

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the average over primal cells of a scalar field defined
 *         by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_loc_elts        number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pcsa_by_analytic(cs_analytic_func_t              *ana,
                  void                            *input,
                  const cs_lnum_t                  n_elts,
                  const cs_lnum_t                 *elt_ids,
                  cs_quadrature_tetra_integral_t  *compute_integral,
                  double                           values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_real_t  *xv = quant->vtx_coord;
  const cs_cdo_connect_t  *connect = cs_cdo_connect;
  const cs_adjacency_t  *c2f = connect->c2f;
  const cs_adjacency_t  *f2e = connect->f2e;
  const double  tcur = cs_time_step->t_cur;

  for (cs_lnum_t  id = 0; id < n_elts; id++) {

    const cs_lnum_t  c_id = (elt_ids == NULL) ? id : elt_ids[id];
    if (connect->cell_type[c_id] == FVM_CELL_TETRA) {

      const cs_lnum_t  *v_ids = connect->c2v->ids + connect->c2v->idx[c_id];

      compute_integral(tcur,
                    xv+3*v_ids[0], xv+3*v_ids[1], xv+3*v_ids[2], xv+3*v_ids[3],
                       quant->cell_vol[c_id],
                       ana, input, values + c_id);

    }
    else {

      const cs_real_t  *xc = quant->cell_centers + 3*c_id;

      for (cs_lnum_t i = c2f->idx[c_id]; i < c2f->idx[c_id+1]; i++) {

        const cs_lnum_t  f_id = c2f->ids[i];
        const cs_quant_t  pfq = cs_quant_set_face(f_id, quant);
        const double hfc = cs_math_3_dot_product(pfq.unitv,
                                                 quant->dedge_vector+3*f_id);
        const cs_lnum_t start = f2e->idx[f_id],
                          end = f2e->idx[f_id+1],
                         n_ef = end - start;

        if (n_ef == 3) {

          cs_lnum_t v0, v1, v2;
          cs_connect_get_next_3_vertices(connect->f2e->ids, connect->e2v->ids,
                                         start, &v0, &v1, &v2);
          compute_integral(tcur,xv + 3*v0, xv + 3*v1, xv + 3*v2, xc,
                           hfc * pfq.meas,
                           ana, input, values + c_id);
        }
        else {

          for (cs_lnum_t j = start; j < end; j++) {

            const cs_lnum_t  _2e = 2*f2e->ids[j];
            const cs_lnum_t  v1 = connect->e2v->ids[_2e];
            const cs_lnum_t  v2 = connect->e2v->ids[_2e+1];

            compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center, xc,
                             hfc*cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                             ana, input, values + c_id);

          } // Loop on edges

        } // Current face is triangle or not ?

      } // Loop on faces

    } /* Not a tetrahedron */

    /* Average */
    values[c_id] /= quant->cell_vol[c_id];

  } // Loop on cells

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the average over primal cells of a vector field defined
 *         by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_loc_elts        number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

/* Note: the only difference from the scalar version is that there's 3*c_id in
 * the values. Consider merging the two
 */

static void
_pcva_by_analytic(cs_analytic_func_t              *ana,
                  void                            *input,
                  const cs_lnum_t                  n_elts,
                  const cs_lnum_t                 *elt_ids,
                  cs_quadrature_tetra_integral_t  *compute_integral,
                  double                           values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_real_t  *xv = quant->vtx_coord;
  const cs_cdo_connect_t  *connect = cs_cdo_connect;
  const cs_adjacency_t  *c2f = connect->c2f;
  const cs_adjacency_t  *f2e = connect->f2e;
  const double  tcur = cs_time_step->t_cur;

  for (cs_lnum_t  id = 0; id < n_elts; id++) {

    const cs_lnum_t  c_id = (elt_ids == NULL) ? id : elt_ids[id];
    double *val_i = values + 3*c_id;

    if (connect->cell_type[c_id] == FVM_CELL_TETRA) {

      const cs_lnum_t  *v_ids = connect->c2v->ids + connect->c2v->idx[c_id];

      compute_integral(tcur,
                    xv+3*v_ids[0], xv+3*v_ids[1], xv+3*v_ids[2], xv+3*v_ids[3],
                       quant->cell_vol[c_id],
                       ana, input, values + c_id);

    }
    else {

      const cs_real_t  *xc = quant->cell_centers + 3*c_id;

      for (cs_lnum_t i = c2f->idx[c_id]; i < c2f->idx[c_id+1]; i++) {

        const cs_lnum_t  f_id = c2f->ids[i];
        const cs_quant_t  pfq = cs_quant_set_face(f_id, quant);
        const double hfc = cs_math_3_dot_product(pfq.unitv,
                                                 quant->dedge_vector+3*f_id);
        const cs_lnum_t start = f2e->idx[f_id],
                          end = f2e->idx[f_id+1],
                         n_ef = end - start;

        if (n_ef == 3) {

          cs_lnum_t v0, v1, v2;
          cs_connect_get_next_3_vertices(connect->f2e->ids, connect->e2v->ids,
                                         start, &v0, &v1, &v2);
          compute_integral(tcur, xv + 3*v0, xv + 3*v1, xv + 3*v2, xc,
                           hfc * pfq.meas,
                           ana, input, values + c_id);

        }
        else {

          for (cs_lnum_t j = start; j < end; j++) {

            const cs_lnum_t  _2e = 2*f2e->ids[j];
            const cs_lnum_t  v1 = connect->e2v->ids[_2e];
            const cs_lnum_t  v2 = connect->e2v->ids[_2e+1];

            compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center, xc,
                             hfc*cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                             ana, input, values + c_id);

          } // Loop on edges

        } // Current face is triangle or not ?

      } // Loop on faces

    } /* Not a tetrahedron */

    const double _overvol = 1./quant->cell_vol[c_id];
    for (short int xyz = 0; xyz < 3; xyz++)
      val_i[xyz] *= _overvol;

  } // Loop on cells

}


/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over a dual cell (or a portion) of a value
 *         defined on a selection of (primal) cells
 *
 * \param[in]      const_val   constant value
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_dcsd_by_value(const double       const_val,
               const cs_lnum_t    n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_adjacency_t  *c2v = cs_cdo_connect->c2v;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_real_t  *dual_vol = quant->dcell_vol; /* scan by c2v */

  if (elt_ids == NULL) {

    assert(n_elts == quant->n_cells);
    for (cs_lnum_t c_id = 0; c_id < n_elts; c_id++)
      for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++)
        values[c2v->ids[j]] += dual_vol[j]*const_val;

  }
  else { /* Loop on selected cells */

    for (cs_lnum_t i = 0; i < n_elts; i++) {
      cs_lnum_t  c_id = elt_ids[i];
      for (cs_lnum_t  j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++)
        values[c2v->ids[j]] += dual_vol[j]*const_val;
    }

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over a dual cell (or a portion) of a
 *         vector-valued density field defined on a selection of (primal) cells
 *
 * \param[in]      const_vec   constant vector
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_dcvd_by_value(const double       const_vec[3],
               const cs_lnum_t    n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_adjacency_t  *c2v = cs_cdo_connect->c2v;
  const cs_real_t  *dual_vol = cs_cdo_quant->dcell_vol; /* scan by c2v */

  if (elt_ids == NULL) {

    for (cs_lnum_t c_id = 0; c_id < n_elts; c_id++) {
      for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++) {
        const cs_lnum_t  v_id = c2v->ids[j];
        const cs_real_t  vol_vc = dual_vol[j];

        values[3*v_id   ] += vol_vc*const_vec[0];
        values[3*v_id +1] += vol_vc*const_vec[1];
        values[3*v_id +2] += vol_vc*const_vec[2];

      }
    }

  }
  else { /* Loop on selected cells */

    for (cs_lnum_t i = 0; i < n_elts; i++) {
      const cs_lnum_t  c_id = elt_ids[i];
      for (cs_lnum_t  j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++) {
        const cs_lnum_t  v_id = c2v->ids[j];
        const cs_real_t  vol_vc = dual_vol[j];

        values[3*v_id   ] += vol_vc*const_vec[0];
        values[3*v_id +1] += vol_vc*const_vec[1];
        values[3*v_id +2] += vol_vc*const_vec[2];
      }
    }

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over a (primal) cell of a value related to
 *         scalar density field
 *
 * \param[in]      const_val   constant value
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pcsd_by_value(const double       const_val,
               const cs_lnum_t    n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;

  if (elt_ids == NULL) { /* All the support entities are selected */
#   pragma omp parallel for if (quant->n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < quant->n_cells; c_id++)
      values[c_id] = quant->cell_vol[c_id]*const_val;
  }

  else { /* Loop on selected cells */
#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      cs_lnum_t  c_id = elt_ids[i];
      values[c_id] = quant->cell_vol[c_id]*const_val;
    }
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the average over a (primal) cell of a scalar field
 *
 * \param[in]      const_val   constant value
 * \param[in]      n_loc_elts  number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static inline void
_pcsa_by_value(const double       const_val,
               const cs_lnum_t    n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;

  if (elt_ids == NULL) { /* All the support entities are selected */
#   pragma omp parallel for if (quant->n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < quant->n_cells; c_id++)
      values[c_id] = const_val;
  }

  else { /* Loop on selected cells */
#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      cs_lnum_t  c_id = elt_ids[i];
      values[c_id] = const_val;
    }
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the integral over a (primal) cell of a vector-valued
 *         density field
 *
 * \param[in]      const_vec   constant values
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pcvd_by_value(const double        const_vec[3],
               const cs_lnum_t     n_elts,
               const cs_lnum_t    *elt_ids,
               double              values[])
{
  const cs_real_t  *vol = cs_cdo_quant->cell_vol;

  if (elt_ids == NULL) { /* All the support entities are selected */
#   pragma omp parallel for if (cs_cdo_quant->n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < cs_cdo_quant->n_cells; c_id++) {
      const cs_real_t  vol_c = vol[c_id];
      values[3*c_id]   = vol_c*const_vec[0];
      values[3*c_id+1] = vol_c*const_vec[1];
      values[3*c_id+2] = vol_c*const_vec[2];
    }
  }

  else { /* Loop on selected cells */
#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      const cs_lnum_t  c_id = elt_ids[i];
      const cs_real_t  vol_c = vol[c_id];
      values[3*c_id  ] = vol_c*const_vec[0];
      values[3*c_id+1] = vol_c*const_vec[1];
      values[3*c_id+2] = vol_c*const_vec[2];
    }
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the average over a (primal) cell of a vector-valued field
 *
 * \param[in]      const_vec   constant values
 * \param[in]      n_loc_elts  number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static inline void
_pcva_by_value(const double        const_vec[3],
               const cs_lnum_t     n_elts,
               const cs_lnum_t    *elt_ids,
               double              values[])
{
  if (elt_ids == NULL) { /* All the support entities are selected */
#   pragma omp parallel for if (cs_cdo_quant->n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < cs_cdo_quant->n_cells; c_id++) {
      memcpy(values+3*c_id,const_vec,3*sizeof(double));
    }
  }

  else { /* Loop on selected cells */
#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) {
      const cs_lnum_t  c_id = elt_ids[i];
      memcpy(values+3*c_id,const_vec,3*sizeof(double));
    }
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the values at each primal faces for a potential defined
 *         by an analytical function on a selection of (primal) cells.
 *         This potential can, be scalar-, vector- or tensor-valued. This is
 *         handled in the definition of the analytic function.
 *
 * \param[in]      ana         pointer to the analytic function
 * \param[in]      input       NULL or pointer to a structure cast on-the-fly
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pfp_by_analytic(cs_analytic_func_t    *ana,
                 void                  *input,
                 const cs_lnum_t        n_elts,
                 const cs_lnum_t       *elt_ids,
                 double                 values[])
{
  const double  tcur = cs_time_step->t_cur;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;

  /* Initialize todo array */
  bool  *todo = NULL;

  BFT_MALLOC(todo, quant->n_faces, bool);
# pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
  for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
    todo[f_id] = true;

  for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

    cs_lnum_t  c_id = elt_ids[i];

    for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

      cs_lnum_t  f_id = c2f->ids[j];
      if (todo[f_id]) {
        const cs_real_t  *xf = cs_quant_set_face_center(f_id, quant);
        ana(tcur, 1, NULL, xf, false,  input, values + f_id);
        todo[f_id] = false;
      }

    } // Loop on cell faces

  } // Loop on selected cells

  BFT_FREE(todo);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the average at each primal faces for a scalar potential
 *         defined by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_loc_elts        number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pfsa_by_analytic(cs_analytic_func_t             *ana,
                  void                           *input,
                  const cs_lnum_t                 n_elts,
                  const cs_lnum_t                *elt_ids,
                  cs_quadrature_tria_integral_t  *compute_integral,
                  double                          values[])
{
  const double  tcur = cs_time_step->t_cur;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;
  const cs_adjacency_t  *f2e = cs_cdo_connect->f2e;
  const cs_adjacency_t  *e2v = cs_cdo_connect->e2v;
  const cs_real_t  *xv = quant->vtx_coord;

  if (elt_ids == NULL) {

#   pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
    for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++) {

      const cs_quant_t pfq = cs_quant_set_face(f_id, quant);
      const cs_lnum_t   start_idx = f2e->idx[f_id],
                        end_idx   = f2e->idx[f_id+1];
      double *val_i = values + f_id;

      switch (end_idx - start_idx) {

      case CS_TRIANGLE_CASE: /* Triangle: one-shot computation */
        {
          cs_lnum_t  v1, v2, v3;

          cs_connect_get_next_3_vertices(f2e->ids, e2v->ids, start_idx,
                                         &v1, &v2, &v3);
          compute_integral(tcur, xv + 3*v1, xv + 3*v2, xv + 3*v3, pfq.meas,
                           ana, input, val_i);
        }
        break;

      default:
        for (cs_lnum_t j = start_idx; j < end_idx; j++) {

          const cs_lnum_t  _2e = 2*f2e->ids[j];
          const cs_lnum_t  v1 = e2v->ids[_2e];
          const cs_lnum_t  v2 = e2v->ids[_2e+1];

          compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center,
                           cs_math_surftri(xv + 3*v1, xv + 3*v2, pfq.center),
                           ana, input, val_i);

        } /* Loop on edges */
        break;

      } /* End of switch */

      /* Average */
      val_i[0] /= pfq.meas;

    } // Loop on faces

  }
  else {

    /* Initialize todo array */
    bool  *todo = NULL;

    BFT_MALLOC(todo, quant->n_faces, bool);
#   pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
    for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
      todo[f_id] = true;

    for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

      cs_lnum_t  c_id = elt_ids[i];

      for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

        cs_lnum_t  f_id = c2f->ids[j];
        if (todo[f_id]) {

          todo[f_id] = false;

          const cs_quant_t pfq = cs_quant_set_face(f_id, quant);
          const cs_lnum_t  start_idx = f2e->idx[f_id],
                           end_idx   = f2e->idx[f_id+1];
          double *val_i = values + f_id;

          switch (end_idx - start_idx) {

          case CS_TRIANGLE_CASE: /* Triangle: one-shot computation */
            {
              cs_lnum_t  v1, v2, v3;

              cs_connect_get_next_3_vertices(f2e->ids, e2v->ids, start_idx,
                                             &v1, &v2, &v3);

              compute_integral(tcur, xv + 3*v1, xv + 3*v2, xv + 3*v3,
                               pfq.meas, ana, input, val_i);
            }
            break;

          default:
            for (cs_lnum_t k = start_idx; k < end_idx; k++) {

              const cs_lnum_t  _2e = 2*f2e->ids[k];
              const cs_lnum_t  v1 = e2v->ids[_2e];
              const cs_lnum_t  v2 = e2v->ids[_2e+1];

              compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center,
                               cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                               ana, input, val_i);
            } /* Loop on edges */
            break;

          } /* End of switch */

          /* Average */
          val_i[0] /= pfq.meas;

        } /* TODO == true */

      } // Loop on cell faces

    } // Loop on selected cells

    BFT_FREE(todo);

  } /* If there is a selection of cells */

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the average at each primal faces for a vector potential
 *         defined by an analytical function on a selection of (primal) cells
 *
 * \param[in]      ana               pointer to the analytic function
 * \param[in]      input             NULL or pointer cast on-the-fly
 * \param[in]      n_loc_elts        number of elements to consider
 * \param[in]      elt_ids           pointer to the list od selected ids
 * \param[in]      compute_integral  function pointer
 * \param[in, out] values            pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pfva_by_analytic(cs_analytic_func_t             *ana,
                  void                           *input,
                  const cs_lnum_t                 n_elts,
                  const cs_lnum_t                *elt_ids,
                  cs_quadrature_tria_integral_t  *compute_integral,
                  double                          values[])
{
  const double  tcur = cs_time_step->t_cur;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;
  const cs_adjacency_t  *f2e = cs_cdo_connect->f2e;
  const cs_adjacency_t  *e2v = cs_cdo_connect->e2v;
  const cs_real_t  *xv = quant->vtx_coord;

  if (elt_ids == NULL) {

#   pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
    for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++) {

      const cs_quant_t pfq = cs_quant_set_face(f_id, quant);
      const cs_lnum_t   start_idx = f2e->idx[f_id],
                        end_idx   = f2e->idx[f_id+1];
      double *val_i = values + 3*f_id;

      switch (end_idx - start_idx) {

      case CS_TRIANGLE_CASE: /* Triangle: one-shot computation */
        {
          cs_lnum_t  v1, v2, v3;

          cs_connect_get_next_3_vertices(f2e->ids, e2v->ids, start_idx,
                                         &v1, &v2, &v3);

          compute_integral(tcur, xv + 3*v1, xv + 3*v2, xv + 3*v3, pfq.meas,
                           ana, input, val_i);
        }
        break;

      default:
        for (cs_lnum_t j = start_idx; j < end_idx; j++) {

          const cs_lnum_t  _2e = 2*f2e->ids[j];
          const cs_lnum_t  v1 = e2v->ids[_2e];
          const cs_lnum_t  v2 = e2v->ids[_2e+1];

          compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center,
                           cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                           ana, input, val_i);

        } /* Loop on face edges */

      } /* End of switch */

      /* Average */
      const double _oversurf = 1./pfq.meas;
      for (short int xyz = 0; xyz < 3; xyz++)
        val_i[xyz] *= _oversurf;

    }  /* Loop on all faces */

  }
  else {

    /* Initialize todo array */
    bool  *todo = NULL;

    BFT_MALLOC(todo, quant->n_faces, bool);
#   pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
    for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
      todo[f_id] = true;

    for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

      cs_lnum_t  c_id = elt_ids[i];

      for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

        cs_lnum_t  f_id = c2f->ids[j];
        if (todo[f_id]) {

          todo[f_id] = false;

          const cs_quant_t pfq = cs_quant_set_face(f_id, quant);
          const cs_lnum_t   start_idx = f2e->idx[f_id],
                            end_idx   = f2e->idx[f_id+1];
          double *val_i = values + 3*f_id;

          switch (end_idx - start_idx) {

          case CS_TRIANGLE_CASE: /* Triangle: one-shot computation */
            {
              cs_lnum_t  v1, v2, v3;

              cs_connect_get_next_3_vertices(f2e->ids, e2v->ids, start_idx,
                                             &v1, &v2, &v3);

              compute_integral(tcur, xv + 3*v1, xv + 3*v2, xv + 3*v3,
                               pfq.meas, ana, input, val_i);
            }
            break;

          default:
            for (cs_lnum_t k = start_idx; k < end_idx; k++) {

              const cs_lnum_t  _2e = 2*f2e->ids[k];
              const cs_lnum_t  v1 = e2v->ids[_2e];
              const cs_lnum_t  v2 = e2v->ids[_2e+1];

              compute_integral(tcur, xv + 3*v1, xv + 3*v2, pfq.center,
                               cs_math_surftri(xv+3*v1, xv+3*v2, pfq.center),
                               ana, input, val_i);

            } /* Loop on face edges */

          } /* End of switch */

          /* Average */
          const double _oversurf = 1./pfq.meas;
          for (short int xyz = 0; xyz < 3; xyz++)
            val_i[xyz] *= _oversurf;

        } // If todo

      } // Loop on cell faces

    } // Loop on selected cells

    BFT_FREE(todo);

  } /* If there is a selection of cells */

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the values at each primal vertices for a potential
 *         defined by an analytical function on a selection of (primal) cells
 *         This potential can, be scalar-, vector- or tensor-valued. This is
 *         handled in the definition of the analytic function.
 *
 * \param[in]      ana         pointer to the analytic function
 * \param[in]      input       NULL or pointer to a structure cast on-the-fly
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

static void
_pvp_by_analytic(cs_analytic_func_t    *ana,
                 void                  *input,
                 const cs_lnum_t        n_elts,
                 const cs_lnum_t       *elt_ids,
                 double                 values[])
{
  const double  tcur = cs_time_step->t_cur;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2v = cs_cdo_connect->c2v;

  /* Initialize todo array */
  cs_lnum_t  *vtx_lst = NULL;

  BFT_MALLOC(vtx_lst, quant->n_vertices, cs_lnum_t);
# pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
  for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++)
    vtx_lst[v_id] = -1; // No flag

  for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

    const cs_lnum_t  c_id = elt_ids[i];
    for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++) {

      cs_lnum_t  v_id = c2v->ids[j];
      if (vtx_lst[v_id] == -1) // Not encountered yet
        vtx_lst[v_id] = v_id;

    } // Loop on cell vertices
  } // Loop on selected cells

  /* Count number of selected vertices */
  cs_lnum_t  n_selected_vertices = 0;
  for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++) {
    if (vtx_lst[v_id] == v_id)
      vtx_lst[n_selected_vertices++] = v_id;
  }

  /* One call for all selected vertices */
  ana(tcur, n_selected_vertices, vtx_lst, quant->vtx_coord,
      false,  // compacted output ?
      input,
      values);

  BFT_FREE(vtx_lst);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the values at each primal faces for a scalar potential
 *
 * \param[in]      const_val   constant value
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the array storing the values
 */
/*----------------------------------------------------------------------------*/

static void
_pfsp_by_value(const double       const_val,
               cs_lnum_t          n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;

  /* Initialize todo array */
  bool  *todo = NULL;

  BFT_MALLOC(todo, quant->n_faces, bool);
# pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
  for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
    todo[f_id] = true;

  for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

    cs_lnum_t  c_id = elt_ids[i];

    for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

      cs_lnum_t  f_id = c2f->ids[j];
      if (todo[f_id])
        values[f_id] = const_val, todo[f_id] = false;

    } // Loop on cell vertices

  } // Loop on selected cells

  BFT_FREE(todo);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the values at each primal faces for a scalar potential
 *
 * \param[in]      const_vec   constant value
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the array storing the values
 */
/*----------------------------------------------------------------------------*/

static void
_pfvp_by_value(const double       const_vec[3],
               cs_lnum_t          n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;

  /* Initialize todo array */
  bool  *todo = NULL;

  BFT_MALLOC(todo, quant->n_faces, bool);
# pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
  for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
    todo[f_id] = true;

  for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

    cs_lnum_t  c_id = elt_ids[i];

    for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

      cs_lnum_t  f_id = c2f->ids[j];
      if (todo[f_id]) {
        todo[f_id] = false;
        memcpy(values+3*f_id,const_vec,3*sizeof(double));
      }

    } // Loop on cell vertices

  } // Loop on selected cells

  BFT_FREE(todo);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Unmarked vertices belonging to the frontier of the cell selection
 *
 * \param[in]      c_id          id of the cell to treat
 * \param[in]      cell_tag      tag for each cell
 * \param[in, out] vtx_tag       tag for each vertex
 */
/*----------------------------------------------------------------------------*/

static void
_untag_frontier_vertices(cs_lnum_t      c_id,
                         const bool     cell_tag[],
                         cs_lnum_t      vtx_tag[])
{
  const cs_mesh_t  *m = cs_glob_mesh;
  const cs_lnum_t  *f2v_idx = m->i_face_vtx_idx;
  const cs_lnum_t  *f2v_lst = m->i_face_vtx_lst;
  const cs_adjacency_t  *c2f = cs_cdo_connect->c2f;

  for (cs_lnum_t j = c2f->idx[c_id]; j < c2f->idx[c_id+1]; j++) {

    const cs_lnum_t  f_id = c2f->ids[j];
    if (f_id < m->n_i_faces) { /* interior face */

      if (cell_tag[m->i_face_cells[f_id][0]] == false ||
          cell_tag[m->i_face_cells[f_id][1]] == false) {

        for (cs_lnum_t i = f2v_idx[f_id]; i < f2v_idx[f_id+1]; i++)
          vtx_tag[f2v_lst[i]] = 0; // untag

      }
    } // This face belongs to the frontier of the selection (only interior)

  } // Loop on cell faces

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define a value to each DoF such that a given quantity is put inside
 *         the volume associated to the list of cells
 *
 * \param[in]      quantity_val  amount of quantity to distribute
 * \param[in]      n_elts        number of elements to consider
 * \param[in]      elt_ids       pointer to the list od selected ids
 * \param[in, out] values        pointer to the array storing the values
 */
/*----------------------------------------------------------------------------*/

static void
_pvsp_by_qov(const double       quantity_val,
             cs_lnum_t          n_elts,
             const cs_lnum_t   *elt_ids,
             double             values[])
{
  const cs_mesh_t  *m = cs_glob_mesh;
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_lnum_t  n_cells = quant->n_cells;
  const cs_lnum_t  n_vertices = quant->n_vertices;
  const cs_real_t  *dc_vol = quant->dcell_vol;
  const cs_adjacency_t  *c2v = cs_cdo_connect->c2v;

  cs_lnum_t  *vtx_tag = NULL;
  bool  *cell_tag = NULL;

  BFT_MALLOC(vtx_tag, n_vertices, cs_lnum_t);
  BFT_MALLOC(cell_tag, m->n_cells_with_ghosts, bool);

  if (n_elts < n_cells) { /* Only some cells are selected */

#   pragma omp parallel for if (n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < n_vertices; v_id++)
      vtx_tag[v_id] = 0;
#   pragma omp parallel for if (n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < m->n_cells_with_ghosts; c_id++)
      cell_tag[c_id] = false;

  /* First pass: flag cells and vertices */
#   pragma omp parallel for if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

      const cs_lnum_t  c_id = elt_ids[i];
      cell_tag[c_id] = true;
      for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++)
        vtx_tag[c2v->ids[j]] = -1; // activated

    } // Loop on selected cells

  }
  else { /* All cells are selected */

    assert(n_cells == n_elts);

#   pragma omp parallel for if (n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < n_vertices; v_id++)
      vtx_tag[v_id] = -1;

#   pragma omp parallel for if (n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < n_cells; c_id++)
      cell_tag[c_id] = true;
    for (cs_lnum_t c_id = n_cells; c_id < m->n_cells_with_ghosts; c_id++)
      cell_tag[c_id] = false;

  }

  if (m->halo != NULL)
    cs_halo_sync_untyped(m->halo, CS_HALO_STANDARD, sizeof(bool), cell_tag);

  /* Second pass: detect cells at the frontier of the selection */
  if (n_elts < n_cells) { /* Only some cells are selected */

    for (cs_lnum_t i = 0; i < n_elts; i++)
      _untag_frontier_vertices(elt_ids[i], cell_tag, vtx_tag);

  }
  else {

    for (cs_lnum_t i = 0; i < n_cells; i++)
      _untag_frontier_vertices(i, cell_tag, vtx_tag);

  }

  /* Handle parallelism */
  if (cs_glob_n_ranks > 1)
    cs_interface_set_max(cs_cdo_connect->interfaces[CS_CDO_CONNECT_VTX_SCAL],
                         n_vertices,
                         1,           // stride
                         true,        // interlace, not useful here
                         CS_LNUM_TYPE,
                         (void *)vtx_tag);

  /* Third pass: compute the (really) available volume */
  double  volume_marked = 0.;

  if (elt_ids != NULL) { /* Only some cells are selected */

#   pragma omp parallel for reduction(+:volume_marked) if (n_elts > CS_THR_MIN)
    for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

      const cs_lnum_t  c_id = elt_ids[i];
      for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++)
        if (vtx_tag[c2v->ids[j]] == -1) // activated
          volume_marked += dc_vol[j]; // | dual_cell cap cell |

    } // Loop on selected cells

  }
  else { /* elt_ids == NULL => all cells are selected */

# pragma omp parallel for reduction(+:volume_marked) if (n_cells > CS_THR_MIN)
    for (cs_lnum_t c_id = 0; c_id < n_cells; c_id++) {
      for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++)
        if (vtx_tag[c2v->ids[j]] == -1) // activated
          volume_marked += dc_vol[j]; // | dual_cell cap cell |
    }

  }

  /* Handle parallelism */
  if (cs_glob_n_ranks > 1)
    cs_parall_sum(1, CS_DOUBLE, &volume_marked);

  double val_to_set = quantity_val;
  if (volume_marked > 0)
    val_to_set /= volume_marked;

  if (n_elts < n_cells) { /* Only some cells are selected */

#   pragma omp parallel for if (n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < n_vertices; v_id++)
      if (vtx_tag[v_id] == -1)
        values[v_id] = val_to_set;

  }
  else { /* All cells are selected */

#   pragma omp parallel for if (n_vertices > CS_THR_MIN)
    for (cs_lnum_t v_id = 0; v_id < n_vertices; v_id++)
      values[v_id] = val_to_set;

  }

  BFT_FREE(cell_tag);
  BFT_FREE(vtx_tag);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Get the values at each primal vertices for a scalar potential
 *
 * \param[in]      const_val   constant value
 * \param[in]      n_elts      number of elements to consider
 * \param[in]      elt_ids     pointer to the list od selected ids
 * \param[in, out] values      pointer to the array storing the values
 */
/*----------------------------------------------------------------------------*/

static void
_pvsp_by_value(cs_real_t          const_val,
               cs_lnum_t          n_elts,
               const cs_lnum_t   *elt_ids,
               double             values[])
{
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_adjacency_t  *c2v = cs_cdo_connect->c2v;

  /* Initialize todo array */
  bool  *todo = NULL;

  BFT_MALLOC(todo, quant->n_vertices, bool);

# pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
  for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++)
    todo[v_id] = true;

  for (cs_lnum_t i = 0; i < n_elts; i++) { // Loop on selected cells

    cs_lnum_t  c_id = elt_ids[i];

    for (cs_lnum_t j = c2v->idx[c_id]; j < c2v->idx[c_id+1]; j++) {

      cs_lnum_t  v_id = c2v->ids[j];
      if (todo[v_id])
        values[v_id] = const_val, todo[v_id] = false;

    } // Loop on cell vertices

  } // Loop on selected cells

  BFT_FREE(todo);
}

/*! (DOXYGEN_SHOULD_SKIP_THIS) \endcond */

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Set shared pointers to main domain members
 *
 * \param[in]  quant       additional mesh quantities struct.
 * \param[in]  connect     pointer to a cs_cdo_connect_t struct.
 * \param[in]  time_step   pointer to a time step structure
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_set_shared_pointers(const cs_cdo_quantities_t    *quant,
                                const cs_cdo_connect_t       *connect,
                                const cs_time_step_t         *time_step)
{
  /* Assign static const pointers */
  cs_cdo_quant = quant;
  cs_cdo_connect = connect;
  cs_time_step = time_step;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Compute the value related to each DoF in the case of a density field
 *         The value defined by the analytic function is by unity of volume
 *
 * \param[in]      dof_flag    indicate where the evaluation has to be done
 * \param[in]      def         pointer to a cs_xdef_t structure
 * \param[in, out] retval      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_density_by_analytic(cs_flag_t           dof_flag,
                                const cs_xdef_t    *def,
                                double              retval[])
{
  /* Sanity check */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

    /* Retrieve information from mesh location structures */
  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  cs_quadrature_tetra_integral_t  *qfunc = NULL;
  switch (def->qtype) {

  case CS_QUADRATURE_BARY: /* Barycenter of the tetrahedral subdiv. */
  case CS_QUADRATURE_BARY_SUBDIV:
    qfunc = cs_quadrature_tet_1pt_scal;
    break;

  case CS_QUADRATURE_HIGHER: /* Quadrature with a unique weight */
    qfunc = cs_quadrature_tet_4pts_scal;
    break;

  case CS_QUADRATURE_HIGHEST: /* Most accurate quadrature available */
    qfunc = cs_quadrature_tet_5pts_scal;
    break;

  default:
    bft_error(__FILE__, __LINE__, 0, _("Invalid quadrature type.\n"));

  } /* Which type of quadrature to use */

  /* Perform the evaluation */
  if (dof_flag & CS_FLAG_SCALAR) { /* DoF is scalar-valued */

    cs_xdef_analytic_input_t  *anai = (cs_xdef_analytic_input_t *)def->input;

    if (cs_flag_test(dof_flag, cs_flag_primal_cell)) {

      _pcsd_by_analytic(anai->func, anai->input,
                        z->n_elts, z->elt_ids, qfunc, retval);

    }
    else if (cs_flag_test(dof_flag, cs_flag_dual_cell)) {

      _dcsd_by_analytic(anai->func, anai->input,
                        z->n_elts, z->elt_ids, qfunc,
                        retval);

    }
    else
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

  }
  else
    bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the quantity defined by a value in the case of a density
 *         field for all the degrees of freedom
 *         Accessor to the value is by unit of volume
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t structure
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_density_by_value(cs_flag_t          dof_flag,
                             const cs_xdef_t   *def,
                             double             retval[])
{
  /* Sanity check */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  /* Retrieve information from mesh location structures */
  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  /* Perform the evaluation */
  if (dof_flag & CS_FLAG_SCALAR) { /* DoF is scalar-valued */

    const cs_real_t  *constant_val = (const cs_real_t *)def->input;

    if (cs_flag_test(dof_flag, cs_flag_primal_cell))
      _pcsd_by_value(constant_val[0], z->n_elts, z->elt_ids, retval);
    else if (cs_flag_test(dof_flag, cs_flag_dual_cell))
      _dcsd_by_value(constant_val[0], z->n_elts, z->elt_ids, retval);
    else
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

  }
  else if (dof_flag & CS_FLAG_VECTOR) { /* DoF is vector-valued */

    const cs_real_t  *constant_vec = (const cs_real_t *)def->input;

    if (cs_flag_test(dof_flag, cs_flag_primal_cell))
      _pcvd_by_value(constant_vec, z->n_elts, z->elt_ids, retval);
    else if (cs_flag_test(dof_flag, cs_flag_dual_cell))
      _dcvd_by_value(constant_vec, z->n_elts, z->elt_ids, retval);
    else
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

  }
  else
    bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the quantity attached to a potential field for all the DoFs
 *         when the definition relies on an analytic expression
 *
 * \param[in]      dof_flag    indicate where the evaluation has to be done
 * \param[in]      def         pointer to a cs_xdef_t pointer
 * \param[in, out] retval      pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_potential_by_analytic(cs_flag_t           dof_flag,
                                  const cs_xdef_t    *def,
                                  double              retval[])
{
  /* Sanity check */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  cs_range_set_t  *rs = NULL;
  cs_xdef_analytic_input_t  *anai = (cs_xdef_analytic_input_t *)def->input;

  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);
  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_cdo_connect_t  *connect = cs_cdo_connect;
  const double  tcur = cs_time_step->t_cur;

  /* Perform the evaluation */
  if (cs_flag_test(dof_flag, cs_flag_primal_vtx)) {

    switch (def->dim) {

    case 1: /* Scalar-valued */
      assert(dof_flag & CS_FLAG_SCALAR);
      rs = connect->range_sets[CS_CDO_CONNECT_VTX_SCAL];
      break;

    case 3: /* Vector-valued */
      assert(dof_flag & CS_FLAG_VECTOR);
      rs = connect->range_sets[CS_CDO_CONNECT_VTX_VECT];
      break;

    default:
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);
      break;

    }

    if (def->meta & CS_FLAG_FULL_LOC)
      anai->func(tcur,
                 quant->n_vertices, NULL, quant->vtx_coord,
                 false,  // compacted output ?
                 anai->input,
                 retval);
    else
      _pvp_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids,
                       retval);

    if (cs_glob_n_ranks > 1)
      cs_range_set_sync(rs, CS_DOUBLE, def->dim, (void *)retval);

  } /* Located at primal vertices */

  else if (cs_flag_test(dof_flag, cs_flag_primal_face)) {

    switch (def->dim) {

    case 1: /* Scalar-valued */
      assert(dof_flag & CS_FLAG_SCALAR);
      rs = connect->range_sets[CS_CDO_CONNECT_FACE_SP0];
      break;

    case 3: /* Vector-valued */
      assert(dof_flag & CS_FLAG_VECTOR);
      rs = connect->range_sets[CS_CDO_CONNECT_FACE_VP0];
      break;

    default:
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);
      break;

    }

    if (def->meta & CS_FLAG_FULL_LOC) {

      /* All the support entities are selected:
         - First pass: interior faces
         - Second pass: border faces
      */
      anai->func(tcur,
                 quant->n_i_faces, NULL, quant->i_face_center,
                 true, /* Output is compacted ? */
                 anai->input,
                 retval);
      anai->func(tcur,
                 quant->n_b_faces, NULL, quant->b_face_center,
                 true, /* Output is compacted ? */
                 anai->input,
                 retval + def->dim*quant->n_i_faces);

    }
    else
      _pfp_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids,
                       retval);

    if (cs_glob_n_ranks > 1)
      cs_range_set_sync(rs, CS_DOUBLE, def->dim, (void *)retval);

  } /* Located at primal faces */

  else if (cs_flag_test(dof_flag, cs_flag_primal_cell) ||
           cs_flag_test(dof_flag, cs_flag_dual_vtx)) {

    if (def->meta & CS_FLAG_FULL_LOC) /* All cells are selected */
      anai->func(tcur,
                 quant->n_cells, NULL, quant->cell_centers,
                 false, // compacted output
                 anai->input,
                 retval);
    else
      anai->func(tcur,
                 z->n_elts, z->elt_ids, quant->cell_centers,
                 false, // compacted output
                 anai->input,
                 retval);

    /* No sync since theses values are computed by only one rank */

  } /* Located at primal cells or dual vertices */
  else
    bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define a value to each DoF in the case of a potential field in order
 *         to put a given quantity inside the volume associated to the zone
 *         attached to the given definition
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_potential_by_qov(cs_flag_t          dof_flag,
                             const cs_xdef_t   *def,
                             double             retval[])
{
  /* Sanity check */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  const cs_real_t  *input = (cs_real_t *)def->input;
  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  /* Perform the evaluation */
  bool check = false;
  if (dof_flag & CS_FLAG_SCALAR) { /* DoF is scalar-valued */

    const cs_real_t  const_val = input[0];

    if (cs_flag_test(dof_flag, cs_flag_primal_vtx))
      _pvsp_by_qov(const_val, z->n_elts, z->elt_ids, retval);
    check = true;

  } /* Located at primal vertices */

  if (!check)
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Stop evaluating a potential from 'quantity over volume'."
                "\n This situation is not handled yet."), __func__);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the quantity attached to a potential field for all the DoFs
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_potential_by_value(cs_flag_t          dof_flag,
                               const cs_xdef_t   *def,
                               double             retval[])
{
  /* Sanity check */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_real_t  *input = (cs_real_t *)def->input;
  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  /* Perform the evaluation */
  if (dof_flag & CS_FLAG_SCALAR) { /* DoF is scalar-valued */

    const cs_real_t  const_val = input[0];

    if (cs_flag_test(dof_flag, cs_flag_primal_vtx)) {

      if (def->meta & CS_FLAG_FULL_LOC) {
#       pragma omp parallel for if (quant->n_vertices > CS_THR_MIN)
        for (cs_lnum_t v_id = 0; v_id < quant->n_vertices; v_id++)
          retval[v_id] = const_val;
      }
      else
        _pvsp_by_value(const_val, z->n_elts, z->elt_ids, retval);

    } /* Located at primal vertices */

    else if (cs_flag_test(dof_flag, cs_flag_primal_face)) {

      if (def->meta & CS_FLAG_FULL_LOC) {
#       pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
        for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
          retval[f_id] = const_val;
      }
      else
        _pfsp_by_value(const_val, z->n_elts, z->elt_ids, retval);

    } /* Located at primal faces */

    else if (cs_flag_test(dof_flag, cs_flag_primal_cell) ||
             cs_flag_test(dof_flag, cs_flag_dual_vtx)) {

      if (def->meta & CS_FLAG_FULL_LOC) {
#       pragma omp parallel for if (quant->n_cells > CS_THR_MIN)
        for (cs_lnum_t c_id = 0; c_id < quant->n_cells; c_id++)
          retval[c_id] = const_val;
      }
      else
        for (cs_lnum_t i = 0; i < z->n_elts; i++) // Loop on selected cells
          retval[z->elt_ids[i]] = const_val;

    } /* Located at primal cells or dual vertices */

    else
      bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

  }
  else
    bft_error(__FILE__, __LINE__, 0, _err_not_handled, __func__);

}
/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the average of a function on the faces
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_average_on_faces_by_value(cs_flag_t          dof_flag,
                                      const cs_xdef_t   *def,
                                      double             retval[])
{
  CS_UNUSED(dof_flag);

  /* Sanity checks */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(cs_flag_test(dof_flag, cs_flag_primal_face));
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  const cs_cdo_quantities_t  *quant = cs_cdo_quant;
  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);
  const cs_real_t  *input = (cs_real_t *)def->input;

  cs_range_set_t  *rs = NULL;

  switch (def->dim) {

  case 1: /* Scalar-valued */
    {
      assert(dof_flag & CS_FLAG_SCALAR);
      rs = cs_cdo_connect->range_sets[CS_CDO_CONNECT_FACE_SP0];

      if (def->meta & CS_FLAG_FULL_LOC) {
#       pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
        for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
          retval[f_id] = input[0];
      }
      else
        _pfsp_by_value(input[0], z->n_elts, z->elt_ids, retval);
    }
    break;

  case 3: /* Vector-valued */
    {
      assert(dof_flag & CS_FLAG_VECTOR);
      rs = cs_cdo_connect->range_sets[CS_CDO_CONNECT_FACE_VP0];

      if (def->meta & CS_FLAG_FULL_LOC) {
#       pragma omp parallel for if (quant->n_faces > CS_THR_MIN)
        for (cs_lnum_t f_id = 0; f_id < quant->n_faces; f_id++)
          memcpy(retval + 3*f_id, input, 3*sizeof(double));
      }
      else
        _pfvp_by_value(input, z->n_elts, z->elt_ids, retval);
    }
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Invalid dimension of analytical function.\n"), __func__);
    break;

  } /* End of switch on the dimension */

  if (cs_glob_n_ranks > 1)
    cs_range_set_sync(rs, CS_DOUBLE, def->dim, (void *)retval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the average of a function on the faces
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_average_on_faces_by_analytic(cs_flag_t          dof_flag,
                                         const cs_xdef_t   *def,
                                         double             retval[])
{
  CS_UNUSED(dof_flag);

  /* Sanity checks */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);
  assert(cs_flag_test(dof_flag, cs_flag_primal_face));

  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  cs_range_set_t  *rs = NULL;
  cs_quadrature_tria_integral_t  *qfunc = NULL;
  cs_xdef_analytic_input_t *anai = (cs_xdef_analytic_input_t *)def->input;

  switch (def->dim) {

  case 1: /* Scalar-valued */
    {
      assert(dof_flag & CS_FLAG_SCALAR);
      rs = cs_cdo_connect->range_sets[CS_CDO_CONNECT_FACE_SP0];

      switch (def->qtype) {

        /* Barycenter of the cell or of the tetrahedral subdiv. */
      case CS_QUADRATURE_BARY:
      case CS_QUADRATURE_BARY_SUBDIV:
        qfunc = cs_quadrature_tria_1pt_scal;
        break;

        /* Quadrature with a unique weight */
      case CS_QUADRATURE_HIGHER:
        qfunc = cs_quadrature_tria_3pts_scal;
        break;

        /* Most accurate quadrature available */
      case CS_QUADRATURE_HIGHEST:
        qfunc = cs_quadrature_tria_4pts_scal;
        break;

      default:
        bft_error(__FILE__, __LINE__, 0, _err_quad, __func__);
        break;

      } /* Which type of quadrature to use */

      _pfsa_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids, qfunc,
                        retval);

    }
    break;

  case 3: /* Vector-valued */
    {
      assert(dof_flag & CS_FLAG_VECTOR);
      rs = cs_cdo_connect->range_sets[CS_CDO_CONNECT_FACE_VP0];

      switch (def->qtype) {

        /* Barycenter of the cell or of the tetrahedral subdiv. */
      case CS_QUADRATURE_BARY:
      case CS_QUADRATURE_BARY_SUBDIV:
        qfunc = cs_quadrature_tria_1pt_vect;
        break;

        /* Quadrature with a unique weight */
      case CS_QUADRATURE_HIGHER:
        qfunc = cs_quadrature_tria_3pts_vect;
        break;

        /* Most accurate quadrature available */
      case CS_QUADRATURE_HIGHEST:
        qfunc = cs_quadrature_tria_4pts_vect;
        break;

      default:
        bft_error(__FILE__, __LINE__, 0, _err_quad, __func__);
        break;

      } /* Which type of quadrature to use */

      _pfva_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids, qfunc,
                        retval);
    }
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Invalid dimension of analytical function.\n"), __func__);

  } /* End of switch on dimension */

  if (cs_glob_n_ranks > 1)
    cs_range_set_sync(rs, CS_DOUBLE, def->dim, (void *)retval);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the average of a function on the cells
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_average_on_cells_by_value(cs_flag_t          dof_flag,
                                      const cs_xdef_t   *def,
                                      double             retval[])
{
  CS_UNUSED(dof_flag);

  /* Sanity checks */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);
  assert(cs_flag_test(dof_flag, cs_flag_primal_cell));

  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);
  const cs_real_t  *input = (cs_real_t *)def->input;

  switch (def->dim) {

  case 1: /* Scalar-valued */
    assert(dof_flag & CS_FLAG_SCALAR);
    _pcsa_by_value(input[0], z->n_elts, z->elt_ids, retval);
    break;

  case 3: /* Vector-valued */
    assert(dof_flag & CS_FLAG_VECTOR);
    _pcva_by_value(input, z->n_elts, z->elt_ids, retval);
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Invalid dimension of analytical function.\n"), __func__);
    break;

  } /* End of switch on dimension */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the average of a function on the cells
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_average_on_cells_by_array(cs_flag_t          dof_flag,
                                      const cs_xdef_t   *def,
                                      double             retval[])
{
  CS_UNUSED(dof_flag);

  /* Sanity checks */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(cs_flag_test(dof_flag, cs_flag_primal_cell));
  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);

  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);
  const cs_xdef_array_input_t *array_input =
    (cs_xdef_array_input_t *)def->input;
  const int stride = array_input->stride;
  const double * val = array_input->values;

  if (stride == 1) { /* Scalar */

    if (def->meta & CS_FLAG_FULL_LOC) {
#     pragma omp parallel for if (cs_cdo_quant->n_cells > CS_THR_MIN)
      for (cs_lnum_t c_id = 0; c_id < cs_cdo_quant->n_cells; c_id++)
        /* A global memcpy could work too */
        retval[c_id] = val[c_id];
    }
    else {

      assert(z->elt_ids != NULL);
#     pragma omp parallel for if (z->n_elts > CS_THR_MIN)
      for (cs_lnum_t i = 0; i < z->n_elts; i++) {
        const cs_lnum_t  c_id = z->elt_ids[i];
        retval[c_id] = val[c_id];
      }

    } /* Perform on the full location */

  }
  else { /* Not scalar-valued */

    if (def->meta & CS_FLAG_FULL_LOC) {
#     pragma omp parallel for if (cs_cdo_quant->n_cells > CS_THR_MIN)
      for (cs_lnum_t c_id = 0; c_id < cs_cdo_quant->n_cells; c_id++) {
        /* A global memcpy could work too */
        memcpy(retval + stride*c_id, val + stride*c_id, stride*sizeof(double));
      }
    }
    else {

      assert(z->elt_ids != NULL);
#     pragma omp parallel for if (z->n_elts > CS_THR_MIN)
      for (cs_lnum_t i = 0; i < z->n_elts; i++) {
        const cs_lnum_t  c_id = z->elt_ids[i];
        memcpy(retval + stride*c_id, val + stride*c_id, stride*sizeof(double));
      }

    } /* Perform on the full location */

  } /* Switch on stride */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Evaluate the average of a function on the cells
 *
 * \param[in]      dof_flag  indicate where the evaluation has to be done
 * \param[in]      def       pointer to a cs_xdef_t pointer
 * \param[in, out] retval    pointer to the computed values
 */
/*----------------------------------------------------------------------------*/

void
cs_evaluate_average_on_cells_by_analytic(cs_flag_t          dof_flag,
                                         const cs_xdef_t   *def,
                                         double             retval[])
{
  /* Sanity checks */
  if (retval == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_array, __func__);

  assert(def != NULL);
  assert(def->support == CS_XDEF_SUPPORT_VOLUME);
  assert(cs_flag_test(dof_flag, cs_flag_primal_cell));

  const cs_zone_t  *z = cs_volume_zone_by_id(def->z_id);

  cs_quadrature_tetra_integral_t  *qfunc = NULL;
  cs_xdef_analytic_input_t *anai = (cs_xdef_analytic_input_t *)def->input;

  switch (def->dim) {

  case 1: /* Scalar-valued */
    {
      assert(dof_flag & CS_FLAG_SCALAR);

      switch (def->qtype) {

        /* Barycenter of the cell or the tetrahedral subdiv. */
      case CS_QUADRATURE_BARY:
      case CS_QUADRATURE_BARY_SUBDIV:
        qfunc = cs_quadrature_tet_1pt_scal;
        break;

        /* Quadrature with a unique weight */
      case CS_QUADRATURE_HIGHER:
        qfunc = cs_quadrature_tet_4pts_scal;
        break;

        /* Most accurate quadrature available */
      case CS_QUADRATURE_HIGHEST:
        qfunc = cs_quadrature_tet_5pts_scal;
        break;

      default:
        bft_error(__FILE__, __LINE__, 0, _err_quad, __func__);
        break;

      } /* Which type of quadrature to use */

      _pcsa_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids, qfunc,
                        retval);
    }
    break;

  case 3: /* Vector-valued */
    {
      assert(dof_flag & CS_FLAG_VECTOR);

      switch (def->qtype) {
        /* Barycenter of the cell or the tetrahedral subdiv. */
      case CS_QUADRATURE_BARY:
      case CS_QUADRATURE_BARY_SUBDIV:
        qfunc = cs_quadrature_tet_1pt_vect;
        break;

        /* Quadrature with a unique weight */
      case CS_QUADRATURE_HIGHER:
        qfunc = cs_quadrature_tet_4pts_vect;
        break;

        /* Most accurate quadrature available */
      case CS_QUADRATURE_HIGHEST:
        qfunc = cs_quadrature_tet_5pts_vect;
        break;

      default:
        bft_error(__FILE__, __LINE__, 0, _err_quad, __func__);

      } /* Which type of quadrature to use */

      _pcva_by_analytic(anai->func, anai->input, z->n_elts, z->elt_ids, qfunc,
                        retval);
    }
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Invalid dimension of analytical function.\n"), __func__);
    break;

  } /* End of switch on the dimension */
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
