#ifndef V_ALLELE_HPP
#define V_ALLELE_HPP

#include "stats/analyzer.hpp"
#include "VarQuin/VarQuin.hpp"

namespace Anaquin
{
    struct VAllele
    {
        struct Options : public AnalyzerOptions
        {
            Caller caller;
        };

        struct Stats : public MappingStats, public SequinStats, public VariantStats
        {
            // Statistics for all variants
            LinearStats all;
            
            // Statistics for SNPs
            LinearStats snp;
            
            // Statistics for indels
            LinearStats ind;

            // Mapping for reference read counts
            std::map<SequinID, Counts> readR;

            // Mapping for variant read counts
            std::map<SequinID, Counts> readV;
        };

        static Stats analyze(const FileName &, const Options &o = Options());
        static void report(const FileName &, const Options &o = Options());
    };
}

#endif