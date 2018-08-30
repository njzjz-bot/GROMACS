/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2019,2020, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*!\file
 * \internal
 * \brief
 * Implements gmx::SetCenter.
 *
 * \author Paul Bauer <paul.bauer.q@gmail.com>
 * \ingroup module_coordinateio
 */

#include "gmxpre.h"

#include "setcenter.h"

#include <algorithm>

#include "gromacs/math/vec.h"
#include "gromacs/pbcutil/pbc.h"

namespace gmx
{

SetCenter::SetCenter(const Selection& center, const CenteringType& centerFlag) :
    center_(center),
    centerFlag_(centerFlag)
{
    switch (centerFlag_)
    {
        case (CenteringType::Triclinic):
        {
            guarantee_ = convertFlag(FrameConverterFlags::SystemIsCenteredInTriclinicBox);
            break;
        }
        case (CenteringType::Rectangular):
        {
            guarantee_ = convertFlag(FrameConverterFlags::SystemIsCenteredInRectangularBox);
            break;
        }
        case (CenteringType::Zero):
        {
            guarantee_ = convertFlag(FrameConverterFlags::SystemIsCenteredInZeroBox);
            break;
        }
        default: { GMX_RELEASE_ASSERT(false, "Unhandled type for Centering type");
        }
    }
}

void SetCenter::convertFrame(t_trxframe* input)
{
    RVec localBoxCenter;
    clear_rvec(localBoxCenter);
    calc_box_center(static_cast<int>(centerFlag_), input->box, localBoxCenter);
    RVec cmax = input->x[center_.position(0).refId()];
    RVec cmin = input->x[center_.position(0).refId()];
    for (int i = 0; i < center_.atomCount(); i++)
    {
        int pos = center_.position(i).refId();
        for (int m = 0; m < DIM; m++)
        {
            if (input->x[pos][m] < cmin[m])
            {
                cmin[m] = input->x[pos][m];
            }
            else if (input->x[pos][m] > cmax[m])
            {
                cmax[m] = input->x[pos][m];
            }
        }
    }
    RVec shift;
    clear_rvec(shift);
    for (int m = 0; m < DIM; m++)
    {
        shift[m] = localBoxCenter[m] - (cmin[m] + cmax[m]) * 0.5;
    }

    for (int i = 0; i < input->natoms; i++)
    {
        rvec_inc(input->x[i], shift);
    }
}

} // namespace gmx
