#ifndef REFERENCE_HPP
#define REFERENCE_HPP

#include <set>
#include "data/data.hpp"
#include "data/hist.hpp"
#include "data/reader.hpp"
#include "data/variant.hpp"
#include "data/minters.hpp"
#include "data/intervals.hpp"

namespace Anaquin
{
    struct DefaultStats
    {
        // Empty Implementation
    };

    /*
     * Generic template for a sequin. Specalized definitions expected to derive from this class.
     */
    
    struct SequinData : public Matched
    {
        inline bool operator<(const SequinID &x)  const { return this->id < x;  }
        inline bool operator==(const SequinID &x) const { return this->id == x; }

        // Input concentration
        inline Concent concent(Mixture m = Mix_1, bool norm = false) const
        {
            return mixes.at(m) / (norm ? l.length() : 1);
        }

        inline SequinID name() const override
        {
            return id;
        }
        
        SequinID id;

        Locus l;

        // Expected concentration (not available if no mixture provided)
        std::map<Mixture, Concent> mixes;
    };

    /*
     * Different rules how two positions can be compared
     */

    enum MatchRule
    {
        Exact,
        Overlap,
        Contains,
    };
    
    template <typename Data = SequinData, typename Stats = DefaultStats> class Reference
    {
        public:

            // Add a sequin defined in a mixture file
            inline void add(const SequinID &id, Base length, Concent c, Mixture m)
            {
                _mixes[m].insert(MixtureData(id, length, c));
                _rawMIDs.insert(id);
            }

            inline Counts countSeqs() const { return _data.size(); }
        
            // Return all validated sequins
            inline const std::map<SequinID, Data> &data() const { return _data; }

            inline const Data *match(const SequinID &id) const
            {
                return _data.count(id) ? &_data.at(id) : nullptr;
            }

            inline const Data *match(const Locus &l, MatchRule m) const
            {
                for (const auto &i : _data)
                {
                    if ((m == Overlap && i.second.l.overlap(l)) || (m == Contains && i.second.l.contains(l)))
                    {
                        return &i.second;
                    }
                }

                return nullptr;
            }
        
            /*
             * Histogram for distribution
             */

            template <typename T> Hist hist(const T &data) const
            {
                Hist hist;
            
                for (const auto &i : data)
                {
                    hist[i.first] = 0;
                }

                return hist;
            }

            inline SequinHist hist() const { return hist(_data); }

            // Calculate the total length of all sequins in the reference
            inline Base size() const
            {
                Base n = 0;
                
                for (const auto &i : _data)
                {
                    n += i.second.l.length();
                }

                assert(n);
                return n;
            }

            inline void finalize()
            {
                validate();
                
                for (auto &i : _data)
                {
                    if (!i.second.l.length())
                    {
                        throw std::runtime_error("Validation failed. Zero length in data.");
                    }
                }
            }

        protected:

            virtual void validate() = 0;

            struct MixtureData
            {
                MixtureData(const SequinID &id, Base length, Concent abund)
                        : id(id), length(length), abund(abund) {}

                inline bool operator<(const SequinID &id)  const { return this->id < id;  }
                inline bool operator==(const SequinID &id) const { return this->id == id; }
                
                inline bool operator<(const MixtureData &x)  const { return id < x.id;  }
                inline bool operator==(const MixtureData &x) const { return id == x.id; }

                SequinID id;

                // Length of the sequin
                Base length;

                // Amount of spiked-in abundance
                Concent abund;
            };

            /*
             * Provide a common framework for validation. Typically, the sequins can be validated
             * by two set of IDs, for example, mixtute and annotation. This function can also be
             * validated a single set of sequins, simply call merge(x, x).
             */
        
            template <typename T> void merge(const std::set<T> &t1, const std::set<T> &t2)
            {
                std::set<SequinID> x, y;
                
                for (const auto &i : t1) { x.insert(static_cast<SequinID>(i)); }
                for (const auto &i : t2) { y.insert(static_cast<SequinID>(i)); }

                assert(!x.empty() && !y.empty());

                std::vector<SequinID> diffs, inters;
            
                /*
                 * Check for any sequin defined in x but not in y
                 */
            
                std::set_difference(x.begin(),
                                    x.end(),
                                    y.begin(),
                                    y.end(),
                                    std::back_inserter(diffs));

                /*
                 * Check for any sequin defined in both sets
                 */
            
                std::set_intersection(x.begin(),
                                      x.end(),
                                      y.begin(),
                                      y.end(),
                                      std::back_inserter(inters));

                /*
                 * Construct a set of validated sequins. A valid sequin is one in which it's
                 * defined in both mixture and annoation.
                 */
            
                std::for_each(inters.begin(), inters.end(), [&](const SequinID &id)
                {
                    auto d = Data();
                              
                    d.id  = id;
                    
                    // Add a new entry for the validated sequin
                    _data[id] = d;

                    assert(!d.id.empty());
                });

                /*
                 * Now, we have a list of validated sequins. Use those sequins to merge information.
                 */
            
                for (const auto i : _mixes)
                {
                    // Eg: MixA, MixB etc
                    const auto mix = i.first;
                
                    // For each of the mixture defined
                    for (const auto j : i.second)
                    {
                        // Only if it's a validated sequin
                        if (_data.count(j.id))
                        {
                            _data.at(j.id).mixes[mix] = j.abund;
                        }
                    }
                }
            
                assert(!_data.empty());
            }
        
            template <typename T> void merge(const std::set<T> &x)
            {
                return merge(x, x);
            }

            inline const MixtureData * findMix(Mixture mix, const SequinID &id) const
            {
                for (auto &i : _mixes.at(mix))
                {
                    if (i.id == id)
                    {
                        return &i;
                    }
                }
                
                return nullptr;
            }

            // Validated sequins
            std::map<SequinID, Data> _data;

            // Set of IDs defined in the mixture
            std::set<SequinID> _rawMIDs;

            // Data for mixture (if defined)
            std::map<Mixture, std::set<MixtureData>> _mixes;
    };

