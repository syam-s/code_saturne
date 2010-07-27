/*============================================================================
 *
 *     This file is part of the Code_Saturne Kernel, element of the
 *     Code_Saturne CFD tool.
 *
 *     Copyright (C) 1998-2010 EDF S.A., France
 *
 *     contact: saturne-support@edf.fr
 *
 *     The Code_Saturne Kernel is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public License
 *     as published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     The Code_Saturne Kernel is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with the Code_Saturne Kernel; if not, write to the
 *     Free Software Foundation, Inc.,
 *     51 Franklin St, Fifth Floor,
 *     Boston, MA  02110-1301  USA
 *
 *============================================================================*/

/*============================================================================
 * Map helper structure
 *============================================================================*/

#if defined(HAVE_CONFIG_H)
#include "cs_config.h"
#endif

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*----------------------------------------------------------------------------
 * BFT library headers
 *----------------------------------------------------------------------------*/

#include <bft_mem.h>
#include <bft_error.h>
#include <bft_printf.h>

/*----------------------------------------------------------------------------
 * FVM library headers
 *----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Local headers
 *----------------------------------------------------------------------------*/

#include "cs_base.h"

/*----------------------------------------------------------------------------
 *  Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_map.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Local Macro Definitions
 *============================================================================*/

/*=============================================================================
 * Local Structure Definitions
 *============================================================================*/

/* Basic Map structure */
/*---------------------*/

struct _cs_map_name_to_id_t {

  size_t      size;             /* Number of entries */
  size_t      max_size;         /* Maximum number of entries */

  size_t      max_keys_size;    /* Maximum size for keys buffer */
  size_t      keys_size;        /* Size of keys buffer */
  char       *keys;             /* Key buffer */

  char      **key;              /* Pointer to keys */
  int        *id;               /* Matching id */
};

/*============================================================================
 *  Global variables
 *============================================================================*/

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Insert new key.
 *
 * parameters:
 *   m     <-> pointer to map structure
 *   key   <-- character string (key)
 *   id    <-- id associated with key
 *   index <-- index of key in map
 *----------------------------------------------------------------------------*/

static void
_name_to_id_insert_key(cs_map_name_to_id_t  *m,
                       const char           *key,
                       int                   id,
                       size_t                index)
{
  size_t i;
  size_t key_size = strlen(key);

  /* Resize map arrays if necessary */

  if (m->size >= m->max_size) {

    size_t prev_size = m->max_size;

    m->max_size*= 2;
    BFT_REALLOC(m->key, m->max_size, char *);
    BFT_REALLOC(m->id, m->max_size, int);

    for (i = prev_size; i < m->max_size; i++) {
      m->key[i] = NULL;
      m->id[i] = -1;
    }
  }

  if (m->keys_size + key_size + 1 >= m->max_keys_size) {

    size_t min_size = m->keys_size + key_size + 1;
    size_t prev_size = m->max_keys_size;

    m->max_keys_size*= 2;
    if (m->max_keys_size < min_size)
      m->max_keys_size = min_size;

    BFT_REALLOC(m->keys, m->max_keys_size, char);

    for (i = prev_size; i < m->max_keys_size; i++)
      m->keys[i] = '\0';
  }

  /* Shift previous data */

  for (i = m->size; i > index; i--) {
    m->key[i] = m->key[i-1];
    m->id[i] = m->id[i-1];
  }

  /* Insert data */

  strcpy(m->keys + m->keys_size, key);

  m->key[index] = m->keys + m->keys_size;
  m->id[index] = id;

  m->keys_size += key_size + 1;

  m->size += 1;
}

/*============================================================================
 * Public function definitions for Fortran API
 *============================================================================*/

/*============================================================================
 * Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Create empty name to id map.
 *
 * returns:
 *   pointer to newly initialized map structure.
 *----------------------------------------------------------------------------*/

cs_map_name_to_id_t *
cs_map_name_to_id_create(void)
{
  cs_map_name_to_id_t *m = NULL;

  BFT_MALLOC(m, 1, cs_map_name_to_id_t);

  m->size = 0;
  m->max_size = 8;

  m->max_keys_size = 128;
  m->keys_size = 0;

  BFT_MALLOC(m->keys, m->max_keys_size, char);

  BFT_MALLOC(m->key, m->max_size, char *);
  BFT_MALLOC(m->id, m->max_size, int);

  return m;
}

/*----------------------------------------------------------------------------
 * Destroy name to id map structure.
 *
 * parameters:
 *   m <-> pointer to map structure.
 *----------------------------------------------------------------------------*/

void
cs_map_name_to_id_destroy(cs_map_name_to_id_t **m)
{
  if (m != NULL) {

    cs_map_name_to_id_t *_m = *m;

    BFT_FREE(_m->id);
    BFT_FREE(_m->key);

    BFT_FREE(_m->keys);

    BFT_FREE(*m);
  }
}

/*----------------------------------------------------------------------------
 * Find id matching a key, inserting key if not already present.
 *
 * parameters:
 *   m     <-> pointer to map structure
 *   key   <-- character string (key)
 *
 * returns:
 *   id matching key (already present or newly inserted)
 *----------------------------------------------------------------------------*/

int
cs_map_name_to_id(cs_map_name_to_id_t  *m,
                  const char           *key)
{
  int start_id, end_id, mid_id;
  int cmp_ret = 1;

  /* Use binary search to find entry */

  start_id = 0;
  end_id = m->size - 1;
  mid_id = start_id + ((end_id -start_id) / 2);

  while (start_id <= end_id) {
    cmp_ret = strcmp(m->key[mid_id], key);
    if (cmp_ret < 0)
      start_id = mid_id + 1;
    else if (cmp_ret > 0)
      end_id = mid_id - 1;
    else
      break;
    mid_id = start_id + ((end_id -start_id) / 2);
  }

  /* If not found, insert key */

  if (cmp_ret != 0)
    _name_to_id_insert_key(m, key, m->size, mid_id);

  return m->id[mid_id];
}

/*----------------------------------------------------------------------------
 * Return the size of a map.
 *
 * parameters:
 *   m <-- pointer to map structure.
 *
 * returns:
 *   number of entries in map.
 *----------------------------------------------------------------------------*/

size_t
cs_map_name_to_id_size(const cs_map_name_to_id_t *m)
{
  size_t retval = 0;

  if (m != NULL)
    retval = m->size;

  return retval;
}

/*----------------------------------------------------------------------------
 * Return a given key in a map.
 *
 * parameters:
 *   m  <-- pointer to map structure.
 *   id <-- key id
 *
 * returns:
 *   pointer to key.
 *----------------------------------------------------------------------------*/

const char *
cs_map_name_to_id_key(const cs_map_name_to_id_t  *m,
                      size_t                      id)
{
  const char *retval = NULL;

  if (m == NULL)
    return retval;

  if (id < m->size)
    retval = (const char *)(m->key[id]);

  return retval;
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
