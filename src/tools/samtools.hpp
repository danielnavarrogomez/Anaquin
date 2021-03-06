#ifndef HTSLIB_HPP
#define HTSLIB_HPP

#include <map>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <htslib/sam.h>
#include "parsers/parser_bam.hpp"

namespace Anaquin
{
    typedef std::string CigarStr;
    
    static std::map<int, char> bam2char =
    {
        { BAM_CMATCH,     'M'  },
        { BAM_CINS,       'I'  },
        { BAM_CDEL,       'D'  },
        { BAM_CREF_SKIP,  'N'  },
        { BAM_CSOFT_CLIP, 'S', },
        { BAM_CHARD_CLIP, 'H', },
        { BAM_CPAD,       'P', },
        { BAM_CEQUAL,     '=', },
        { BAM_CDIFF,      'X', },
    };

    inline void bam2print(const ParserBAM::Data &x)
    {
        const auto format = "%1%\t%2%\t%3%\t%4%\t%5%\t%6%\t%7%\t%8%\t%9%\t%10%\t%11%";
        const auto str = (boost::format(format) % x.name
                                                % x.flag
                                                % x.cID
                                                % x.l.start
                                                % x.mapq
                                                % x.cigar
                                                % x.rnext
                                                % x.pnext
                                                % x.tlen
                                                % x.seq
                                                % x.qual).str();
        std::cout << str << std::endl;
    }
    
    inline std::string bam2rnext(bam_hdr_t *h, bam1_t *b)
    {
        const auto cID = std::string(h->target_name[b->core.tid]);
        
        if (b->core.mtid == -1)
        {
            return "";
        }

        const auto rID = std::string(h->target_name[b->core.mtid]);
        
        if (rID == cID)
        {
            return "=";
        }

        assert(!rID.empty());
        return rID;
    }
    
    inline std::string bam2qual(bam1_t *x)
    {
        std::stringstream buf;
        
        for (auto i = 0; i < x->core.l_qseq; ++i)
        {
            buf << (char) (bam_get_qual(x)[i] + 33);
        }
        
        return buf.str();
    }
    
    inline std::string bam2seq(bam1_t *x)
    {
        std::stringstream buf;

        for (auto i = 0; i < x->core.l_qseq; ++i)
        {
            buf << seq_nt16_str[bam_seqi(bam_get_seq(x),i)];
        }

        return buf.str();
    }
    
    inline CigarStr bam2cigar(bam1_t *x)
    {
        std::stringstream buf;
        const auto t = bam_get_cigar(x);

        for (auto i = 0; i < x->core.n_cigar; i++)
        {
            buf << std::to_string(bam_cigar_oplen(t[i]));
            buf << bam2char.at(bam_cigar_op(t[i]));
        }
        
        return buf.str();
    }

    inline std::vector<int> bam2delta(bam1_t *x)
    {
        std::vector<int> d;
        const auto t = bam_get_cigar(x);
        
        for (int i = x->core.n_cigar - 1; i >= 0; i--)
        {
            const auto val = bam_cigar_oplen(t[i]);
            
            switch (bam_cigar_op(t[i]))
            {
                case BAM_CPAD:
                case BAM_CDIFF:
                case BAM_CEQUAL:
                case BAM_CMATCH:
                case BAM_CSOFT_CLIP:  { d.push_back(0);    break; }
                case BAM_CINS:        { d.push_back(-val); break; }
                case BAM_CDEL:        { d.push_back(val);  break; }
                case BAM_CREF_SKIP:   { d.push_back(-val); break; }
                case BAM_CHARD_CLIP:  { d.push_back(val);  break; }
            }
        }
        
        return d;
    }
    
#ifdef REVERSE_ALIGNMENT
    inline Base reversePos(const Locus &l, ParserBAM::Data &x, const ParserBAM::Info &i)
    {
        // Length of the chromosome
        const auto clen = i.length;

        // Length of the sequence
        const auto slen = x.seq.size();
        
        // Insertion, deletion etc
        const auto delta = sum(bam2delta(reinterpret_cast<bam1_t *>(i.data)));
        
        return clen - (l.start + slen + delta) + 2;
    }
#endif

    inline CigarStr bam2rcigar(bam1_t *x)
    {
        std::stringstream buf;        
        const auto t = bam_get_cigar(x);

        for (int i = x->core.n_cigar - 1; i >= 0; i--)
        {
            buf << std::to_string(bam_cigar_oplen(t[i]));
            buf << bam2char.at(bam_cigar_op(t[i]));
        }
        
        return buf.str();
    }

#ifdef REVERSE_ALIGNMENT
    
    // Reverse an alignment
    inline void reverse(ParserBAM::Data &x, const ParserBAM::Info &i)
    {
        /*
         * The following needs to be reversed:
         *
         *   1. POS
         *   2. CIGAR
         *   3. PNEXT
         *   4. SEQ
         *   5. QUAL
         */

        const auto b = reinterpret_cast<bam1_t *>(i.data);

        x.cigar = bam2rcigar(b);

        std::reverse(x.seq.begin(),  x.seq.end());
        std::reverse(x.qual.begin(), x.qual.end());
        
        // The left-most position in the forward strand
        const auto rstart = reversePos(x.l, x, i);
        
        // The right-most positiion in the forward strand
        const auto rend = rstart + x.l.length() - 1;

        const auto t = x.l;
        
        // New position on the forward strand
        x.l = Locus(rstart, rend);
        
        assert(t.length() == x.l.length());
    }
    
#endif
}

#endif
