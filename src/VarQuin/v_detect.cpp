#include "tools/tools.hpp"
#include "VarQuin/v_detect.hpp"

using namespace Anaquin;

typedef SeqVariant::Context Context;

extern Scripts PlotVLODR();
extern Scripts PlotVGROC();
extern Scripts PlotVCROC();
extern Scripts PlotAllele();

extern Path __output__;
extern std::string __full_command__;

inline std::string gt2str(Genotype x)
{
    switch (x)
    {
        case Genotype::Somatic:     { return "Somatic";     }
        case Genotype::Homozygous:  { return "Homozygous";  }
        case Genotype::Heterzygous: { return "Heterzygous"; }
    }
}

inline std::string var2str(Variation x)
{
    switch (x)
    {
        case Variation::SNP:       { return "SNP";       }
        case Variation::Deletion:  { return "Deletion";  }
        case Variation::Insertion: { return "Insertion"; }
    }
}

inline std::string ctx2Str(Context x)
{
    switch (x)
    {
        case Context::Cancer:        { return "Cancer";                    }
        case Context::LowGC:         { return "LowGC";                     }
        case Context::HighGC:        { return "HighGC";                    }
        case Context::Common:        { return "Common";                    }
        case Context::VeryLowGC:     { return "VeryLowGC";                 }
        case Context::VeryHighGC:    { return "VeryHighGC";                }
        case Context::LongHompo:     { return "LongHomopolymer";           }
        case Context::ShortHompo:    { return "ShortHomopolymer";          }
        case Context::ShortDinRep:   { return "ShortDinucleotideRepeat";   }
        case Context::LongDinRep:    { return "LongDinucleotideRepeat";    }
        case Context::ShortQuadRep:  { return "ShortQuadNucleotideRepeat"; }
        case Context::LongQuadRep:   { return "LongQuadNucleotideRepeat";  }
        case Context::ShortTrinRep:  { return "ShortTrinucleotideRepeat";  }
        case Context::LongTrinRep:   { return "LongTrinucleotideRepeat";   }
    }
}

static Scripts createVGROC(const FileName &file, const std::string &score, const std::string &refRat)
{
    return (boost::format(PlotVGROC()) % date()
                                       % __full_command__
                                       % __output__
                                       % file
                                       % score
                                       % refRat).str();
}

VDetect::EStats VDetect::analyzeE(const FileName &file, const Options &o)
{
    const auto regs = Standard::instance().r_var.regs1();
    
    VDetect::EStats stats;
    
    if (!file.empty())
    {
        readVFile(file, [&](const ParserVCF::Data &x, const ParserProgress &p)
        {
            // Only interested whether the variant falls in the reference regions
            if (regs.count(x.cID) && regs.at(x.cID).contains(x.l))
            {
                stats.found++;
            }
        });
    }
    else
    {
        stats.found = NAN;
    }

    return stats;
}

