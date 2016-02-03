#include <catch.hpp>
#include "data/reader.hpp"
#include "parsers/parser_varscan.hpp"

using namespace Anaquin;

TEST_CASE("ParserVarScan_Test")
{
    std::vector<ParserVarScan::Data> x;
    
    ParserVarScan::parse(Reader("tests/data/VARQuin/varscan.tab"), [&](const ParserVarScan::Data &d, const ParserProgress &)
    {
        x.push_back(d);
    });
    
    REQUIRE(x.size() == 1469);
}