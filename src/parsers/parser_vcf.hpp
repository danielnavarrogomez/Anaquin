#ifndef PARSER_VCF_HPP
#define PARSER_VCF_HPP

#include "stats/analyzer.hpp"
#include "parsers/parser.hpp"

namespace Anaquin
{
    typedef Variation VCFVariant;

    struct ParserVCF
    {
        inline static Mutation strToSNP(const std::string &r, const std::string &v)
        {
            if (r.size() == v.size())
            {
                return SNP;
            }
            else if (r.size() > v.size())
            {
                return Deletion;
            }
            else
            {
                return Insertion;
            }
        }

        typedef std::function<void (const VCFVariant &, const ParserProgress &)> Callback;
        static void parse(const Reader &, Callback);
    };
}

#endif