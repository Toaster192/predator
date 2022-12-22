/*
 * Copyright (C) 2011-2022 Kamil Dudka <kdudka@redhat.com>
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

#include "config.h"
#include "sigcatch.hh"

#include "util.hh"

#include <map>

#ifndef _NSIG
#   define _NSIG 0x100
#endif

// we define our own TSigHandler because sighandler_t is missing on Darwin
typedef void (*TSigHandler)(int);

static volatile sig_atomic_t sig_flags[_NSIG];

typedef std::map<int /* signum */, TSigHandler> TBackup;
static TBackup backup;

/// check that signum is a signal known at compile time
static bool isKnownSig(int signum)
{
    return (0 < signum)
        && (signum < _NSIG);
}

static void generic_signal_handler(int signum)
{
    if (!isKnownSig(signum))
        // out of range
        return;

    sig_flags[signum] = static_cast<sig_atomic_t>(true);
}

bool SignalCatcher::install(int signum)
{
    if (!isKnownSig(signum))
        return false;

    if (hasKey(backup, signum))
        return false;

    const TSigHandler old = signal(signum, generic_signal_handler);
    if (SIG_ERR == old)
        return false;

    ::backup[signum] = old;
    return true;
}

bool SignalCatcher::cleanup()
{
    bool ok = true;

    // uninstall signal handler
    for (TBackup::const_reference item : ::backup) {
        if (SIG_ERR == signal(item.first, item.second))
            ok = false;
    }

    if (!ok)
        // we already have a (non-recoverable) problem
        return false;

    // clear static data
    ::backup.clear();
    for (int i = 0; i < _NSIG; ++i)
        sig_flags[i] = static_cast<sig_atomic_t>(false);

    return true;
}

bool SignalCatcher::caught(int signum)
{
    if (!sig_flags[signum])
        return false;

    sig_flags[signum] = static_cast<sig_atomic_t>(false);
    return true;
}

bool SignalCatcher::caught(int *pSignum)
{
    for (TBackup::const_reference item : ::backup) {
        const int signum = item.first;
        if (!caught(signum))
            continue;

        *pSignum = signum;
        return true;
    }

    return false;
}
