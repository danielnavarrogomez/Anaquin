#ifndef GI_D_VARIANT_HPP
#define GI_D_VARIANT_HPP

#include "analyzer.hpp"

namespace Spike
{
    struct VariantStats
    {
        // Overall performance
        Performance p;

        // The proportion of genetic variation with alignment coverage
        double covered;
    };

    struct DVariant
    {
        struct Options : public SingleMixtureOptions
        {
            // Empty Implementation
        };

        static VariantStats analyze(const std::string &file, const Options &options = Options());
    };
}

#endif