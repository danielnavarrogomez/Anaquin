#ifndef GI_ALIGNMENT_HPP
#define GI_ALIGNMENT_HPP

#include "types.hpp"
#include "locus.hpp"

namespace Spike
{
    struct Alignment
    {
        std::string id;
        
        // If this field is false, no assumption can be made to other fields
        bool mapped;
        
        // Whether this is a spliced read
        bool spliced;
        
        // Location of the alignment relative to the chromosome
        Locus l;
        
        // Only valid if the alignment is spliced
        BasePair skipped;
        
        std::string seq;
    };    
}

#endif