VDetect::SStats VDetect::analyzeS(const FileName &file, const Options &o)
{
    const auto &r = Standard::instance().r_var;

    VDetect::SStats stats;
    
    typedef SeqVariant::Context Context;
    
    auto muts = std::set<Variation>
    {
        Variation::SNP,
        Variation::Deletion,
        Variation::Insertion,
    };
    
    auto grps = std::set<Context>
    {
        Context::LowGC,
        Context::HighGC,
        Context::Cancer,
        Context::Common,
        Context::LongHompo,
        Context::VeryLowGC,
        Context::VeryHighGC,
        Context::ShortDinRep,
        Context::LongDinRep,
        Context::ShortHompo,
        Context::LongQuadRep,
        Context::LongTrinRep,
        Context::ShortQuadRep,
        Context::ShortTrinRep,
    };

    for (auto &i : grps) { stats.g2c[i]; }
    for (auto &i : muts) { stats.m2c[i]; }
    
    o.analyze(file);

    readVFile(file, [&](const ParserVCF::Data &x, const ParserProgress &p)
    {
        if (p.i && !(p.i % 100000))
        {
            o.wait(std::to_string(p.i));
        }
        
        const auto &r = Standard::instance().r_var;
        
        auto findMatch = [&](const ParserVCF::Data &query)
        {
            VariantMatch m;

            m.query = query;
            m.seqByPos = nullptr;
            
            // Can we match by position?
            if ((m.seqByPos = r.findVar(query.cID, query.l)))
            {
                // Match by reference allele?
                m.ref = m.seqByPos->ref == query.ref;
                
                // Match by alternative allele?
                m.alt = m.seqByPos->alt == query.alt;
            }
            
            if (m.seqByPos)
            {
                m.rReg = m.seqByPos->name;
                A_ASSERT(!m.rReg.empty());
            }
            else
            {
                MergedIntervals<> inters;
                
                try
                {
                    // We should to search where the FPs are
                    inters = r.mInters(x.cID);
                    
                    A_ASSERT(inters.size());
                    
                    const auto m2 = inters.contains(x.l);
                    
                    // Can we find the corresponding region for the FP?
                    if (m2)
                    {
                        m.rReg = m2->id();
                        A_ASSERT(!m.rReg.empty());
                    }
                }
                catch (...) {}
            }
            
            return m;
        };
        
        const auto m = findMatch(x);
        
        // Matched if the position and alleles agree
        const auto matched = m.seqByPos && m.ref && m.alt;
        
        if (matched)
        {
            const auto key = m.seqByPos->key();
            
            stats.tps.push_back(m);
            
            const auto exp = r.findAFreq(m.seqByPos->name);
            const auto obs = m.query.allF;
            
            A_ASSERT(!isnan(exp) && !isnan(obs));
            
            // Eg: 2821292107
            const auto id = toString(key);
            
            // Add for all variants
            stats.oa.add(id, exp, obs);
            
            // Add for mutation type
            stats.m2a[m.query.type()].add(id, exp, obs);
        }
        else
        {
            // FP because the variant is not found in the reference
            stats.fps.push_back(m);
        }
    });
    
    o.info("Aggregating statistics");

    /*
     * Determine the quantification limits
     */
    
    stats.oa.limit = stats.oa.limitQuant();

    for (auto &i : stats.m2a)
    {
        i.second.limit = i.second.limitQuant();
    }

    /*
     * Determine the classification performance
     */

    auto forTP = [&]()
    {
        for (auto &i : stats.tps)
        {
            // This shouldn't fail...
            const auto &sv = r.findSeqVar(i.seqByPos->key());
            
            // Overall performance
            stats.oc.tp()++;
            
            // Performance for each group
            stats.g2c[sv.ctx].tp()++;
            
            // Performance for each mutation
            stats.m2c[i.query.type()].tp()++;
        }
    };
    
    auto forFP = [&]()
    {
        for (auto &i : stats.fps)
        {
            // Overall performance
            stats.oc.fp()++;
            
            // Performance for each mutation
            stats.m2c[i.query.type()].fp()++;
        }
    };

    forTP();
    forFP();

    for (auto &mut : muts)
    {
        stats.m2c[mut].nr() = r.nType(mut);
        stats.m2c[mut].nq() = stats.m2c[mut].tp() + stats.m2c[mut].fp();
        stats.m2c[mut].fn() = stats.m2c[mut].nr() - stats.m2c[mut].tp();
        stats.oc.nr() += r.nType(mut);
    }

    stats.oc.fn() = stats.oc.nr() - stats.oc.tp();
    
    for (auto &grp : grps)
    {
        stats.g2c[grp].nr() = r.nContext(grp);
        stats.g2c[grp].nq() = stats.g2c[grp].tp() + stats.g2c[grp].fp();
        stats.g2c[grp].fn() = stats.g2c[grp].nr() - stats.g2c[grp].tp();
    }
    
    A_ASSERT(stats.oc.nr() >= stats.oc.fn());
    
    return stats;
}

static void writeQuins(const FileName &file,
                       const VDetect::SStats &ss,
                       const VDetect::Options &o)
{
    const auto &r = Standard::instance().r_var;
    const auto format = "%1%\t%2%\t%3%\t%4%\t%5%\t%6%\t%7%\t%8%\t%9%\t%10%\t%11%\t%12%\t%13%";

    o.generate(file);
    o.writer->open(file);
    o.writer->write((boost::format(format) % "Name"
                                           % "Chrom"
                                           % "Position"
                                           % "Label"
                                           % "ReadR"
                                           % "ReadV"
                                           % "Depth"
                                           % "ExpFreq"
                                           % "ObsFreq"
                                           % "Qual"
                                           % "Genotype"
                                           % "Context"
                                           % "Mutation").str());
    for (const auto &i : r.vars())
    {
        // Can we find this sequin?
        const auto isTP = ss.findTP(i.name);

        // This shouldn't fail...
        const auto &sv = r.findSeqVar(i.key());

        if (isTP)
        {
            // Called variant (if found)
            const auto &c = isTP->query;
            
            o.writer->write((boost::format(format) % i.name
                                                   % i.cID
                                                   % i.l.start
                                                   % "TP"
                                                   % c.readR
                                                   % c.readV
                                                   % c.depth
                                                   % r.findAFreq(i.name)
                                                   % c.allF
                                                   % toString(c.qual)
                                                   % gt2str(sv.gt)
                                                   % ctx2Str(sv.ctx)
                                                   % var2str(i.type())).str());
        }

        // Failed to detect the variant
        else
        {
            o.writer->write((boost::format(format) % i.name
                                                   % i.cID
                                                   % i.l.start
                                                   % "FN"
                                                   % "-"
                                                   % "-"
                                                   % "-"
                                                   % r.findAFreq(i.name)
                                                   % "-"
                                                   % "-"
                                                   % gt2str(sv.gt)
                                                   % ctx2Str(sv.ctx)
                                                   % var2str(i.type())).str());
        }
    }
    
    o.writer->close();
}