    /*
     * -------------------- Ladder Analysis --------------------
     */
    
    class LadderRef : public Reference<SequinData, DefaultStats>
    {
        public:
        
            typedef std::string JoinID;
            typedef std::string UnjoinID;
        
            typedef std::set<JoinID>   JoinIDs;
            typedef std::set<UnjoinID> UnjoinIDs;

            typedef std::map<JoinID, Counts> JoinHist;

            LadderRef();

            // Return a list of sequin IDs at the joined level
            JoinIDs joinIDs() const;

            // Construct a histogram at the joined level
            JoinHist joinHist() const;

            // Calculate the limit of detection at the joined level
            Limit limitJoin(const JoinHist &) const;

            // Return abundance for all segments of a particular conjoined
            void concent(const JoinID &, Concent &, Concent &, Concent &, Concent &, Mixture) const;

        protected:

            void validate() override;

        private:
    
            struct LadderRefImpl;
        
            std::shared_ptr<LadderRefImpl> _impl;
    };
    
    /*
     * -------------------- Metagenomics Analysis --------------------
     */
    
    class MetaRef : public Reference<SequinData, DefaultStats>
    {
        public:
            MetaRef();

            void addStand(const SequinID &, const Locus &);

            // Whether the locus is contained in one of the genomes
            const SequinData * contains(const GenomeID &, const Locus &) const;

        protected:

            void validate() override;
        
        private:
        
            struct MetaRefImpl;

            std::shared_ptr<MetaRefImpl> _impl;
    };
    
    /*
     * -------------------- Fusion Analysis --------------------
     */
    
    class FusionRef : public Reference<SequinData, DefaultStats>
    {
        public:

            struct KnownFusion
            {
                inline bool operator<(const KnownFusion &x)  const { return id < x.id;  }
                inline bool operator==(const KnownFusion &x) const { return id == x.id; }

                operator const SequinID &() const { return id; }
                
                // Where this fusion belongs
                SequinID id;
            
                // The position of the breakpoint
                Base l1, l2;
            
                // Orientation for each of the segment
                Strand s1, s2;
            };

            /*
             * Represents the concentration between normal and fusion gene
             */

            struct NormalFusion
            {
                // Concentration for the normal splicing
                Concent normal;
                
                // Concentration for the fusion chimeria
                Concent fusion;

                inline Fold fold() const { return normal / fusion; }
            };

            FusionRef();

            /*
             * Modifier operations
             */
        
            void addFusion(const KnownFusion &);
        
            // Add an intron junction for the normal genes
            void addJunct(const SequinID &, const Locus &);

            // Add fusion or normal genes comprised of the standards
            void addStand(const SequinID &, const Locus &);

            SequinHist normalHist() const;
            SequinHist fusionHist() const;

            Counts countFusion() const;
            Counts countJuncts() const;

