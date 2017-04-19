#include <ss/stats.hpp>
#include "tools/random.hpp"
#include "VarQuin/v_sample.hpp"
#include "writers/writer_sam.hpp"
#include "parsers/parser_bambed.hpp"

// Defined in main.cpp
extern Anaquin::FileName BedRef();

using namespace Anaquin;

typedef std::map<ChrID, std::map<Locus, Proportion>> NormFactors;

static ParserBAMBED::Stats sample(const FileName &file,
                                  const NormFactors &norms,
                                  VSample::Stats &stats,
                                  const C2Intervals &sampled,
                                  const VSample::Options &o)
{
    typedef std::map<ChrID, std::map<Locus, std::shared_ptr<RandomSelection>>> Selection;
    
    /*
     * Initalize independnet random generators for every sampling region
     */
    
    Selection select;
    
    for (const auto &i : norms)
    {
        for (const auto &j : i.second)
        {
            A_ASSERT(j.second >= 0 && j.second <= 1.0 && !isnan(j.second));
            
            // Create independent random generator for each region
            select[i.first][j.first] = std::shared_ptr<RandomSelection>(new RandomSelection(1.0 - j.second));
        }
    }

    A_ASSERT(select.size() == norms.size());

    o.info("Sampling: " + file);
    
    WriterSAM writer;
    writer.openTerm();

    return ParserBAMBED::parse(file, sampled, [&](const ParserSAM::Data &x,
                                                  const ParserSAM::Info &info,
                                                  const Interval *inter)
    {
        if (info.p.i && !(info.p.i % 1000000))
        {
            o.logInfo(std::to_string(info.p.i));
        }
        
        /*
         * We should sample:
         *
         *    - Anything that is not mapped
         *    - Anything outside the sampling regions
         *    - Inside the region with probability
         */

        auto shouldSampled = !x.mapped;
        
        if (!shouldSampled)
        {
            Interval *inter;
            
            /*
             * Should that be contains or overlap? We prefer overlaps because any read that is overlapped
             * into the regions still give valuable information and sequencing depth.
             */

            if (sampled.count(x.cID) && (inter = sampled.at(x.cID).overlap(x.l)))
            {
                // Transform for the trimming region
                auto l = Locus(inter->l().start + o.edge, inter->l().end - o.edge);
                
                if (select.at(x.cID).at(l)->select(x.name))
                {
                    shouldSampled = true;
                }
            }
            else
            {
                // Never throw away reads outside the regions
                shouldSampled = true;
            }
        }
        
        if (shouldSampled)
        {
            stats.totAfter.nSeqs++;
            
            // Write SAM read to console
            writer.write(x);
            
            return ParserBAMBED::Response::OK;
        }

        return ParserBAMBED::Response::SKIP_EVERYTHING;
    });
}

template <typename Stats> Coverage stats2cov(const VSample::Method meth, const Stats &stats)
{
    switch (meth)
    {
        case VSample::Method::Mean:   { return stats.mean; }
        case VSample::Method::Median: { return stats.p50;  }

        /*
         * Prop and Reads specifies a fixed proportion to subsample. It's not actually a measure to report coverage.
         */

        case VSample::Method::Prop:
        case VSample::Method::Reads: { return stats.mean; }
    }
}

