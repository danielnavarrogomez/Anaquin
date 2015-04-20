#include <catch.hpp>
#include "rna/raligner.hpp"

using namespace Spike;

static std::string exts[] = { "sam", "bam" };

TEST_CASE("RAlign_RNA_Cufflinks")
{
    // The sample file was taken from Cufflink's source distribution. It's obviously independent.
    const auto r = RAligner::analyze("tests/data/cufflinks_test.sam");
    
    REQUIRE(0 == r.nr);
    REQUIRE(3271 == r.n);
    REQUIRE(3271 == r.nq);
    REQUIRE(0 == r.dilution());
}

TEST_CASE("RAlign_Simulations_1")
{
    for (auto ex : exts)
    {
        const auto r = RAligner::analyze("tests/data/rna_sims_1/align/accepted_hits." + ex);

        REQUIRE(r.n  == 98041);
        REQUIRE(r.nr == 98040);
        REQUIRE(r.nq == 1);

        REQUIRE(r.s_base.id == "R_5_2");
        REQUIRE(r.s_base.counts == 14);
        REQUIRE(r.s_base.abund == 9765.0);
        
        REQUIRE(r.s_exon.id == "R_5_2");
        REQUIRE(r.s_exon.counts == 14);
        REQUIRE(r.s_exon.abund == 9765.0);
    }
}

TEST_CASE("RAlign_Simulations_1_Exon")
{
    for (auto ex : exts)
    {
        RAligner::Options options;
        options.level = RAligner::Exon;
        const auto r = RAligner::analyze("tests/data/rna_sims_1/align/accepted_hits." + ex, options);
    
        REQUIRE(r.n  == 52360);
        REQUIRE(r.nr == 52359);
        REQUIRE(r.nq == 1);

        REQUIRE(r.m_base.fn == 1);
        REQUIRE(r.m_base.tp == 51919);
        REQUIRE(r.m_base.fp == 440);
    }
}