/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2009, The GROMACS development team,
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
 */
/*! \internal \file
 * \brief
 * Implements gmx::Selection.
 *
 * \author Teemu Murtola <teemu.murtola@cbr.su.se>
 * \ingroup module_selection
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <smalloc.h>
#include <statutil.h>
#include <string2.h>
#include <xvgr.h>

#include "gromacs/selection/position.h"
#include "gromacs/selection/selection.h"
#include "gromacs/selection/selvalue.h"

#include "selelem.h"

namespace gmx
{

Selection::Selection(t_selelem *elem, const char *selstr)
    : name_(elem->name), selectionText_(selstr),
      mass_(NULL), charge_(NULL), originalMass_(NULL), originalCharge_(NULL),
      rootElement_(elem), coveredFractionType_(CFRAC_NONE),
      coveredFraction_(1.0), averageCoveredFraction_(1.0),
      bDynamic_(false), bDynamicCoveredFraction_(false)
{
    gmx_ana_pos_clear(&rawPositions_);

    if (elem->child->type == SEL_CONST)
    {
        // TODO: This is not exception-safe if any called function throws.
        gmx_ana_pos_copy(&rawPositions_, elem->child->v.u.p, true);
    }
    else
    {
        t_selelem *child;

        child = elem->child;
        child->flags     &= ~SEL_ALLOCVAL;
        _gmx_selvalue_setstore(&child->v, &rawPositions_);
        /* We should also skip any modifiers to determine the dynamic
         * status. */
        while (child->type == SEL_MODIFIER)
        {
            child = child->child;
            if (child->type == SEL_SUBEXPRREF)
            {
                child = child->child;
                /* Because most subexpression elements are created
                 * during compilation, we need to check for them
                 * explicitly here.
                 */
                if (child->type == SEL_SUBEXPR)
                {
                    child = child->child;
                }
            }
        }
        /* For variable references, we should skip the
         * SEL_SUBEXPRREF and SEL_SUBEXPR elements. */
        if (child->type == SEL_SUBEXPRREF)
        {
            child = child->child->child;
        }
        bDynamic_ = (child->child->flags & SEL_DYNAMIC);
    }
    initCoveredFraction(CFRAC_NONE);
}


Selection::~Selection()
{
    gmx_ana_pos_deinit(&rawPositions_);
    if (mass_ != originalMass_)
    {
        sfree(mass_);
    }
    if (charge_ != originalCharge_)
    {
        sfree(charge_);
    }
    sfree(originalMass_);
    sfree(originalCharge_);
}


void
Selection::printInfo(FILE *fp) const
{
    fprintf(fp, "\"%s\" (%d position%s, %d atom%s%s)", name_.c_str(),
            posCount(),  posCount()  == 1 ? "" : "s",
            atomCount(), atomCount() == 1 ? "" : "s",
            isDynamic() ? ", dynamic" : "");
    fprintf(fp, "\n");
}


bool
Selection::initCoveredFraction(e_coverfrac_t type)
{
    coveredFractionType_ = type;
    if (type == CFRAC_NONE || rootElement_ == NULL)
    {
        bDynamicCoveredFraction_ = false;
    }
    else if (!_gmx_selelem_can_estimate_cover(rootElement_))
    {
        coveredFractionType_ = CFRAC_NONE;
        bDynamicCoveredFraction_ = false;
    }
    else
    {
        bDynamicCoveredFraction_ = true;
    }
    coveredFraction_ = bDynamicCoveredFraction_ ? 0.0 : 1.0;
    averageCoveredFraction_ = coveredFraction_;
    return type == CFRAC_NONE || coveredFractionType_ != CFRAC_NONE;
}


void
Selection::printDebugInfo(FILE *fp, int nmaxind) const
{
    const gmx_ana_pos_t &p = rawPositions_;

    fprintf(fp, "  ");
    printInfo(fp);
    fprintf(fp, "    ");
    gmx_ana_index_dump(fp, p.g, -1, nmaxind);

    fprintf(fp, "    Block (size=%d):", p.m.mapb.nr);
    if (!p.m.mapb.index)
    {
        fprintf(fp, " (null)");
    }
    else
    {
        int n = p.m.mapb.nr;
        if (nmaxind >= 0 && n > nmaxind)
            n = nmaxind;
        for (int i = 0; i <= n; ++i)
            fprintf(fp, " %d", p.m.mapb.index[i]);
        if (n < p.m.mapb.nr)
            fprintf(fp, " ...");
    }
    fprintf(fp, "\n");

    int n = posCount();
    if (nmaxind >= 0 && n > nmaxind)
        n = nmaxind;
    fprintf(fp, "    RefId:");
    if (!p.m.refid)
    {
        fprintf(fp, " (null)");
    }
    else
    {
        for (int i = 0; i < n; ++i)
            fprintf(fp, " %d", p.m.refid[i]);
        if (n < posCount())
            fprintf(fp, " ...");
    }
    fprintf(fp, "\n");

    fprintf(fp, "    MapId:");
    if (!p.m.mapid)
    {
        fprintf(fp, " (null)");
    }
    else
    {
        for (int i = 0; i < n; ++i)
            fprintf(fp, " %d", p.m.mapid[i]);
        if (n < posCount())
            fprintf(fp, " ...");
    }
    fprintf(fp, "\n");
}


void
Selection::initializeMassesAndCharges(const t_topology *top)
{
    snew(originalMass_,   posCount());
    snew(originalCharge_, posCount());
    for (int b = 0; b < posCount(); ++b)
    {
        originalCharge_[b] = 0;
        if (top)
        {
            originalMass_[b] = 0;
            for (int i = rawPositions_.m.mapb.index[b];
                     i < rawPositions_.m.mapb.index[b+1];
                     ++i)
            {
                int index = rawPositions_.g->index[i];
                originalMass_[b]   += top->atoms.atom[index].m;
                originalCharge_[b] += top->atoms.atom[index].q;
            }
        }
        else
        {
            originalMass_[b] = 1;
        }
    }
    if (isDynamic() && !hasFlag(efDynamicMask))
    {
        snew(mass_,   posCount());
        snew(charge_, posCount());
        for (int b = 0; b < posCount(); ++b)
        {
            mass_[b]   = originalMass_[b];
            charge_[b] = originalCharge_[b];
        }
    }
    else
    {
        mass_   = originalMass_;
        charge_ = originalCharge_;
    }
}


void
Selection::refreshMassesAndCharges()
{
    if (mass_ != originalMass_)
    {
        for (int i = 0; i < posCount(); ++i)
        {
            int refid  = rawPositions_.m.refid[i];
            mass_[i]   = originalMass_[refid];
            charge_[i] = originalCharge_[refid];
        }
    }
}


void
Selection::updateCoveredFractionForFrame()
{
    if (isCoveredFractionDynamic())
    {
        real cfrac = _gmx_selelem_estimate_coverfrac(rootElement_);
        coveredFraction_ = cfrac;
        averageCoveredFraction_ += cfrac;
    }
}


void
Selection::computeAverageCoveredFraction(int nframes)
{
    if (isCoveredFractionDynamic() && nframes > 0)
    {
        averageCoveredFraction_ /= nframes;
    }
}


void
Selection::restoreOriginalPositions()
{
    if (isDynamic())
    {
        gmx_ana_pos_t &p = rawPositions_;
        gmx_ana_index_copy(p.g, rootElement_->v.u.g, false);
        p.g->name = NULL;
        gmx_ana_indexmap_update(&p.m, p.g, hasFlag(gmx::efDynamicMask));
        p.nr = p.m.nr;
    }
}

} // namespace gmx
