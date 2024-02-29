// TODO header?
#ifndef H_GUARD_SMG2JSON_H
#define H_GUARD_SMG2JSON_H

/**
 * @file smg2json
 * SMG 2 json - saves current SMG state into a json
 */

#include "symheap.hh"

#include <string>

/// save a json file "smg-NNNN.json" of the current SMG state
bool smg2json(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc = 0);

#endif /* H_GUARD_SMG2JSON_H */
