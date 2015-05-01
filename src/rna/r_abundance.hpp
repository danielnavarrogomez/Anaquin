#ifndef GI_R_ABUNDANCE_HPP
#define GI_R_ABUNDANCE_HPP

#include "r_analyzer.hpp"

namespace Spike
{
    struct RAbundanceStats : public AnalyzerStats
    {
        Confusion m;
        Sensitivity s;

        // Correlation for the samples
        double r;
        
        // Adjusted R2 for the linear model
        double r2;
        
        // Coefficient for the linear model
        double slope;
    };

    struct RAbundance : public RAnalyzer
    {
        struct Options : public SingleMixtureOptions
        {
            RNALevel level;
        };

        static RAbundanceStats analyze(const std::string &file, const Options &options = Options());
    };
}

#endif