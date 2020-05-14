#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <stdio.h>
#include <fstream>
#include "StringBasics.h"
#include "../src/GroupFromAnnotation.h"
#include "../src/Meta.h"
#include "RMSingleVariantReader.h"
#include "RMGroupTestReader.h"
using namespace std;

bool file_exists(const std::string &name) {
  ifstream f(name.c_str());
  return f.good();
}

TEST_CASE("Program Arguments") {
  SECTION("--range") {
    Meta meta;
    meta.prefix = "test.range";
    meta.setLogFile();
    meta.Region = "1:1-87";
    meta.SKAT = true;
    meta.scorefile.Add("tests/datasets/simulated/region/test.smallchunk.MetaScore.assoc.gz");
    meta.covfile.Add("tests/datasets/simulated/region/test.smallchunk.MetaCov.assoc.gz");

    GroupFromAnnotation group;
    group.groupFile = "tests/datasets/simulated/region/test.smallchunk.mask.tab";

    meta.Prepare();
    group.Run("", meta.log);
    meta.PoolSummaryStat(group);

    // This is pretty wild. If you don't run the printing routine, the single variant p-values aren't stored
    // internally, so group-based tests can't look them up. TODO: refactor this...
    meta.WriteSingleVariantResults(group);
    meta.Run(group);

    // Given the range above, only ZSYH2 should be tested and not the other gene.
    auto group_reader = RMGroupTestReader("test.range.meta.SKAT_.results");
    auto group_rec = group_reader.get_record("ZSYH2");
    auto num_group_rec = group_reader.get_num_records();

    REQUIRE(num_group_rec == 1);
    REQUIRE(group_rec->pvalue_liu == Approx(1.28628e-09));

    // Given the range above, the single variant results should only contain records from position 1:2 to 1:87.
    auto sv_reader = RMSingleVariantReader("test.range.meta.singlevar.results");
    auto num_sv_rec = sv_reader.get_num_records();
    auto sv_rec_first = *sv_reader.begin();
    auto sv_rec_last = *(--sv_reader.end());

    REQUIRE(num_sv_rec == 86);

    REQUIRE(sv_rec_first->chrom == "1");
    REQUIRE(sv_rec_first->pos == 2);
    REQUIRE(sv_rec_first->pvalue == Approx(0.1487579));

    REQUIRE(sv_rec_last->chrom == "1");
    REQUIRE(sv_rec_last->pos == 87);
    REQUIRE(sv_rec_last->pvalue == Approx(0.7183580));

    remove("test.range.raremetal.log");
    remove("test.range.meta.plots.pdf");
    remove("test.range.meta.singlevar.results");
    remove("test.range.meta.SKAT_.results");
  }
}

TEST_CASE("P-value precision") {
  SECTION("Score statistic resulting in very small p-value") {
    Meta meta;

    meta.setLogFile();

    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.score.txt.gz");
    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.score.txt.gz");

    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.cov.txt.gz");
    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.cov.txt.gz");

    GroupFromAnnotation group;
    meta.PoolSummaryStat(group);

    // This is actually an error in the tutorial dataset, where the alt allele is missing and the row is read incorrectly.
    // It ends up being convenient, though, because it creates a record with a very extremely small p-value.
    // The :432 is because raremetal ends up shifting the entire row left by 1 because of the missing alt entry.
    int idx = meta.SNPmaf_name.Find("9:494428375:G:432");

    String snp_name = meta.SNPmaf_name[idx];
    StringArray tmp;
    tmp.AddTokens(snp_name, ":");
    String SNPname_noallele = tmp[0] + ":" + tmp[1];
    int N = meta.SNP_effect_N[idx];
    double U = meta.SNPstat.Double(SNPname_noallele);
    double V = meta.SNP_Vstat.Double(SNPname_noallele);

    SingleVariantResult result(U, V, N);
    REQUIRE(result.effSize == Approx(1074.71));
    REQUIRE(result.log_pvalue == Approx(1727.694));

    // Catch2 can't test for approximate long doubles, have to log10 it
    double p_from_log = -static_cast<double>(log10(result.pvalue));
    REQUIRE(p_from_log == Approx(1727.694));
  }
}

