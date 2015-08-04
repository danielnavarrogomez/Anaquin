#ifndef GI_F_ANALYZER_HPP
#define GI_F_ANALYZER_HPP

#include "stats/analyzer.hpp"
#include "parsers/parser_top_fusion.hpp"
#include "parsers/parser_star_fusion.hpp"

namespace Anaquin
{
    struct FAnalyzer
    {
        template <typename Options, typename T> static ClassifyResult
            classifyFusion(const T &f, Confusion &m, SequinID &id, Options &options)
        {
            const auto &s = Standard::instance();

            // Don't bother unless in-silico chromosome
            if (f.chr_1 != s.id || f.chr_2 != s.id)
            {
                return Ignore;
            }

            ClassifyResult r = Negative;

            if (classify(m, f, [&](const T &)
            {
                if (f.start_1 == 8168506)
                {
                    r = Negative;
                }

                const auto min = std::min(f.start_1, f.start_2);
                const auto max = std::max(f.start_1, f.start_2);

                const auto r = std::find_if(s.f_breaks.begin(), s.f_breaks.end(), [&](const FusionBreak &x)
                {
                    return min == x.l.start && max == x.l.end;
                });

                if (r != s.f_breaks.end())
                {
                    id = r->id;
                }

                return (r != s.f_breaks.end()) ? Positive : Negative;
            }))
            {
                return Positive;
            }

            return Negative;
        }
        
        template <typename Options, typename Stats> static Stats analyze(const std::string &file, const Options &options = Options())
        {
            Stats stats;
            const auto &s = Standard::instance();

            options.info("Parsing alignment file");

            /*
             * Positive classified. Updated the statistics.
             */

            auto positive = [&](const SequinID &id, Reads reads)
            {
                assert(!id.empty() && s.f_seqs_A.count(id));
                
                // Known abundance for the fusion
                const auto known = s.f_seqs_A.at(id).abund() / s.f_seqs_A.at(id).length;
                
                // Measured abundance for the fusion
                const auto measured = reads;

                stats.h.at(id)++;
                stats.z.push_back(id);
                stats.x.push_back(log2f(known));
                stats.y.push_back(log2f(measured));
            };
            
            SequinID id;

            if (options.soft == Software::Star)
            {
                ParserStarFusion::parse(Reader(file), [&](const ParserStarFusion::Fusion &f, const ParserProgress &)
                {
                    if (classifyFusion(f, stats.m, id, options) == ClassifyResult::Positive)
                    {
                        positive(id, f.reads);
                    }
                });
            }
            else
            {
                ParserTopFusion::parse(Reader(file), [&](const ParserTopFusion::Fusion &f, const ParserProgress &)
                {
                    if (classifyFusion(f, stats.m, id, options) == ClassifyResult::Positive)
                    {
                        positive(id, f.reads);
                    }
                });
                
                
//                ParserFusion::parse(Reader(file), [&](const ParserFusion::Fusion &f, const ParserProgress &p)
//                {
//                    options.logInfo((boost::format("%1%: %2% %3%") % p.i % f.chr_1 % f.chr_2).str());
//                    
//                    // Don't bother unless in silico chromosome
//                    if (f.chr_1 != s.id || f.chr_2 != s.id)
//                    {
//                        return;
//                    }
//                    
//                    ClassifyResult r = Negative;
//                    
//                    if (classify(stats.m, f, [&](const ParserFusion::Fusion &)
//                    {
//                        const auto start_1 = f.start_1;
//                        
//                        if (f.dir_1 == Backward && f.dir_2 == Forward)
//                        {
//                            for (const auto &i : s.seq2locus_2)
//                            {
//                                id = i.first;
//                                
//                                // Reference locus
//                                const auto &rl = i.second;
//                                
//                                // Starting position of the fusion on the reference chromosome
//                                const auto r_start_1 = rl.start;
//                                
//                                // Starting position of the fusion on the reference chromosome
//                                const auto r_start_2 = rl.end;
//                                
//                                if (r_start_1 == start_1 && r_start_2 == f.start_2)
//                                {
//                                    r = Positive;
//                                    break;
//                                }
//                            }
//                        }
//                        else
//                        {
//                            for (const auto &i : s.seq2locus_1)
//                            {
//                                id = i.first;
//                                
//                                // Reference locus
//                                const auto &rl = i.second;
//                                
//                                // Starting position of the fusion on the reference chromosome
//                                const auto r_start_2 = rl.end;
//                                
//                                if (i.second.start == start_1 && r_start_2 == f.start_2)
//                                {
//                                    r = Positive;
//                                    break;
//                                }
//                            }
//                        }
//                        
//                        assert(!id.empty());
//                        
//                        if (r == Positive && !s.f_seqs_A.count(id))
//                        {
//                            options.warn(id + " is defined in the reference but not in the mixture.");
//                            return Negative;
//                        }
//
//                        return r;
//                    }))
//                    {
//                        positive(id, f.reads);
//                    }
//                });
            }

            /*
             * Find out all the sequins undetected in the experiment
             */

            options.info("There are " + std::to_string(stats.h.size()) + " sequins in the reference");
            options.info("Checking for missing sequins");
            
            for (const auto &i : s.seqIDs)
            {
                const auto &seqID = i;

                // If the histogram has an entry of zero
                if (!stats.h.at(seqID))
                {
                    if (!s.f_seqs_A.count(seqID))
                    {
                        options.warn(seqID + " defined in the referene but not in the mixture and it is undetected.");
                        continue;
                    }

                    options.warn(seqID + " defined in the referene but not detected");
                    
                    const auto seq = s.f_seqs_A.at(seqID);

                    // Known abundance for the fusion
                    const auto known = seq.abund() / seq.length;

                    stats.y.push_back(0); // TODO: We shouldn't even need to add those missing sequins!
                    stats.z.push_back(seqID);
                    stats.x.push_back(log2f(known));

                    stats.miss.push_back(MissingSequin(seqID, known));
                }
            }

            // The references are simply the known fusion points
            stats.m.nr = s.f_breaks.size();

            options.info("Calculating limit of sensitivity");
            stats.s = Expression::analyze(stats.h, s.f_seqs_A);
            
            stats.covered = std::accumulate(stats.h.begin(), stats.h.end(), 0,
                    [&](int sum, const std::pair<SequinID, Counts> &p)
                    {
                        return sum + (p.second ? 1 : 0);
                    }) / stats.m.nr;

            assert(stats.covered >= 0 && stats.covered <= 1.0);
            
            return stats;
        }
    };
}

#endif