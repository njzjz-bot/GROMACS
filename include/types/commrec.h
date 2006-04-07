/*
 * $Id$
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * GRoups of Organic Molecules in ACtion for Science
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GMX_MPI
#include <mpi.h>
#endif

#include "idef.h"

#define DD_MAXCELL  8
#define DD_MAXICELL 4

typedef struct {
  /* The global charge group division */
  int  *ncg;     /* Number of home charge groups for each node */
  int  *index;   /* Index of nnodes+1 into cg */
  int  *cg;      /* Global charge group index */
  int  *nat;     /* Number of home atoms for each node. */
  int  *ibuf;    /* Buffer for communication */
} gmx_domdec_master_t;

typedef struct {
  /* The cell (node) we communicate with */
  int cell;
  int ncg;
  int nat;
  /* Index of size ncg into the global charge groups */
  int *index_gl;
  /* Index of size ncg into the local charge groups */
  int *index;
  int nalloc;
} gmx_domdec_comm_t;

typedef struct {
  int  j0;       /* j-cell start               */
  int  j1;       /* j-cell end                 */
  int  cg1;      /* i-charge-group end         */
  int  jcg0;     /* j-charge-group start       */
  int  jcg1;     /* j-charge-group end         */
  ivec shift0;   /* Minimum shifts to consider */
  ivec shift1;   /* Maximum shifts to consider */
} gmx_domdec_ns_t;

typedef struct {
  int     ftype;   /* the function type */
  t_iatom *iatoms; /* the iatom pointer */
} gmx_at2ilist_t;

typedef struct {
  int *index;
  gmx_at2ilist_t *il;
} gmx_reverse_top_t;

typedef struct {
  int     cell;
  atom_id a;
} gmx_ga2la_t;

typedef struct {
  int nsend;
  int *a;
  int a_nalloc;
  int nrecv;
} gmx_conatomsend_t;

typedef struct {
  /* The number of constraints in the whole system */
  int  ncon_global;
  /* The number of flexible constraints in the whole system */
  int  nflexcon_global;
  /* Index from global atom numbers to global constraints */
  t_block at2con;
  /* A pointer to the global iatoms array for the constraints */
  t_iatom *iatoms;
  /* The fully local and connected constraints */
  int  ncon;
  int  *con;
  int  *con_nlocat;
  int  con_nalloc;
  /* Global to local constraint index */
  int  *gc2lc;
  /* The atom indices we need from the surrounding cells */
  int  nind_req;
  int  *ind_req;
  int  ind_req_nalloc;
  /* Global to local communicated constraint atom only index */
  int  *ga2la;
  /* The number of indices to receive during the setup */
  int  nreq[DIM][2][2];
  /* The atoms to send */
  gmx_conatomsend_t cas[DIM][2];
  bool *bSendAtom;
  int   bSendAtom_nalloc;
  /* Send buffers */
  int  *ibuf;
  rvec *vbuf;
  int  buf_nalloc;
} gmx_domdec_constraints_t;

typedef struct {
  int nodeid;
  int nnodes;
  int masterrank;
#ifdef GMX_MPI
  MPI_Comm all;
#endif

  /* The communication setup, identical for each cell */
  ivec nc;
  int  ndim;
  ivec dim;
  /* Forward and backward neighboring cells */
  int  neighbor[DIM][2];
  /* The bonded and non-bonded communication setup */
  int  ncell;
  ivec shift[DD_MAXCELL];

  /* Only available on the master node */
  gmx_domdec_master_t ma;
  /* Switch that tells if the master has the charge group distribution */
  bool bMasterHasAllCG;

  /* Global atom number to interaction list */
  gmx_reverse_top_t reverse_top;
  int  nbonded_global;
  int  nbonded_local;

  /* Constraint stuff */
  gmx_domdec_constraints_t *constraints;

  /* The following arrays will have size ncell */
  /* Nodes we need to send coordinates to and receive forces from */
  gmx_domdec_comm_t comm0[DD_MAXCELL];
  /* Nodes we need to receive coordinates from and send forces to */
  gmx_domdec_comm_t comm1[DD_MAXCELL];

  /* Local charge group index */
  int  ncg_tot;
  int  *cgindex;
  int  cgindex_nalloc;

  /* Index from the local atoms to the global atoms */
  int  nat_tot;
  /* The total number of atoms, including the extra ones for constraints */
  int  nat_tot_con;
  int  *gatindex;
  int  gatindex_nalloc;

  /* Global atom number to local atom number, -1 if not local */
  gmx_ga2la_t *ga2la;

  /* For neighborsearching */
  int  nicell;
  gmx_domdec_ns_t icell[DD_MAXICELL];

  /* Communication buffers */
  int  *buf_i1;
  int  nalloc_i1;
  int  *buf_i2;
  int  nalloc_i2;
  rvec *buf_vs;
  int  nalloc_vs;
  rvec *buf_vr;
  int  nalloc_vr;
  
  /* MPI requests for move_x */
  int  nmpi_req;
#ifdef GMX_MPI
  MPI_Request mpi_req[(DD_MAXCELL-1)*2];
#endif
} gmx_domdec_t;

typedef struct {
  int nsim;
  int sim;
#ifdef GMX_MPI
  MPI_Group mpi_group_masters;
  MPI_Comm mpi_comm_masters;
#endif
} gmx_multisim_t;

typedef struct {
  /* The nodids in one sim are numbered sequentially from 0.
   * All communication within some simulation should happen
   * in mpi_comm_mysim, or its subset mpi_comm_mygroup.
   */
  int nodeid,nnodes,npmenodes;
  int left,right;
  int threadid,nthreads;
#ifdef GMX_MPI
  MPI_Comm mpi_comm_mysim;
  MPI_Comm mpi_comm_mygroup;
#endif
  gmx_domdec_t *dd;

  gmx_multisim_t *ms;
} t_commrec;

#define MASTERNODE(cr)     ((cr)->nodeid == 0)
#define MASTERTHREAD(cr)   ((cr)->threadid == 0)
#define MASTER(cr)         (MASTERNODE(cr) && MASTERTHREAD(cr))
#define NODEPAR(cr)        ((cr)->nnodes > 1)
#define THREADPAR(cr)      ((cr)->nthreads > 1)
#define PAR(cr)            (NODEPAR(cr) || THREADPAR(cr))
#define RANK(cr,nodeid)    (nodeid)
#define MASTERRANK(cr)     (0)

#define DOMAINDECOMP(cr)   ((cr)->dd != NULL)

#define DDMASTER(dd)       ((dd)->nodeid == 0)
#define DDRANK(dd,nodeid)  (nodeid)
#define DDMASTERRANK(dd)   (0)

#define MULTISIM(cr)       ((cr)->ms)
#define MSRANK(ms,nodeid)  (nodeid)
#define MASTERSIM(ms)      ((ms)->sim == 0)

/* Parallel and/or multi simulation */
#define MULTIMASTER(cr)    ((cr)->nodeid == 0)
#define MULTIPAR(cr)       (PAR(cr) || MULTISIM(cr))

/*
 * with the help of this macro + enum we are able to find out 
 * what type of pme work the local node has to do 
 *
 * pmeduty = 0 if node does pp only
 *         = 1 if node does pme only
 *         = 2 if ALL nodes do both (no PME/PP node splitting)
 */ 
#define pmeduty(cr)        ((!cr->npmenodes)? 2:(cr->nodeid >= cr->nnodes-cr->npmenodes))

enum {
  epmePPONLY, epmePMEONLY, epmePMEANDPP
};
