#include "VARQuin/v_allele.hpp"

using namespace Anaquin;

VAllele::Stats VAllele::analyze(const FileName &file, const Options &o)
{
    VAllele::Stats stats;
    
    stats.data[ChrT];

    const auto &r = Standard::instance().r_var;

    parseVariant(file, o.caller, [&](const VariantMatch &m)
    {
        const auto &v = *m.query;

        if (v.chrID == ChrT && m.match)
        {
            // Expected allele frequence
            const auto known = r.alleleFreq(m.match->id);

            // Measured coverage is the number of base calls aligned and used in variant calling
            const auto measured = static_cast<double>(v.dp_a) / (v.dp_r + v.dp_a);
            
            /*
             * Plotting the relative allele frequency that is established by differences
             * in the concentration of reference and variant DNA standards.
             */
            
            // Eg: D_1_12_R_373892_G/A
            const auto id = (boost::format("%1%_%2%_%3%_%4%:") % m.match->id
                                                               % m.match->ref
                                                               % m.match->l.start
                                                               % m.match->alt).str();
            stats.data.at(ChrT).add(id, known, measured);
        }
    });
    
    return stats;
}

void VAllele::report(const FileName &file, const Options &o)
{
    const auto &stats = analyze(file, o);
    
//    o.writer->open("VarAllele_false.stats");
//
//    classify(file, stats, [&](const VCFVariant &v, const Variation *match)
//    {
//        // The known coverage for allele frequnece
//        const auto known = r.alleleFreq(Mix_1, match->bID);
//
//        // The measured coverage is the number of base calls aligned and used in variant calling
//        const auto measured = static_cast<double>(v.dp_a) / (v.dp_r + v.dp_a);
//
//        /*
//         * Plotting the relative allele frequency that is established by differences
//         * in the concentration of reference and variant DNA standards.
//         */
//
//        // Eg: D_1_12_R_373892_G/A
//        const auto id = (boost::format("%1%_%2%_%3%_%4%:") % match->id
//                                                           % match->ref
//                                                           % match->l.start
//                                                           % match->alt).str();
//
//        stats.chrT->h.at(match->id)++;
//
//        // TODO: How to handle a case where variant is reported but with zero counts?
//        if (v.dp_a == 0)
//        {
//            return;
//        }
//
//        stats.chrT->add(id, known, measured);
//    });
// 
//    stats.chrT->ss = r.limit(stats.chrT->h);
//    stats.chrT->sn = static_cast<double>(stats.chrT->detected) / r.countVars();
//    
    /*
     * Generating summary statistics
     */

    o.info("Generating summary statistics");
    //AnalyzeReporter::linear("VarAllele_summary.stats", file, stats, "variants", o.writer);

    /*
     * Generating scatter plot for SNPs
     */
    
    /*
     * Generating scatter plot for indels
     */
    
    o.writer->open("VarAllele_Indel.R");
    o.writer->write(RWriter::scatter(stats.data.at(ChrT), "VarAllele", "Expected allele frequency", "Measured allele frequency", "Expected allele frequency (log2)", "Measured allele frequency (log2)"));
    o.writer->close();
}