VSample::Stats VSample::analyze(const FileName &gen, const FileName &seq, const Options &o)
{
    o.analyze(gen);
    o.analyze(seq);

    o.logInfo("Edge: " + std::to_string(o.edge));
    
    const auto &r = Standard::instance().r_var;
    
    VSample::Stats stats;
    
    // Regions to subsample before trimming
    const auto tRegs = r.regions(true);

    // Regions without trimming
    const auto regs = r.regions(false);
    
    A_ASSERT(!tRegs.empty());
    A_ASSERT(tRegs.size() == regs.size());
    
    // Checking endogenous alignments before sampling
    const auto eStats = ParserBAMBED::parse(gen, tRegs, [&](const ParserSAM::Data &x, const ParserSAM::Info &info, const Interval *)
    {
        if (info.p.i && !(info.p.i % 1000000))
        {
            o.logWait(std::to_string(info.p.i));
        }
        
        if (x.mapped)
        {
            stats.totBefore.nEndo++;
        }
        
        return ParserBAMBED::Response::OK;
    });

    // Reads aligned to the sequins should be trimmed
    std::set<ReadName> trimmed;

    // Checking sequin alignments before sampling
    const auto sStats = ParserBAMBED::parse(seq, tRegs, [&](ParserSAM::Data &x, const ParserSAM::Info &info, const Interval *inter)
    {
        if (info.p.i && !(info.p.i % 1000000))
        {
            o.logWait(std::to_string(info.p.i));
        }
        
        if (x.mapped)
        {
            stats.totBefore.nSeqs++;
        }

        return ParserBAMBED::Response::OK;
    });
    
    // Normalization for each region
    NormFactors norms;

    // Required for summary statistics
    std::vector<double> allNorms;

    std::vector<double> allafterSeqsC;
    std::vector<double> allbeforeEndoC;
    std::vector<double> allbeforeSeqsC;
    
    // For each chromosome...
    for (auto &i : tRegs)
    {
        const auto cID = i.first;

        // For each region...
        for (auto &j : i.second.data())
        {
            const auto &l = j.second.l();
            
            // Endogenous statistics for the region
            const auto gs = eStats.inters.at(cID).find(l.key())->stats();
            
            // Sequins statistics for the region
            const auto ss = sStats.inters.at(cID).find(l.key())->stats();
            
            o.info("Calculating coverage for " + j.first);
            
            /*
             * Now we have the data, we'll need to compare coverage and determine the fraction that
             * the synthetic alignments needs to be sampled.
             */
            
            const auto endoC = stats2cov(o.meth, gs);
            const auto seqsC = stats2cov(o.meth, ss);

            Proportion norm;

            switch (o.meth)
            {
                case VSample::Method::Mean:
                case VSample::Method::Median: { norm = seqsC ? std::min(endoC / seqsC, 1.0) : NAN; break; }
                case VSample::Method::Prop:
                {
                    A_ASSERT(!isnan(o.p));
                    norm = o.p;
                    break;
                }

                case VSample::Method::Reads:
                {
                    A_ASSERT(!isnan(o.reads));

                    const auto gAligns = gs.aligns;
                    const auto sAligns = ss.aligns;

                    /*
                     * Nothing to subsample if no sequin alignments. Subsample everything if the
                     * genomic region has higher coverage.
                     */

                    norm = sAligns == 0 ? 0 : gAligns >= sAligns ? 1 : ((Proportion) gAligns) / sAligns;

                    break;
                }
            }

            allbeforeEndoC.push_back(endoC);
            allbeforeSeqsC.push_back(seqsC);
            
            if (isnan(norm))
            {
                o.logWarn((boost::format("Normalization is NAN for %1%:%2%-%3%") % i.first
                                                                                 % l.start
                                                                                 % norm).str());
                
                // We can't just use NAN...
                norm = 0.0;
            }
            else if (norm == 1.0)
            {
                o.logWarn((boost::format("Normalization is 1 for %1%:%2%-%3% (%4%)") % i.first
                                                                                     % l.start
                                                                                     % l.end
                                                                                     % j.first).str());
            }
            else
            {
                o.logInfo((boost::format("Normalization is %1% for %2%:%3%-%4% (%5%)") % norm
                                                                                       % i.first
                                                                                       % l.start
                                                                                       % l.end
                                                                                       % j.first).str());
            }
            
            if (!endoC) { stats.noGAlign++; }
            if (!seqsC) { stats.noSAlign++; }
            
            stats.c2v[cID][l].rID    = j.first;
            stats.c2v[cID][l].endo   = endoC;
            stats.c2v[cID][l].before = seqsC;
            stats.c2v[cID][l].norm   = norms[i.first][l] = norm;
            
            allNorms.push_back(norm);
        }
    }
    
    // We have the normalization factors so we can proceed with subsampling.
    const auto after = sample(seq, norms, stats, regs, o);
    
    /*
     * Assume our subsampling is working, let's check the coverage for every region.
     */
    
    // For each chromosome...
    for (auto &i : after.inters)
    {
        // For each region...
        for (auto &j : i.second.data())
        {
            stats.count++;
            
            // Coverage after subsampling
            const auto cov = stats2cov(o.meth, j.second.stats());

            auto l = j.second.l();
            
            // Transform for the trimming region
            l = Locus(l.start + o.edge, l.end - o.edge);

            stats.c2v[i.first].at(l).after = cov;
            
            // Required for generating summary statistics
            allafterSeqsC.push_back(cov);
        }
    }
    
    stats.beforeEndo = SS::mean(allbeforeEndoC);
    stats.beforeSeqs = SS::mean(allbeforeSeqsC);
    stats.afterEndo  = stats.beforeEndo;
    stats.afterSeqs  = SS::mean(allafterSeqsC);
    
    stats.normSD   = SS::getSD(allNorms);
    stats.normAver = SS::mean(allNorms);
    
    stats.totAfter.nEndo = stats.totBefore.nEndo;
    
    stats.sampAfter.nEndo  = eStats.nMap;
    stats.sampBefore.nEndo = eStats.nMap;
    
    // Remember, the synthetic reads have been mapped to the forward genome
    stats.sampBefore.nSeqs = sStats.nMap;

    // Remember, the synthetic reads have been mapped to the forward genome
    stats.sampAfter.nSeqs = after.nMap;

    return stats;
}