TEST_CASE("Allele frequencies") {
  SECTION("Average and min/max") {
    Meta meta;
    meta.prefix = "test.allelefreq";
    meta.setLogFile();
    meta.averageFreq = true;
    meta.minMaxFreq = true;
    meta.scorefile.Add("tests/datasets/simulated/heterog/study0_raremetal.txt.gz");
    meta.scorefile.Add("tests/datasets/simulated/heterog/study1_raremetal.txt.gz");

    GroupFromAnnotation group;
    meta.Prepare();
    meta.PoolSummaryStat(group);
    meta.WriteSingleVariantResults(group);

    auto score_reader = RMSingleVariantReader("test.allelefreq.meta.singlevar.results");
    auto rec1 = score_reader.get_record("8:875238_G/C");

    REQUIRE(rec1->alt_af_mean == Approx(0.486337));
    REQUIRE(rec1->alt_af_se == Approx(0.0519997));
    REQUIRE(rec1->alt_af_min == Approx(0.4345));
    REQUIRE(rec1->alt_af_max == Approx(0.5385));

    // Note: when given the test files in order of study0, then study1, metal will select "A" as the
    // effect allele. However, raremetal will always report towards the alt allele, which is "G".
    // Remember this when interpreting allele frequencies/effects. A mean of 0.515486 is approx 1 - 0.48.
    auto rec2 = score_reader.get_record("3:1291852_A/G");
    REQUIRE(rec2->alt_af_mean == Approx(0.515486));
    REQUIRE(rec2->alt_af_se == Approx(0.0149234));
    REQUIRE(rec2->alt_af_min == Approx(0.502));
    REQUIRE(rec2->alt_af_max == Approx(0.532));

    remove("test.allelefreq.meta.singlevar.results");
    remove("test.allelefreq.meta.plots.pdf");
    remove("test.allelefreq.raremetal.log");
  }
}

TEST_CASE("File I/O") {
  SECTION("Simple meta-analysis") {
    Meta meta;
    meta.prefix = "test.fileio.simple";
    meta.setLogFile();

    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.score.txt.gz");
    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.score.txt.gz");

    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.cov.txt.gz");
    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.cov.txt.gz");

    GroupFromAnnotation group;
    meta.Prepare();
    meta.PoolSummaryStat(group);
    meta.WriteSingleVariantResults(group);

    auto score_reader = RMSingleVariantReader("test.fileio.simple.meta.singlevar.results");
    auto rec1 = score_reader.get_record("9:44001280_G/A");
    REQUIRE(rec1->pvalue == Approx(0.348151));
    REQUIRE(rec1->effect_size == Approx(0.423643));
    REQUIRE(rec1->effect_stderr == Approx(0.451557));
    REQUIRE(rec1->h2 == Approx(0.000860396));
    REQUIRE(rec1->pooled_alt_af == Approx(0.00244379));
    REQUIRE(score_reader.get_nstudies() == 2);

    auto rec2 = score_reader.get_record("9:494428375_G/432");
    double p_from_log = -static_cast<double>(log10(rec2->pvalue));
    REQUIRE(p_from_log == Approx(1727.694));

    remove("test.fileio.simple.meta.singlevar.results");
    remove("test.fileio.simple.meta.plots.pdf");
    remove("test.fileio.simple.raremetal.log");
  }
}

TEST_CASE("Heterogeneity statistics") {
  SECTION("Should be correct on simple example") {
    Meta meta;

    meta.setLogFile();
    meta.bHeterogeneity = true;
    meta.scorefile.Add("tests/datasets/simulated/heterog/study0_raremetal.txt.gz");
    meta.scorefile.Add("tests/datasets/simulated/heterog/study1_raremetal.txt.gz");

    GroupFromAnnotation group;
    meta.PoolSummaryStat(group);

    // High heterogeneity
    REQUIRE(meta.SNP_heterog_stat.Double("3:1291852") == Approx(109.990000));

    // Low heterogeneity
    REQUIRE(meta.SNP_heterog_stat.Double("8:875238") == Approx(0.357466));
  }
}

TEST_CASE("Tutorial datasets") {
  SECTION("tut_rm") {
    Meta meta;
    meta.prefix = "test.tut_rm";
    meta.setLogFile();

    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.score.txt.gz");
    meta.scorefile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.score.txt.gz");

    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY1.QT1.singlevar.cov.txt.gz");
    meta.covfile.Add("tests/raremetal/test_tut_rm/inputs/STUDY2.QT1.singlevar.cov.txt.gz");

    GroupFromAnnotation group;
    meta.Prepare();
    meta.PoolSummaryStat(group);
    meta.WriteSingleVariantResults(group);

    auto reader_tested = RMSingleVariantReader("test.tut_rm.meta.singlevar.results");
    auto reader_expect = RMSingleVariantReader("tests/raremetal/test_tut_rm/expected/COMBINED.QT1.meta.singlevar.results");

    bool test = reader_tested == reader_expect;
    REQUIRE(test);
  }
}