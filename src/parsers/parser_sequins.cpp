#include "data/reader.hpp"
#include "data/standard.hpp"
#include "parsers/parser_sequins.hpp"
#include <boost/algorithm/string.hpp>

using namespace Anaquin;

SequinList ParserSequins::parse(const std::string &file)
{
    const auto &s = Standard::instance();    
    Reader f(file);
    
    SequinList l;
    std::string line;
    
    while (f.nextLine(line))
    {
        boost::trim(line);

        if (std::find_if(s.r_seqs_A.begin(), s.r_seqs_A.end(), [&](const std::pair<SequinID, Sequin> &p)
        {
            return p.first == line;
        }) != s.r_seqs_A.end())
        {
            throw std::runtime_error("Unknown sequin: " + line);
        }

        l.push_back(line);
    }
    
    return l;
}