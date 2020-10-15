/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2020, by the GROMACS development team, led by
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
#include "gmxpre.h"

#include "pullcoordexpressionparser.h"

#include "config.h"

#include "gromacs/utility/arrayref.h"
#include "gromacs/utility/exceptions.h"
#include "gromacs/utility/gmxassert.h"
#include "gromacs/utility/stringutil.h"

#include "pull_internal.h"

namespace gmx
{

double PullCoordExpressionParser::evaluate(ArrayRef<const double> variables)
{
#if HAVE_MUPARSER
    if (!parser_)
    {
        initializeParser(variables.size());
    }
    GMX_ASSERT(variables.size() == variableValues_.size(),
               "The size of variables should match the size passed at the first call of this "
               "method");
    // Todo: consider if we can use variableValues_ directly without the extra variables buffer
    std::copy(variables.begin(), variables.end(), variableValues_.begin());

    return parser_->Eval();
#else
    GMX_UNUSED_VALUE(variables);

    GMX_RELEASE_ASSERT(false, "evaluate() should not be called without muparser");

    return 0;
#endif
}

void PullCoordExpressionParser::initializeParser(int numVariables)
{
#if HAVE_MUPARSER
    parser_ = std::make_unique<mu::Parser>();
    parser_->SetExpr(expression_);
    variableValues_.resize(numVariables);
    for (int n = 0; n < numVariables; n++)
    {
        variableValues_[n] = 0;
        std::string name   = "x" + std::to_string(n + 1);
        parser_->DefineVar(name, &variableValues_[n]);
    }
#else
    GMX_UNUSED_VALUE(numVariables);
    GMX_RELEASE_ASSERT(false, "Can not use transformation pull coordinate without muparser");
#endif
}

} // namespace gmx
