#include <catch.hpp>
#include "var/v_align.hpp"

using namespace Anaquin;

TEST_CASE("Missing_Mixture_File")
{

}

//-c ladder -p correct tests/data/ladder/aligned_A.sam

//TEST_CASE("DAlign_Simulations")
//{
//    const auto r = DAlign::analyze("tests/data/dna/aligned.sam");
//
//    REQUIRE(r.p.m.sp() == Approx(0.999984782));
//    REQUIRE(r.p.m.sn() == Approx(0.9998288238));
//    REQUIRE(r.p.m.nq == 262846);
//    REQUIRE(r.p.m.nr == 262887);
//    REQUIRE(r.p.s.id == "D_3_7_R");
//    REQUIRE(r.p.s.counts == 2);
//    REQUIRE(r.p.s.abund == Approx(1.005944252));
//}