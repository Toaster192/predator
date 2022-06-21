/*
 * Copyright (C) 2011 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config_cl.h"

#include <cl/cl_msg.hh>
#include <cl/memdebug.hh>

#include <iomanip>

#if DEBUG_MEM_USAGE
#   include <malloc.h>

#ifndef HAVE_MALLINFO2
static bool overflowDetected;
#endif

static ssize_t peak;

bool rawMemUsage(ssize_t *pDst)
{
#ifndef HAVE_MALLINFO2
    if (::overflowDetected)
        return false;

    struct mallinfo info = mallinfo();
#else
    struct mallinfo2 info = mallinfo2();
#endif
    const ssize_t raw = info.uordblks;

#ifndef HAVE_MALLINFO2
    const unsigned mib = raw >> /* MiB */ 20;
    if (2048U < mib) {
        // mallinfo() is broken by design <https://bugzilla.redhat.com/173813>
        ::overflowDetected = true;
        return false;
    }
#endif

    *pDst = raw;
    if (peak < raw)
        // update the current peak
        peak = raw;

    return true;
}

static ssize_t memDrift;

bool initMemDrift()
{
    if (rawMemUsage(&::memDrift))
        return true;

    // failed to get current memory usage
    ::memDrift = 0U;
    return false;
}

bool currentMemUsage(ssize_t *pDst)
{
    if (!rawMemUsage(pDst))
        // failed to get current memory usage
        return false;

    *pDst -= ::memDrift;
    return true;
}

struct AmountFormatter {
    float       value;
    unsigned    width;
    unsigned    pre;

    AmountFormatter(ssize_t value_, unsigned div, unsigned dig, unsigned dec):
        value(value_),
        width(dig + 1 + dec),
        pre(dec)
    {
        const float ratio = static_cast<float>(1U << div);
        value /= ratio;
    }
};

std::ostream& operator<<(std::ostream &str, const AmountFormatter &fmt)
{
    const std::ios_base::fmtflags oldFlags = str.flags();
    const int oldPrecision = str.precision();

    using namespace std;
    str << fixed << setw(fmt.width) << setprecision(fmt.pre) << fmt.value;

    str.flags(oldFlags);
    str.precision(oldPrecision);
    return str;
}

#include <iostream>
bool printMemUsage(const char *fnc)
{
    ssize_t cb;
    if (!currentMemUsage(&cb))
        // instead of printing misleading numbers, we rather print nothing
        return false;

    CL_DEBUG("current memory usage: " << AmountFormatter(cb,
                /* MiB */ 20,
                /* int digits */ 4,
                /* dec digits */ 2)
            << " MB (just completed " << fnc << "())");

    return true;
}

bool printPeakMemUsage()
{
#ifndef HAVE_MALLINFO2
    if (::overflowDetected)
        return false;
#endif

    const ssize_t diff = ::peak - ::memDrift;
    CL_NOTE("peak memory usage: " << AmountFormatter(diff,
                /* MiB */ 20,
                /* int digits */ 0,
                /* dec digits */ 2)
            << " MB");

    return true;
}

#else // DEBUG_MEM_USAGE

bool rawMemUsage(ssize_t *)
{
    return false;
}

bool initMemDrift()
{
    return false;
}

bool currentMemUsage(ssize_t *)
{
    return false;
}

bool printMemUsage(const char *)
{
    return false;
}

bool printPeakMemUsage()
{
    return false;
}

#endif
