#ifndef GI_ALIGNMENT_HPP
#define GI_ALIGNMENT_HPP

#include "data/locus.hpp"

namespace Anaquin
{
    struct Alignment
    {
        operator const Locus &() const { return l; }

        std::string id;
        
        // Eg: B7_591:6:155:12:674
        std::string qName;
        
        unsigned i;
        
        // If this field is false, no assumption can be made to other fields
        bool mapped;

        // Whether this is a spliced read
        bool spliced;
        
        // Location of the alignment relative to the chromosome
        Locus l;
        
        // Only valid if the alignment is spliced
        Base skipped;
    };    
}

#endif