static void writeDetected(const FileName &file,
                          const VDetect::SStats &ss,
                          const VDetect::Options &o)
{
    const auto &r = Standard::instance().r_var;
    const auto format = "%1%\t%2%\t%3%\t%4%\t%5%\t%6%\t%7%\t%8%\t%9%\t%10%\t%11%\t%12%\t%13%";
    
    o.generate(file);
    o.writer->open(file);
    o.writer->write((boost::format(format) % "Name"
                                           % "Chrom"
                                           % "Position"
                                           % "Label"
                                           % "ReadR"
                                           % "ReadV"
                                           % "Depth"
                                           % "ExpFreq"
                                           % "ObsFreq"
                                           % "Pval"
                                           % "Qual"
                                           % "Context"
                                           % "Mutation").str());

    auto f = [&](const std::vector<VariantMatch> &x, const std::string &label)
    {
        for (const auto &i : x)
        {
            auto sID = (i.seqByPos && i.alt && i.ref ? i.seqByPos->name : "-");
            const auto ctx = sID != "-" ?  ctx2Str(r.findSeqVar(i.seqByPos->key()).ctx) : "-";

            o.writer->write((boost::format(format) % (i.rReg.empty() ? "-" : i.rReg)
                                                   % i.query.cID
                                                   % i.query.l.start
                                                   % label
                                                   % i.query.readR
                                                   % i.query.readV
                                                   % i.query.depth
                                                   % (sID != "-" ? std::to_string(r.findAFreq(sID)) : "-")
                                                   % i.query.allF
                                                   % ld2ss(i.query.p)
                                                   % toString(i.query.qual)
                                                   % ctx
                                                   % var2str(i.query.type())).str());
        }
    };
    
    f(ss.tps, "TP");
    f(ss.fps, "FP");

    o.writer->close();
}