static void generateCSV(const FileName &file, const VSample::Stats &stats, const VSample::Options &o)
{
    o.generate(file);

    const auto format = boost::format("%1%\t%2%\t%3%\t%4%\t%5%\t%6%\t%7%\t%8%");
    
    o.generate(file);
    o.writer->open(file);
    o.writer->write((boost::format(format) % "Name"
                                           % "ChrID"
                                           % "Start"
                                           % "End"
                                           % "Genome"
                                           % "Before"
                                           % "After"
                                           % "Norm").str());

    // For each chromosome...
    for (const auto &i : stats.c2v)
    {
        // For each variant...
        for (const auto &j : i.second)
        {
            o.writer->write((boost::format(format) % j.second.rID
                                                   % i.first
                                                   % j.first.start
                                                   % j.first.end
                                                   % j.second.endo
                                                   % j.second.before
                                                   % j.second.after
                                                   % j.second.norm).str());            
        }
    }
    
    o.writer->close();
}

static void generateSummary(const FileName &file,
                            const FileName &gen,
                            const FileName &seq,
                            const VSample::Stats &stats,
                            const VSample::Options &o)
{
    o.generate(file);
    
    auto meth2Str = [&]()
    {
        switch (o.meth)
        {
            case VSample::Method::Mean:   { return "Mean";       }
            case VSample::Method::Median: { return "Median";     }
            case VSample::Method::Reads:  { return "Reads";      }
            case VSample::Method::Prop:   { return "Proportion"; }
        }
    };

    const auto summary = "-------VarSubsample Summary Statistics\n\n"
                         "       Reference annotation file: %1%\n"
                         "       Alignment file (genome):  %2%\n"
                         "       Alignment file (sequins): %3%\n\n"
                         "-------Reference regions\n\n"
                         "       Variant regions: %4% regions\n"
                         "       Method: %5%\n\n"
                         "-------Total alignments (before subsampling)\n\n"
                         "       Synthetic: %6%\n"
                         "       Genome:    %7%\n\n"
                         "-------Total alignments (after subsampling)\n\n"
                         "       Synthetic: %8%\n"
                         "       Genome:    %9%\n\n"
                         "-------Alignments within sampling regions (before subsampling)\n\n"
                         "       Synthetic: %10%\n"
                         "       Genome:    %11%\n\n"
                         "-------Alignments within sampling regions (after subsampling)\n\n"
                         "       Synthetic: %12%\n"
                         "       Genome:    %13%\n\n"
                         "       Normalization: %14% \u00B1 %15%\n\n"
                         "-------Before subsampling (within sampling regions)\n\n"
                         "       Synthetic coverage (average): %16%\n"
                         "       Genome coverage (average):    %17%\n\n"
                         "-------After subsampling (within sampling regions)\n\n"
                         "       Synthetic coverage (average): %18%\n"
                         "       Genome coverage (average):    %19%\n";
    
    o.generate(file);
    o.writer->open(file);
    o.writer->write((boost::format(summary) % BedRef()               // 1
                                            % gen                    // 2
                                            % seq                    // 3
                                            % stats.count            // 4
                                            % meth2Str()             // 5
                                            % stats.totBefore.nSeqs  // 6
                                            % stats.totBefore.nEndo  // 7
                                            % stats.totAfter.nSeqs   // 8
                                            % stats.totAfter.nEndo   // 9
                                            % stats.sampBefore.nSeqs // 10
                                            % stats.sampBefore.nEndo // 11
                                            % stats.sampAfter.nSeqs  // 12
                                            % stats.sampAfter.nEndo  // 13
                                            % stats.normAver         // 14
                                            % stats.normSD           // 15
                                            % stats.beforeSeqs       // 16
                                            % stats.beforeEndo       // 17
                                            % stats.afterSeqs        // 18
                                            % stats.afterEndo        // 19
                     ).str());
    o.writer->close();
}

void VSample::report(const FileName &gen, const FileName &seqs, const Options &o)
{
    const auto stats = analyze(gen, seqs, o);
    
    /*
     * Generating VarSubsample_summary.stats
     */
    
    generateSummary("VarSubsample_summary.stats", gen, seqs, stats, o);

    /*
     * Generating VarSubsample_sequins.csv
     */

    generateCSV("VarSubsample_sequins.csv", stats, o);
}