            const KnownFusion *findFusion(const SequinID &) const;
            const KnownFusion *findFusion(Base x, Base y, Strand o1, Strand o2, double fuzzy) const;

            // Find a reference junction for the normal genes
            const SequinData *findJunct(const Locus &) const;

            const NormalFusion *findNormFus(const SequinID &) const;

        protected:
        
            void validate() override;

        private:
        
            struct FusionRefImpl;

            std::shared_ptr<FusionRefImpl> _impl;
    };

    /*
     * -------------------- Variant Analysis --------------------
     */
    
    struct Variant;
    
    class VarRef : public Reference<SequinData, DefaultStats>
    {
        public:

            VarRef();

            void readBRef(const Reader &);
            void readVRef(const Reader &);

            Anaquin::Base countBaseSyn() const;
            Anaquin::Base countBaseGen() const;

            Counts countGeneSyn() const;
            Counts countGeneGen() const;

            // Returns number of known variants
            Counts countVar() const;

            // Count SNPs for a chromosome
            Counts countSNP(const ChrID &) const;
        
            // Counts indels for the synthetic chromosomes
            Counts countSNPSyn() const;
        
            // Counts indels for the genome
            Counts countSNPGen() const;
        
            // Counts indels for a chromosome
            Counts countInd(const ChrID &) const;
        
            // Counts indels for the synthetic chromosomes
            Counts countIndSyn() const;
        
            // Counts indels for the genome
            Counts countIndGen() const;

            inline Counts countVarSyn() const { return countSNPSyn() + countIndSyn(); }
            inline Counts countVarGen() const { return countSNPGen() + countIndGen(); }

            // Histogram for all reference chromosomes
            std::map<ChrID, Hist> hist() const;

            C2Intervals  dInters()    const;
            ID2Intervals dIntersSyn() const;
            C2Intervals  dIntersGen() const;

            MC2Intervals mInters()  const;
            MC2Intervals msInters() const;
            MC2Intervals mgInters() const;
        
            MergedIntervals<> mInters(const ChrID &) const;
        
            std::map<ChrID, std::map<long, Counts>> vHist() const;

            const Variant *findVar(const ChrID &, long key) const;
            const Variant *findVar(const ChrID &, const Locus &) const;

            Concent findRCon(const SequinID &) const;
            Concent findVCon(const SequinID &) const;
        
            // Returns the expected allele fold-change
            Fold findAFold(const SequinID &) const;

            // Returns the expected allele frequency
            Proportion findAFreq(const SequinID &) const;

            // Is this germline? Homozygous?
            bool isGermline() const;
        
        protected:

            void validate() override;

        private:

            struct VarRefImpl;
            struct VariantPair;

            std::shared_ptr<VarRefImpl> _impl;
    };
    
    class Reader;
    
    /*
     * -------------------- Transcriptome Analysis --------------------
     */
    
    struct GeneData;
    struct TransData;
    
    class TransRef : public Reference<SequinData, DefaultStats>
    {
        public:

            TransRef();

            void readRef(const Reader &);

            std::map<ChrID, Hist> histGene() const;
            std::map<ChrID, Hist> histIsof() const;

            MC2Intervals meInters() const;
            MC2Intervals ueInters() const;
            MC2Intervals uiInters() const;

            Base countLenSyn() const;
            Base countLenGen() const;

            MC2Intervals mergedExons() const;
            MergedIntervals<> mergedExons(const ChrID &cID) const;

            // Number of sequin genes from mixture
            Counts countGeneSeqs() const;

            Counts countUExon(const ChrID &) const;
            Counts countUExonSyn() const;
            Counts countUExonGen() const;

            Counts countUIntr(const ChrID &) const;
            Counts countUIntrSyn() const;
            Counts countUIntrGen() const;

            Counts countGeneSyn() const;
            Counts countGeneGen() const;

            Counts countTransSyn() const;
            Counts countTransGen() const;
        
            // Concentration at the gene level
            Concent concent(const GeneID &, Mixture m = Mix_1) const;

            GeneID s2g(const SequinID &) const;
        
            const GeneData  *findGene (const ChrID &, const GeneID &) const;
            const TransData *findTrans(const ChrID &, const GeneID &) const;

        protected:
        
            void validate() override;

            void merge(const std::set<SequinID> &, const std::set<SequinID> &);
        
        private:

            struct TransRefImpl;

            std::shared_ptr<TransRefImpl> _impl;        
    };
}

#endif