/*
 * Copyright (C) 2010-2011 Kamil Dudka <kdudka@redhat.com>
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


#ifndef H_GUARD_SYM_PLOT_H
#define H_GUARD_SYM_PLOT_H

/**
 * @file symplot.hh
 * SymPlot - symbolic heap plotter
 */

#include "symheap.hh"
#include "worklist.hh"
#include <cl/clutil.hh>

#include <string>

class HeapCrawler {
    public:
        HeapCrawler(const SymHeap &sh, const bool digForward = true):
            sh_(const_cast<SymHeap &>(sh)),
            digForward_(digForward)
        {
        }

        bool /* anyChange */ digObj(const TObjId);
        bool /* anyChange */ digVal(const TValId);

        const TObjSet objs() const { return objs_; }
        const TValSet vals() const { return vals_; }

    private:
        void digFields(const TObjId of);
        void operate();

    private:
        SymHeap                    &sh_;
        WorkList<TValId>            wl_;
        bool                        digForward_;
        TObjSet                     objs_;
        TValSet                     vals_;
};

enum EFieldClass {
    FC_VOID = 0,
    FC_PTR,
    FC_NEXT,
    FC_PREV,
    FC_DATA
};

struct FieldWrapper {
    FldHandle       fld;
    EFieldClass     code;

    FieldWrapper():
        code(FC_VOID)
    {
    }

    FieldWrapper(const FldHandle &obj_, EFieldClass code_):
        fld(obj_),
        code(code_)
    {
    }

    FieldWrapper(const FldHandle &obj_):
        fld(obj_),
        code(isDataPtr(fld.type())
            ? FC_PTR
            : FC_DATA)
    {
    }
};


typedef std::set<int>                   TIdSet;

/// create a plot named "name-NNNN.dot", starting from all live objects
bool plotHeap(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc = 0,
        std::string                     *pName = 0,
        const TIdSet                    *pHighlight = 0);

/// create a plot named "name-NNNN.dot", starting from the given starting points
bool plotHeap(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc,
        const TValList                  &startingPoints);

/// create a plot named "name-NNNN.dot", containing @b only the given objects
bool plotHeap(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc,
        const TObjSet                   &objs);

#endif /* H_GUARD_SYM_PLOT_H */
