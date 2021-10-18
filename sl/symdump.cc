/*
 * Copyright (C) 2010 Kamil Dudka <kdudka@redhat.com>
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
#include "symdump.hh"

#include <cl/code_listener.h>
#include <cl/cldebug.hh>
#include <cl/clutil.hh>

#include "symheap.hh"
#include "symplot.hh"
#include "symtrace.hh"

#include <iostream>

using std::cout;

void dump_plot_core(const SymHeapCore *core, const char *name)
{
    if (!core) {
        cout << "dump_plot: error: got a NULL pointer\n";
        return;
    }

    const SymHeap *sh = dynamic_cast<const SymHeap *>(core);
    if (!sh) {
        cout << "dump_plot: error: failed to downcast SymHeapCore to SymHeap\n";
        return;
    }

    // paralyze SymHeap self-checks while plotting from a debugger
    ProtectionIntrusion intrudor;

    // attempt to plot heap
    if (!plotHeap(*sh, name))
        cout << "dump_plot: warning: call of SymPlot::plot() has failed\n";
}

void sl_dump(const SymHeapCore *sh)
{
    dump_plot_core(sh, "dump_plot");
}

void sl_dump(const SymHeapCore &sh, const char *name)
{
    dump_plot_core(&sh, name);
}

void sl_dump(const SymHeapCore &sh)
{
    sl_dump(&sh);
}

void sl_dump(Trace::Node *endPoint)
{
    Trace::plotTrace(endPoint, "dump_trace");
}

/// dummy function to pull all symbols from a static library
void initSymDump(TStorRef stor, int i)
{
    if (!i)
        return;

    const SymHeapCore sh(stor, 0);
    sl_dump(&sh);
    sl_dump(sh);
    sl_dump(sh, static_cast<const char *>(0));
    sl_dump(static_cast<Trace::Node *>(0));
}

void initSymDump(TStorRef stor)
{
    initSymDump(stor, 0);
}