static void writeSummary(const FileName &file,
                         const FileName &endo,
                         const FileName &seqs,
                         const VDetect::EStats &es,
                         const VDetect::SStats &ss,
                         const VDetect::Options &o)
{
    const auto &r = Standard::instance().r_var;

    extern FileName VCFRef();
    extern FileName BedRef();
    extern FileName MixRef();

    auto germline = [&]()
    {
        const auto summary = "-------VarDetect Output Results\n\n"
                             "-------VarDetect Output\n\n"
                             "       Reference variant annotations:      %1%\n"
                             "       Reference coordinate annotations:   %2%\n\n"
                             "       User identified variants (sample):   %3%\n"
                             "       User identified variants (sequin): %59%\n\n"
                             "       Number of variants in reference regions (sample):   %60%\n"
                             "       Number of variants in reference regions (sequin): %61%\n\n"
                             "-------Reference variants by mutation\n\n"
                             "       SNPs:   %4%\n"
                             "       Indels: %5%\n"
                             "       Total:  %6%\n\n"
                             "-------Reference variants by context\n\n"
                             "       Common:                       %31%\n"
                             "       Very Low GC:                  %32%\n"
                             "       Low GC:                       %33%\n"
                             "       High GC:                      %34%\n"
                             "       Very High GC:                 %35%\n"
                             "       Short Dinucleotide Repeat:    %36%\n"
                             "       Long Dinucleotide Repeat:     %37%\n"
                             "       Short Homopolymer:            %38%\n"
                             "       Long Homopolymer:             %39%\n"
                             "       Short Quad Nucleotide Repeat: %40%\n"
                             "       Long Quad Nucleotide Repeat:  %41%\n"
                             "       Short Trinucleotide Repeat:   %42%\n"
                             "       Long Trinucleotide Repeat:    %43%\n\n"
                             "-------Reference variants by genotype\n\n"
                             "       Homozygosity:   %44%\n"
                             "       Heterozygosity: %45%\n\n"
                             "-------Called variants by mutation\n\n"
                             "       %7% SNPs\n"
                             "       %8% indels\n"
                             "       %9% variants\n\n"
                             "-------Diagnostic performance by mutation\n\n"
                             "       True Positive:  %10% SNPs\n"
                             "       True Positive:  %11% indels\n"
                             "       True Positive:  %12% variants\n\n"
                             "       False Positive: %13% SNPs\n"
                             "       False Positive: %14% indels\n"
                             "       False Positive: %15% variants\n\n"
                             "       False Negative: %16% SNPs\n"
                             "       False Negative: %17% indels\n"
                             "       False Negative: %18% variants\n\n"
                             "       *Variants\n"
                             "       Sensitivity: %19$.4f\n"
                             "       Precision:   %20$.4f\n"
                             "       F1 Score:    %21$.4f\n"
                             "       FDR Rate:    %22$.4f\n\n"
                             "       *SNPs\n"
                             "       Sensitivity: %23$.4f\n"
                             "       Precision:   %24$.4f\n"
                             "       F1 Score:    %25$.4f\n"
                             "       FDR Rate:    %26$.4f\n\n"
                             "       *Indels\n"
                             "       Sensitivity: %27$.4f\n"
                             "       Precision:   %28$.4f\n"
                             "       F1 Score:    %29$.4f\n"
                             "       FDR Rate:    %30$.4f\n\n"
                             "-------Diagnostic performance by context\n\n"
                             "       *Low GC\n"
                             "       Sensitivity: %46$.4f\n\n"
                             "       *High GC\n"
                             "       Sensitivity: %47$.4f\n\n"
                             "       *Common\n"
                             "       Sensitivity: %48$.4f\n\n"
                             "       *Long Homopolymer\n"
                             "       Sensitivity: %49$.4f\n\n"
                             "       *Very Low GC\n"
                             "       Sensitivity: %50$.4f\n\n"
                             "       *Very High GC\n"
                             "       Sensitivity: %51$.4f\n\n"
                             "       *Short Dinucleotide Repeat\n"
                             "       Sensitivity: %52$.4f\n\n"
                             "       *Long Dinucleotide Repeat\n"
                             "       Sensitivity: %53$.4f\n\n"
                             "       *Short Homopolymer\n"
                             "       Sensitivity: %54$.4f\n\n"
                             "       *Long Quad Nucleotide Repeat\n"
                             "       Sensitivity: %55$.4f\n\n"
                             "       *Long Trinucleotide Repeat\n"
                             "       Sensitivity: %56$.4f\n\n"
                             "       *Short Quad Nucleotide Repeat\n"
                             "       Sensitivity: %57$.4f\n\n"
                             "       *Short Trinucleotide Repeat\n"
                             "       Sensitivity: %58$.4f";

        #define D(x) (isnan(x) ? "-" : std::to_string(x))
        
        const auto &m2c = ss.m2c;
        const auto &snp = m2c.at(Variation::SNP);
        const auto &del = m2c.at(Variation::Deletion);
        const auto &ins = m2c.at(Variation::Insertion);
        
        const auto c_nSNP = snp.nq();
        const auto c_nDel = del.nq();
        const auto c_nIns = ins.nq();
        
        const auto tp_SNP = snp.tp();
        const auto tp_Del = del.tp();
        const auto tp_Ins = ins.tp();

        const auto fp_SNP = snp.fp();
        const auto fp_Del = del.fp();
        const auto fp_Ins = ins.fp();
        
        const auto fn_SNP = snp.fn();
        const auto fn_Del = del.fn();
        const auto fn_Ins = ins.fn();
        
        auto ind = del;
        ind += ins;

        #define CSN(x) D(ss.g2c.at(x).sn())

        o.generate(file);
        o.writer->open("VarDetect_summary.stats");
        o.writer->write((boost::format(summary) % VCFRef()                      // 1
                                                % BedRef()                      // 2
                                                % seqs                          // 3
                                                % r.countSNP()                  // 4
                                                % r.countInd()                  // 5
                                                % (r.countSNP() + r.countInd()) // 6
                                                % c_nSNP                        // 7
                                                % (c_nDel + c_nIns)             // 8
                                                % (c_nSNP + c_nDel + c_nIns)    // 9
                                                % tp_SNP                        // 10
                                                % (tp_Del + tp_Ins)             // 11
                                                % (tp_SNP + tp_Del + tp_Ins)    // 12
                                                % fp_SNP                        // 13
                                                % (fp_Del + fp_Ins)             // 14
                                                % (fp_SNP + fp_Del + fp_Ins)    // 15
                                                % fn_SNP                        // 16
                                                % (fn_Del + fn_Ins)             // 17
                                                % (fn_SNP + fn_Del + fn_Ins)    // 18
                                                % D(ss.oc.sn())                 // 19
                                                % D(ss.oc.pc())                 // 20
                                                % D(ss.oc.F1())                 // 21
                                                % D(1-ss.oc.pc())               // 22
                                                % D(snp.sn())                   // 23
                                                % D(snp.pc())                   // 24
                                                % D(snp.F1())                   // 25
                                                % D(1-snp.pc())                 // 26
                                                % D(ind.sn())                   // 27
                                                % D(ind.pc())                   // 28
                                                % D(ind.F1())                   // 29
                                                % D(1-ind.pc())                 // 30
                                                % D(r.nContext(Context::Common))       // 31
                                                % D(r.nContext(Context::VeryLowGC))    // 32
                                                % D(r.nContext(Context::LowGC))        // 33
                                                % D(r.nContext(Context::HighGC))       // 34
                                                % D(r.nContext(Context::VeryHighGC))   // 35
                                                % D(r.nContext(Context::ShortDinRep))  // 36
                                                % D(r.nContext(Context::LongDinRep))   // 37
                                                % D(r.nContext(Context::ShortHompo))   // 38
                                                % D(r.nContext(Context::LongHompo))    // 39
                                                % D(r.nContext(Context::ShortQuadRep)) // 40
                                                % D(r.nContext(Context::LongQuadRep))  // 41
                                                % D(r.nContext(Context::ShortTrinRep)) // 42
                                                % D(r.nContext(Context::LongTrinRep))  // 43
                                                % D(r.nGeno(Genotype::Homozygous))     // 44
                                                % D(r.nGeno(Genotype::Heterzygous))    // 45
                                                % CSN(Context::LowGC)                  // 46
                                                % CSN(Context::HighGC)                 // 47
                                                % CSN(Context::Common)                 // 48
                                                % CSN(Context::LongHompo)              // 49
                                                % CSN(Context::VeryLowGC)              // 50
                                                % CSN(Context::VeryHighGC)             // 51
                                                % CSN(Context::ShortDinRep)            // 52
                                                % CSN(Context::LongDinRep)             // 53
                                                % CSN(Context::ShortHompo)             // 54
                                                % CSN(Context::LongQuadRep)            // 55
                                                % CSN(Context::LongTrinRep)            // 56
                                                % CSN(Context::ShortQuadRep)           // 57
                                                % CSN(Context::ShortTrinRep)           // 58
                                                % (endo.empty() ? "-" : endo)          // 59
                                                % (endo.empty() ? "-" : toString(es.found)) // 60
                                                % (c_nSNP + c_nDel + c_nIns)           // 61
                         ).str());
    };
    
    germline();
    o.writer->close();
}

void VDetect::report(const FileName &endo, const FileName &seqs, const Options &o)
{
    const auto es = analyzeE(endo, o);
    const auto ss = analyzeS(seqs, o);

    o.info("TP: " + std::to_string(ss.oc.tp()));
    o.info("FP: " + std::to_string(ss.oc.fp()));
    o.info("FN: " + std::to_string(ss.oc.fn()));

    o.info("Generating statistics");

    /*
     * Generating VarDetect_sequins.csv
     */
    
    writeQuins("VarDetect_sequins.csv", ss, o);

    /*
     * Generating VarDetect_summary.stats
     */
    
    writeSummary("VarDetect_summary.stats", endo, seqs, es, ss, o);
    
    /*
     * Generating VarDetect_detected.csv
     */
    
    writeDetected("VarDetect_detected.csv", ss, o);
    
    /*
     * Generating VarDetect_ROC.R
     */
    
    o.generate("VarDetect_ROC.R");
    o.writer->open("VarDetect_ROC.R");
    
    o.writer->write(createVGROC("VarDetect_detected.csv", "data$Depth", "'FP'"));
    o.writer->close();
}
