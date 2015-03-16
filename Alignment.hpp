#ifndef AS_ALIGNMENT_HPP
#define AS_ALIGNMENT_HPP

#include "Types.hpp"

/*
 * This class represents a sequencing alignment.
 */

struct Alignment
{
    std::string id;

    // If this field is false, no assumption can be made to other fields
    bool mapped;

    // The starting position of the alignment
    Position start;
    
	std::string seq;
};

#endif