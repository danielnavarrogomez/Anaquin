#include "MetaQuin/m_express.hpp"

using namespace Anaquin;

// Defined in resources.cpp
extern Scripts PlotMExpress();

MExpress::Stats MExpress::analyze(const FileName &file, const MExpress::Options &o)
{
    const auto &r = Standard::instance().r_meta;
    
    MExpress::Stats stats;

    // Initialize the sequins
    stats.hist = r.hist();
    
    assert(!o.psl.empty());
    
    /*
     * Generate statistics for the alignment
     */
    
    // Generate statistics for BLAT
    auto t = MBlat::analyze(o.psl);

    /*
     * Generate statistics for the assembly
     */
 
    o.info("Analyzing: " + file);

/*
    switch (o.soft)
    {
        case Software::Velvet:  { stats.assembly = Velvet::analyze<MAssembly::Stats, DAsssembly::Contig>(file, &t);             break; }
        case Software::RayMeta: { stats.assembly = RayMeta::analyze<MAssembly::Stats, DAsssembly::Contig>(file, o.contigs, &t); break; }
    }
*/

    stats.blat = t;
 
    if (!stats.assembly.n)
    {
        throw std::runtime_error("No contig detected in the input file. Please check and try again.");
    }
    else if (stats.assembly.contigs.empty())
    {
        throw std::runtime_error("No contig aligned in the input file. Please check and try again.");
    }

    stats.n_chrT = stats.assembly.contigs.size();
    stats.n_geno = stats.assembly.n - stats.n_chrT;

    o.info("Analyzing the alignments");

    for (auto &meta : stats.blat.metas)
    {
        auto &align = meta.second;
        
        if (!r.match(align->seq->id))
        {
            o.warn((boost::format("%1% not defined in the mixture. Skipped.") % align->seq->id).str());
            continue;
        }
        
        /*
         * Calculate the limit of sensitivity. LOS is defined as the sequin with the lowest amount of
         * concentration while still detectable in the experiment.
         */

        if (stats.limit.id.empty() || align->seq->concent() < stats.limit.abund)
        {
            stats.limit.id     = align->seq->id;
            stats.limit.abund  = align->seq->concent();
            stats.limit.counts = align->contigs.size();
        }
        
        const auto p = MExpress::calculate(stats, stats.blat, stats.assembly, align->seq->id, *meta.second, o, o.coverage);

        if (p.x && p.y)
        {
            stats.add(align->seq->id, p.x, p.y);
        }
    }

    stats.limit = r.absolute(stats.hist);

    return stats;
}

static void generateContigs(const FileName &file, const MExpress::Stats &stats, const MExpress::Options &o)
{
    o.info("Generating " + file);
    o.writer->open(file);

    const auto format = "%1%\t%2%\t%3%\t%4%\t%5%";
    
    o.writer->write((boost::format(format) % "contigID"
                                           % "seqID"
                                           % "length"
                                           % "coverage"
                                           % "normalized").str());
    
    for (const auto &i : stats.blat.aligns)
    {
        if (stats.assembly.contigs.count(i.first))
        {
            const auto &contig = stats.assembly.contigs.at(i.first);
            
            o.writer->write((boost::format(format) % i.first
                                                   % i.second->id()
                                                   % contig.k_len
                                                   % contig.k_cov
                                                   % contig.normalized()).str());
        }
        else
        {
            o.writer->write((boost::format(format) % i.first
                                                   % i.second->id()
                                                   % "-"
                                                   % "-"
                                                   % "-").str());
        }
    }
    
    o.writer->close();
}

void MExpress::report(const FileName &file, const MExpress::Options &o)
{
    const auto stats = MExpress::analyze(file, o);

    /*
     * Generating summary statistics
     */
    
    o.info("Generating MetaExpress_summary.stats");
    o.writer->open("MetaExpress_summary.stats");
    o.writer->write(StatsWriter::inflectSummary(o.rChrT,
                                                o.rGeno,
                                                file,
                                                stats.hist,
                                                stats,
                                                stats,
                                                "sequins"));
    o.writer->close();
    
    /*
     * Generating detailed statistics for sequins
     */
    
    o.info("Generating MetaExpress_quins.stats");
    o.writer->open("MetaExpress_quins.stats");
    o.writer->write(StatsWriter::writeCSV(stats));
    o.writer->close();

    /*
     * Generating for expression plot
     */
    
    o.info("Generating MetaExpress_express.R");
    o.writer->open("MetaExpress_express.R");
    o.writer->write(RWriter::createScript("MetaExpress_quins.stats", PlotMExpress()));
    o.writer->close();
    
    /*
     * Generating detailed statistics for the contigs
     */

    generateContigs("MetaExpress_contigs.stats", stats, o);
}