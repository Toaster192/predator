#ifndef H_GUARD_EXTRACT_PTA_H
#define H_GUARD_EXTRACT_PTA_H

/**
 * @file extract_pta.hh
 * TODO comment
 */

#include "symheap.hh"
#include "symplot.hh"

#include <string>

// TODO:
/// Create a file named "name-NNNN.txt", with just the points-to information of the SMG
bool extractPTA(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc = 0);

#endif /* H_GUARD_EXTRACT_PTA_H */
