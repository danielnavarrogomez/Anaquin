#include <ctime>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <getopt.h>
#include <strings.h>
#include <execinfo.h>
#include <sys/stat.h>

#include "RnaQuin/r_fold.hpp"
#include "RnaQuin/r_align.hpp"
#include "RnaQuin/r_report.hpp"
#include "RnaQuin/r_sample.hpp"
#include "RnaQuin/r_express.hpp"
#include "RnaQuin/r_assembly.hpp"

#include "VarQuin/v_germ.hpp"
#include "VarQuin/v_trim.hpp"
#include "VarQuin/v_flip.hpp"
#include "VarQuin/v_copy.hpp"
#include "VarQuin/v_kmer.hpp"
#include "VarQuin/v_align.hpp"
#include "VarQuin/v_calibrate.hpp"
#include "VarQuin/v_somatic.hpp"
#include "VarQuin/v_conjoint.hpp"
#include "VarQuin/v_structure.hpp"

#include "MetaQuin/m_coverage.hpp"
#include "MetaQuin/m_assembly.hpp"

#include "parsers/parser_vcf.hpp"
#include "parsers/parser_blat.hpp"
#include "parsers/parser_fold.hpp"
#include "parsers/parser_cdiff.hpp"
#include "parsers/parser_edgeR.hpp"
#include "parsers/parser_DESeq2.hpp"
#include "parsers/parser_sleuth.hpp"
#include "parsers/parser_express.hpp"
#include "parsers/parser_cufflink.hpp"
#include "parsers/parser_kallisto.hpp"

#include "writers/file_writer.hpp"
#include "writers/terminal_writer.hpp"

#ifdef UNIT_TEST
#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#endif

typedef int Option;

typedef std::string Value;
typedef std::set<Value> Range;

/*
 * Options specified in the command line
 */

#define OPT_TEST    320
#define OPT_TOOL    321
#define OPT_PATH    325
#define OPT_VERSION 338

#define OPT_R_BASE   800
#define OPT_R_BED    801
#define OPT_METHOD   802
#define OPT_R_GTF    803
#define OPT_R_VCF    804
#define OPT_TRIM     805
#define OPT_MIXTURE  806
#define OPT_R_AF     807
#define OPT_R_CON    808
#define OPT_R_CNV    809
#define OPT_FUZZY    810
#define OPT_R_LAD    811
#define OPT_R_IND    812
#define OPT_U_SAMPLE 813
#define OPT_U_SEQS   814
#define OPT_EDGE     817
#define OPT_U_BASE   818

using namespace Anaquin;

// Shared with other modules
bool __showInfo__ = true;

// Shared with other modules
std::string __full_command__;

// Shared with other modules
Path __working__;

// Shared with other modules
Path __output__;

// Shared with other modules
std::string date()
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer, 80, "%d-%m-%Y %H:%M:%S", timeinfo);
    std::string str(buffer);
    
    return str;
}

static std::map<Value, Tool> _tools =
{
    { "Test",           Tool::Test           },
    { "Help",           Tool::Help           },

    { "RnaAlign",       Tool::RnaAlign       },
    { "RnaAssembly",    Tool::RnaAssembly    },
    { "RnaReport",      Tool::RnaReport      },
    { "RnaExpression",  Tool::RnaExpress     },
    { "RnaFoldChange",  Tool::RnaFoldChange  },
    { "RnaSubsample",   Tool::RnaSubsample   },

    { "VarStructure",   Tool::VarStructure   },
    { "VarConjoint",    Tool::VarConjoint    },
    { "VarCopy",        Tool::VarCopy        },
    { "VarAlign",       Tool::VarAlign       },
    { "VarGermline",    Tool::VarGermline    },
    { "VarSomatic",     Tool::VarSomatic     },
    { "VarCalibrate",   Tool::VarCalibrate   },
    { "VarTrim",        Tool::VarTrim        },
    { "VarFlip",        Tool::VarFlip        },
    { "VarKmer",        Tool::VarKmer        },
    { "VarSplit",       Tool::VarSplit       },

    { "MetaCoverage",   Tool::MetaCoverage   },
    { "MetaAssembly",   Tool::MetaAssembly   },
    { "MetaSubsample",  Tool::MetaSubsample  },
};

static std::map<Tool, std::set<Option>> _options =
{
    /*
     * RnaQuin Analysis
     */
    
    { Tool::RnaSubsample,  { OPT_U_SEQS, OPT_METHOD } },
    { Tool::RnaAssembly,   { OPT_R_GTF, OPT_R_LAD, OPT_U_SEQS } },
    { Tool::RnaFoldChange, { OPT_R_LAD, OPT_U_SEQS, OPT_METHOD } },
    { Tool::RnaExpress,    { OPT_R_LAD, OPT_U_SEQS, OPT_METHOD } },
    { Tool::RnaAlign,      { OPT_R_GTF, OPT_U_SEQS } },
    { Tool::RnaReport,     { OPT_R_IND, OPT_R_LAD, OPT_U_SEQS } },

    /*
     * VarQuin Analysis
     */

    { Tool::VarFlip,      { OPT_U_SEQS } },
    { Tool::VarTrim,      { OPT_R_BED, OPT_U_SEQS } },
    { Tool::VarAlign,     { OPT_R_BED, OPT_U_SEQS } },
    { Tool::VarCopy,      { OPT_R_CNV, OPT_R_BED, OPT_U_SAMPLE, OPT_U_SEQS, OPT_METHOD } },
    { Tool::VarCalibrate, { OPT_R_BED, OPT_U_SAMPLE,  OPT_U_SEQS, OPT_METHOD } },
    { Tool::VarGermline,  { OPT_R_BED, OPT_R_VCF, OPT_U_SEQS } },
    { Tool::VarKmer,      { OPT_U_SEQS, OPT_R_AF } },
    { Tool::VarStructure, { OPT_R_VCF, OPT_R_BED, OPT_U_SEQS } },
    { Tool::VarSomatic,   { OPT_R_VCF, OPT_R_BED, OPT_U_SEQS } },
    { Tool::VarSplit,     { OPT_U_SEQS } },
    { Tool::VarConjoint,  { OPT_R_CON } },

    /*
     * MetaQuin Analysis
     */

    { Tool::MetaAssembly, { OPT_R_BED, OPT_R_LAD, OPT_U_SEQS } },
    { Tool::MetaCoverage, { OPT_R_BED, OPT_R_LAD, OPT_U_SEQS } }
};

/*
 * Variables used in argument parsing
 */

struct Parsing
{
    // The path that outputs are written
    std::string path = "output";

    // Input files for sequins (multiple inputs)
    std::vector<FileName> seqs;
    
    // Specific options
    std::map<Option, std::string> opts;
    
    // How Anaquin is invoked
    std::string command;

    // Mixture A or mixture B
    Mixture mix = Mix_1;
    
    Proportion sampled = NAN;
    
    Tool tool;
};

// Wrap the variables so that it'll be easier to reset them
static Parsing _p;

static FileName __mockGTFRef__;

void SetGTFRef(const FileName &file)
{
    __mockGTFRef__ = file;
}

FileName GTFRef()
{
    return !__mockGTFRef__.empty() ? __mockGTFRef__ : _p.opts.at(OPT_R_GTF);
}

FileName LadRef() { return _p.opts.at(OPT_R_LAD); }
FileName CNVRef() { return _p.opts.at(OPT_R_CNV); }
FileName ConRef() { return _p.opts.at(OPT_R_CON); }
FileName AFRef()  { return _p.opts.at(OPT_R_AF);  }
FileName BedRef() { return _p.opts.at(OPT_R_BED); }
FileName VCFRef() { return _p.opts.at(OPT_R_VCF); }

static Scripts fixManual(const Scripts &str)
{
    auto x = str;
    
    boost::replace_all(x, "<b>", "\e[1m");
    boost::replace_all(x, "</b>", "\e[0m");
    boost::replace_all(x, "<i>", "\e[3m");
    boost::replace_all(x, "</i>", "\e[0m");
    
    return x;
}

struct InvalidUsageException : public std::exception {};

struct InvalidOptionException : public std::exception
{
    InvalidOptionException(const std::string &opt) : opt(opt) {}

    const std::string opt;
};

struct InvalidValueException : public std::exception
{
    InvalidValueException(const std::string &opt, const std::string &val) : opt(opt), val(val) {}

    const std::string opt, val;
};

struct InvalidToolError : public InvalidValueException
{
    InvalidToolError(const std::string &val) : InvalidValueException("-t", val) {}
};

struct MissingOptionError : public std::exception
{
    MissingOptionError(const std::string &opt) : opt(opt) {}
    MissingOptionError(const std::string &opt, const std::string &range) : opt(opt), range(range) {}

    // Option that is missing
    const std::string opt;
    
    // Possible values for the missing option
    const std::string range;
};

struct UnknownFormatError : public std::runtime_error
{
    UnknownFormatError() : std::runtime_error("Unknown format") {}
};

/*
 * Argument options
 */

static const char *short_options = ":";

static const struct option long_options[] =
{
    { "v",       no_argument, 0, OPT_VERSION },
    { "version", no_argument, 0, OPT_VERSION },

    { "usequin", required_argument, 0, OPT_U_SEQS  },
    { "usample", required_argument, 0, OPT_U_SAMPLE },

    { "rbed",    required_argument, 0, OPT_R_BED  },
    { "rgtf",    required_argument, 0, OPT_R_GTF  },
    { "rvcf",    required_argument, 0, OPT_R_VCF  },
    { "rind",    required_argument, 0, OPT_R_IND  },

    { "raf",     required_argument, 0, OPT_R_AF   }, // Ladder for allele frequency
    { "rcnv",    required_argument, 0, OPT_R_CNV  }, // Ladder for copy number variation
    { "rcon",    required_argument, 0, OPT_R_CON  }, // Ladder for conjoint k-mers
    { "rmix",    required_argument, 0, OPT_R_LAD  }, // Ladder for everything else (RnaQuin and MetaQuin)
    
    { "mix",     required_argument, 0, OPT_MIXTURE },
    { "method",  required_argument, 0, OPT_METHOD  },
    { "trim",    required_argument, 0, OPT_TRIM    },

    { "edge",    required_argument, 0, OPT_EDGE   },
    { "fuzzy",   required_argument, 0, OPT_FUZZY  },
    
    { "o",       required_argument, 0, OPT_PATH },

    {0, 0, 0, 0 }
};

// Used by modules without std::iostream defined
void printWarning(const std::string &msg)
{
    std::cout << "[Warn]: " << msg << std::endl;
}

static std::string optToStr(int opt)
{
    for (const auto &o : long_options)
    {
        if (o.val == opt)
        {
            return o.name;
        }
    }
    
    throw std::runtime_error("Invalid option: " + std::to_string(opt));
}

static void printUsage()
{
    extern Scripts Manual();
    std::cout << fixManual(Manual()) << std::endl;
}

static Scripts manual(Tool tool)
{
    extern Scripts VarCopy();
    extern Scripts VarGermline();
    extern Scripts VarFlip();
    extern Scripts VarTrim();
    extern Scripts VarAlign();
    extern Scripts VarSomatic();
    extern Scripts VarKmer();
    extern Scripts VarCalibrate();
    extern Scripts VarConjoint();
    extern Scripts VarStructure();
    extern Scripts RnaAlign();
    extern Scripts RnaReport();
    extern Scripts RnaAssembly();
    extern Scripts RnaSubsample();
    extern Scripts RnaExpression();
    extern Scripts RnaFoldChange();
    extern Scripts MetaCoverage();
    extern Scripts MetaAssembly();
    extern Scripts MetaSubsample();
    
    switch (tool)
    {
        case Tool::RnaAlign:       { return RnaAlign();       }
        case Tool::RnaAssembly:    { return RnaAssembly();    }
        case Tool::RnaExpress:     { return RnaExpression();  }
        case Tool::RnaFoldChange:  { return RnaFoldChange();  }
        case Tool::RnaSubsample:   { return RnaSubsample();   }
        case Tool::RnaReport:      { return RnaReport();      }
        case Tool::VarCopy:        { return VarCopy();        }
        case Tool::VarFlip:        { return VarFlip();        }
        case Tool::VarSomatic:     { return VarSomatic();     }
        case Tool::VarAlign:       { return VarAlign();       }
        case Tool::VarCalibrate:   { return VarCalibrate();   }
        case Tool::VarKmer:        { return VarKmer();        }
        case Tool::VarGermline:    { return VarGermline();    }
        case Tool::VarConjoint:    { return VarConjoint();    }
        case Tool::VarStructure:   { return VarStructure();   }
        case Tool::MetaAssembly:   { return MetaAssembly();   }
        case Tool::MetaCoverage:   { return MetaCoverage();   }
        default:                   { return ""; }
    }
}

static void readGTF(Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.g1 = Standard::readGTF(Reader(_p.opts[key]));
    }
}

template <typename F> void readT1(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.t1 = std::shared_ptr<Translate>(new Translate(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readT2(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.t2 = std::shared_ptr<Translate>(new Translate(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL1(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l1 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL2(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l2 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL3(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l3 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL4(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l4 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL5(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l5 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

template <typename F> void readL6(F f, Option key, UserReference &r)
{
    if (_p.opts.count(key))
    {
        r.l6 = std::shared_ptr<Ladder>(new Ladder(f(Reader(_p.opts[key]))));
    }
}

static void readVCF2(Option opt, UserReference &r, Base trim = 0)
{
    typedef SequinVariant::Context Context;
    
    if (!_p.opts[opt].empty())
    {
        r.v2 = std::shared_ptr<VCFLadder>(new VCFLadder(Standard::addVCF(Reader(_p.opts[opt]), std::set<Context> {})));
    }
}

static void readVCFSom1(Option opt, UserReference &r, Base trim = 0)
{
    typedef SequinVariant::Context Context;
    
    if (!_p.opts[opt].empty())
    {
        r.v1 = std::shared_ptr<VCFLadder>(new VCFLadder(Standard::addVCF(Reader(_p.opts[opt]),
            std::set<Context>
            {
                Context::Common,
                Context::VeryLowGC,
                Context::LowGC,
                Context::HighGC,
                Context::VeryHighGC,
                Context::ShortDinRep,
                Context::LongDinRep,
                Context::ShortHompo,
                Context::LongHompo,
                Context::ShortQuadRep,
                Context::LongQuadRep,
                Context::ShortTrinRep,
                Context::LongTrinRep,
            })));
    }
}

static void readVCFNoSom1(Option opt, UserReference &r, Base trim = 0)
{
    typedef SequinVariant::Context Context;

    if (!_p.opts[opt].empty())
    {
        r.v1 = std::shared_ptr<VCFLadder>(new VCFLadder(Standard::addVCF(Reader(_p.opts[opt]),
                    std::set<Context> { Context::Cancer })));
    }
}

static void readReg1(Option opt, UserReference &r, Base trim = 0)
{
    if (!_p.opts[opt].empty())
    {
        r.r1 = std::shared_ptr<BedData>(new BedData(Standard::readBED(Reader(_p.opts[opt]), trim)));
    }
}

static void readReg2(Option opt, UserReference &r, Base trim = 0)
{
    if (!_p.opts[opt].empty())
    {
        r.r2 = std::shared_ptr<BedData>(new BedData(Standard::readBED(Reader(_p.opts[opt]), trim)));
    }
}

// Apply a reference source given where it comes from
template <typename Reference> void applyRef(Reference ref, Option opt)
{
    if (__showInfo__)
    {
        std::cout << "[INFO]: Loading: " << _p.opts[opt] << std::endl;
    }
    
    if (!_p.opts[opt].empty())
    {
        ref(Reader(_p.opts[opt]));
    }
}

template <typename Analyzer, typename F> void startAnalysis(F f, typename Analyzer::Options o)
{
    const auto path = _p.path;

    // This might be needed for scripting
    __full_command__ = _p.command;

#ifndef DEBUG
    o.writer = std::shared_ptr<FileWriter>(new FileWriter(path));
    o.logger = std::shared_ptr<FileWriter>(new FileWriter(path));
    o.output = std::shared_ptr<TerminalWriter>(new TerminalWriter());
    o.logger->open("anaquin.log");
#endif
    
    system(("mkdir -p " + path).c_str());
    
    o.work  = path;
    
    auto t  = std::time(nullptr);
    auto tm = *std::localtime(&t);

    o.info(_p.command);
    o.info(date());
    o.info("Path: " + path);

    std::clock_t begin = std::clock();

    f(o);
    
    std::clock_t end = std::clock();

    const auto elapsed = (boost::format("Completed. %1% seconds.") % (double(end - begin) / CLOCKS_PER_SEC)).str();
    o.info(elapsed);

#ifndef DEBUG
    o.logger->close();
#endif
}

template <typename Analyzer, typename Files> void analyze(const Files &files, typename Analyzer::Options o = typename Analyzer::Options())
{
    return startAnalysis<Analyzer>([&](const typename Analyzer::Options &o)
    {
        Analyzer::report(files, o);
    }, o);
}

template <typename Analyzer> void analyze_1(Option x, typename Analyzer::Options o = typename Analyzer::Options())
{
    return analyze<Analyzer>(_p.opts.at(x), o);
}

template <typename Analyzer> void analyze_2(Option x1, Option x2, typename Analyzer::Options o = typename Analyzer::Options())
{
    return startAnalysis<Analyzer>([&](const typename Analyzer::Options &o)
    {
        #define D(x) _p.opts.count(x) ? _p.opts[x] : ""
        Analyzer::report(D(x1), D(x2), o);
    }, o);
}

template < typename Analyzer> void analyze_n(typename Analyzer::Options o = typename Analyzer::Options())
{
    return startAnalysis<Analyzer>([&](const typename Analyzer::Options &o)
    {
        Analyzer::report(_p.seqs, o);
    }, o);
}

static void fixInputs(int argc, char ** argv)
{
    for (auto i = 0; i < argc; i++)
    {
        if (argv[i] && strlen(argv[i]) && argv[i][0] == (char)-30)
        {
            auto tmp = std::string(argv[i]);
            
            auto invalid = [&](char c)
            {
                return !(c>=0 && c<128);
            };
            
            tmp.erase(remove_if(tmp.begin(), tmp.end(), invalid), tmp.end());
            tmp = "-" + tmp;

            strcpy(argv[i], tmp.c_str());
        }
    }
}

void parse(int argc, char ** argv)
{
    auto tmp = new char*[argc+1];
    
    for (auto i = 0; i < argc; i++)
    {
        tmp[i] = (char *) malloc((strlen(argv[i]) + 1) * sizeof(char));
        strcpy(tmp[i], argv[i]);
    }
    
    fixInputs(argc, argv=tmp);

    auto &tool = _p.tool;
    
    _p = Parsing();

    if (argc <= 1)
    {
        printUsage();
        return;
    }

    // Required for unit-testing
    optind = 1;

    /*
     * Reconstruct the overall command
     */
    
    for (int i = 0; i < argc; i++)
    {
        _p.command += std::string(argv[i]) + " ";
    }

    A_ASSERT(!_p.command.empty());

    int next, index;

    // Attempt to parse and store a floating point from string
    auto parseDouble = [&](const std::string &str, double &r)
    {
        A_ASSERT(next);
        
        try
        {
            r = stof(str);
        }
        catch (...)
        {
            throw std::runtime_error(str + " is not a valid floating number. Please check and try again.");
        }
    };

    auto checkPath = [&](const Path &path)
    {
        if (path[0] == '/')
        {
            return path;
        }
        else
        {
            return __working__ + "/" + path;
        }
    };
    
    auto checkFile = [&](const FileName &file)
    {
        if (!std::ifstream(file).good())
        {
            throw InvalidFileError(file);
        }
    };

    /*
     * Pre-process arguments. This way, we can examine the options in whatever order we'd like to
     */

    std::vector<Option> opts;
    std::vector<Value>  vals;

    if (!_tools.count(argv[1]) && strcmp(argv[1], "-v"))
    {
        throw InvalidToolError(argv[1]);
    }
    else
    {
        _p.tool = _tools[argv[1]];
    }
    
    if (_p.tool == Tool::VarCalibrate || _p.tool == Tool::RnaSubsample || _p.tool == Tool::VarTrim || _p.tool == Tool::VarCopy)
    {
        __showInfo__ = false;
    }
    
    const auto isHelp = argc >= 3 && (!strcmp(argv[2], "-h") || !strcmp(argv[2], "--help"));

    if (isHelp)
    {
        if (argc != 3)
        {
            throw std::runtime_error("Too many arguments for help usage. Usage: anaquin <tool> -h or anaquin <tool> --help");
        }

        std::cout << fixManual(manual(_p.tool)) << std::endl << std::endl;
        return;
    }
    
    unsigned n = 2;

    while ((next = getopt_long_only(argc, argv, short_options, long_options, &index)) != -1)
    {
        if (next < OPT_TOOL)
        {
            throw InvalidOptionException(argv[n]);
        }
        
        opts.push_back(next);
        
        // Whether this option has an value
        const auto hasValue = optarg;
        
        n += hasValue ? 2 : 1;
        
        vals.push_back(hasValue ? std::string(optarg) : "");
    }

    for (auto i = 0; i < opts.size(); i++)
    {
        auto opt = opts[i];
        auto val = vals[i];

        switch (opt)
        {
            case OPT_EDGE:
            case OPT_FUZZY:
            {
                try
                {
                    stoi(val);
                    _p.opts[opt] = val;
                }
                catch (...)
                {
                    throw std::runtime_error(val + " is not an integer. Please check and try again.");
                }

                break;
            }

            case OPT_METHOD:
            {
                switch (_p.tool)
                {
                    case Tool::VarCopy:
                    case Tool::VarCalibrate:
                    case Tool::VarGermline:
                    case Tool::VarSomatic:
                    case Tool::RnaExpress:
                    case Tool::VarStructure:
                    case Tool::RnaFoldChange: { _p.opts[opt] = val; break; }

                    case Tool::RnaSubsample:
                    case Tool::MetaSubsample:
                    {
                        parseDouble(_p.opts[opt] = val, _p.sampled);
                        
                        if (_p.sampled <= 0.0)
                        {
                            throw std::runtime_error("Invalid value for -method. Sampling fraction must be greater than zero.");
                        }
                        else if (_p.sampled >= 1.0)
                        {
                            throw std::runtime_error("Invalid value for -method. Sampling fraction must be less than one.");
                        }
                        
                        break;
                    }
                        
                    default : { break; }
                }
                
                break;
            }

            case OPT_TRIM:
            case OPT_R_AF:
            case OPT_R_CNV:
            case OPT_R_LAD:
            case OPT_R_IND:
            case OPT_R_CON: { _p.opts[opt] = val; break; }

            case OPT_MIXTURE:
            {
                if      (val == "A") { _p.mix = Mixture::Mix_1; }
                else if (val == "B") { _p.mix = Mixture::Mix_2; }
                else                 { throw InvalidValueException("-mix", val); }
                break;
            }

            case OPT_R_VCF:
            case OPT_R_BED:
            case OPT_R_GTF:
            case OPT_U_SEQS:
            case OPT_U_SAMPLE:
            {
                if (opt == OPT_U_SEQS)
                {
                    std::vector<FileName> temp;
                    Tokens::split(val, ",", temp);
                    
                    for (auto i = 0; i < temp.size(); i++)
                    {
                        checkFile(_p.opts[opt] = temp[i]);
                        _p.seqs.push_back(temp[i]);
                    }
                }
                
                checkFile(_p.opts[opt] = val); break;
            }

            case OPT_PATH: { _p.path = val; break; }

            default: { throw InvalidUsageException(); }
        }
    }

    __output__ = _p.path = checkPath(_p.path);
    
    /*
     * Have all the required options given?
     */
    
    std::set<Option> required;
    
    std::copy_if(_options[_p.tool].begin(), _options[_p.tool].end(), std::inserter(required, required.end()), [&](const Option &x)
    {
        for (auto &o : long_options)
        {
            if (o.val == x)
            {
                return o.has_arg == required_argument;
            }
        }

        A_THROW("Option " + std::to_string(x) + " no found");
    });

    for (const auto i : _p.opts)
    {
        if (required.count(i.first))
        {
            required.erase(i.first);
        }
    }
    
    if (!required.empty())
    {
        throw MissingOptionError("-" + optToStr(*required.begin()));
    }

    if (__showInfo__)
    {
        std::cout << "-----------------------------------------" << std::endl;
        std::cout << "------------- Sequin Analysis -----------" << std::endl;
        std::cout << "-----------------------------------------" << std::endl << std::endl;        
    }

    UserReference r;
    
    auto &s = Standard::instance();
    
    switch (_p.tool)
    {
        case Tool::Test:
        {
#ifdef UNIT_TEST
            Catch::Session().run(1, argv);
#else
            A_THROW("UNIT_TEST is not defined");
#endif
            break;
        }

        case Tool::RnaAlign:
        case Tool::RnaReport:
        case Tool::RnaExpress:
        case Tool::RnaAssembly:
        case Tool::RnaSubsample:
        case Tool::RnaFoldChange:
        {
            if (__showInfo__)
            {
                std::cout << "[INFO]: RNA-Seq Analysis" << std::endl;
            }

            if (_p.tool != Tool::RnaSubsample)
            {
                switch (_p.tool)
                {
                    case Tool::RnaAlign:
                    {
                        readGTF(OPT_R_GTF, r);
                        break;
                    }

                    case Tool::RnaAssembly:
                    {
                        readGTF(OPT_R_GTF, r);
                        readL1(std::bind(&Standard::readIsoform, &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL2(std::bind(&Standard::readGene,    &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL3(std::bind(&Standard::readLength,  &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL4(std::bind(&Standard::readGeneL,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL5(std::bind(&Standard::readIDiff,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL6(std::bind(&Standard::readGDiff,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        break;
                    }

                    case Tool::RnaExpress:
                    case Tool::RnaFoldChange:
                    {
                        readL1(std::bind(&Standard::readIsoform, &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL2(std::bind(&Standard::readGene,    &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL3(std::bind(&Standard::readLength,  &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL4(std::bind(&Standard::readGeneL,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL5(std::bind(&Standard::readIDiff,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        readL6(std::bind(&Standard::readGDiff,   &s, std::placeholders::_1), OPT_R_LAD, r);
                        break;
                    }

                    default: { break; }
                }

                s.r_rna.finalize(_p.tool, r);
            }

            switch (_p.tool)
            {
                case Tool::RnaReport:
                {
                    startAnalysis<RReport>([&](const typename RReport::Options &o)
                    {
                        RReport::report(_p.opts.at(OPT_R_IND), _p.opts.at(OPT_R_LAD), _p.seqs[0], o);
                    }, RReport::Options());

                    break;
                }
                    
                case Tool::RnaAlign:    { analyze_1<RAlign>(OPT_U_SEQS);    break; }
                case Tool::RnaAssembly:
                {
                    RAssembly::Options o;
                    o.mix = _p.mix;
                    analyze_1<RAssembly>(OPT_U_SEQS, o);                    
                    break;
                }

                case Tool::RnaSubsample:
                {
                    RSample::Options o;
                    o.p = _p.sampled;
                    analyze_1<RSample>(OPT_U_SEQS, o);
                    break;
                }
                    
                case Tool::RnaExpress:
                {
                    RExpress::Options o;
                    o.mix = _p.mix;
                    
                    if (_p.opts[OPT_METHOD] != "gene" && _p.opts[OPT_METHOD] != "isoform")
                    {
                        throw InvalidValueException("-method", _p.opts[OPT_METHOD]);
                    }

                    o.metrs = _p.opts[OPT_METHOD] == "gene" ? RExpress::Metrics::Gene : RExpress::Metrics::Isoform;
                    
                    const auto &file = _p.seqs[0];
                    
                    // Is this a GTF by extension?
                    const auto isGTF = file.find(".gtf") != std::string::npos;
                    
                    if (isGTF)
                    {
                        o.format = RExpress::Format::GTF;
                    }
                    else if (ParserExpress::isExpress(file))
                    {
                        o.format = RExpress::Format::Text;
                    }
                    else if (ParserKallisto::isKallisto(file))
                    {
                        o.format = RExpress::Format::Kallisto;
                    }
                    else
                    {
                        A_THROW("Unknown file type: " + file + ". Input file should be a GTF file or Anaquin format. Please check our user guide for details.");
                    }

                    analyze_n<RExpress>(o);
                    break;
                }

                case Tool::RnaFoldChange:
                {
                    RFold::Options o;

                    if (_p.opts[OPT_METHOD] != "gene" && _p.opts[OPT_METHOD] != "isoform")
                    {
                        throw InvalidValueException("-method", _p.opts[OPT_METHOD]);
                    }
                    
                    o.metrs = _p.opts[OPT_METHOD] == "gene" ? RFold::Metrics::Gene : RFold::Metrics::Isoform;
                    
                    const auto &file = _p.seqs[0];
                    
                    if (ParserCDiff::isTracking(Reader(file)))
                    {
                        o.format = RFold::Format::Cuffdiff;
                        std::cout << "[INFO]: Cuffdiff format" << std::endl;
                    }
                    else if (ParserSleuth::isSleuth(Reader(file)))
                    {
                        o.format = RFold::Format::Sleuth;
                        std::cout << "[INFO]: Sleuth format" << std::endl;
                    }
                    else if (ParserDESeq2::isDESeq2(Reader(file)))
                    {
                        o.format = RFold::Format::DESeq2;
                        std::cout << "[INFO]: DESeq2 format" << std::endl;
                    }
                    else if (ParserEdgeR::isEdgeR(Reader(file)))
                    {
                        o.format = RFold::Format::edgeR;
                        std::cout << "[INFO]: edgeR format" << std::endl;
                    }
                    else if (ParserDiff::isDiff(Reader(file)))
                    {
                        o.format = RFold::Format::Anaquin;
                        std::cout << "[INFO]: Anaquin format" << std::endl;
                    }
                    else
                    {
                        throw std::runtime_error("Unknown file format: " + file + ". Anaquin supports Cuffdiff, DESeq2, edgeR and RnaQuin FoldChange format. Please note the input file requires a header.");
                    }

                    analyze_1<RFold>(OPT_U_SEQS, o);
                    break;
                }

                default : { break; }
            }

            break;
        }

        case Tool::MetaCoverage:
        case Tool::MetaAssembly:
        case Tool::MetaSubsample:
        {
            std::cout << "[INFO]: Metagenomics Analysis" << std::endl;
            
            switch (_p.tool)
            {
                case Tool::MetaCoverage:
                {
                    readReg1(OPT_R_BED, r);
                    readL1(std::bind(&Standard::addMMix, &s, std::placeholders::_1), OPT_R_LAD, r);
                    break;
                }

                case Tool::MetaAssembly:
                {
                    readReg1(OPT_R_BED, r);
                    readL1(std::bind(&Standard::addMMix, &s, std::placeholders::_1), OPT_R_LAD, r);
                    break;
                }

                case Tool::MetaSubsample:
                {
                    readReg1(OPT_R_BED, r);
                    break;
                }

                default: { break; }
            }
            
            Standard::instance().r_meta.finalize(_p.tool, r);
            
            switch (_p.tool)
            {
                case Tool::MetaCoverage:
                {
                    MCoverage::Options o;
                    o.mix = _p.mix;
                    
                    if (_p.seqs.size() == 1)
                    {
                        o.format = MCoverage::Format::BAM;
                    }
                    else if (_p.seqs.size() == 2)
                    {
                        o.format = MCoverage::Format::RayMeta;
                    }
                    else
                    {
                        throw UnknownFormatError();
                    }
                    
                    analyze_n<MCoverage>(o);
                    break;
                }

                case Tool::MetaAssembly:
                {
                    MAssembly::Options o;
                    o.mix = _p.mix;
                    
                    // This is the only supporting format
                    o.format = MAssembly::Format::Blat;

                    analyze_n<MAssembly>(o);
                    break;
                }

                default: { break; }
            }

            break;
        }

        case Tool::VarCopy:
        case Tool::VarFlip:
        case Tool::VarTrim:
        case Tool::VarKmer:
        case Tool::VarSplit:
        case Tool::VarAlign:
        case Tool::VarGermline:
        case Tool::VarSomatic:
        case Tool::VarConjoint:
        case Tool::VarCalibrate:
        case Tool::VarStructure:
        {
            if (__showInfo__)
            {
                std::cout << "[INFO]: Variant Analysis" << std::endl;
            }

            switch (_p.tool)
            {
                case Tool::VarFlip:  { readReg1(OPT_R_BED, r); break; }
                case Tool::VarSplit: { readReg1(OPT_R_BED, r); break; }
                    
                case Tool::VarAlign:
                {
                    readReg1(OPT_R_BED, r);
                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
                    break;
                }
                    
                case Tool::VarStructure:
                {
                    assert(false);
//                    readReg1(OPT_R_BED, r);
//                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
//                    readVCFNoCancer(OPT_R_VCF, r);
                    break;
                }
                    
                case Tool::VarConjoint:
                {
                    readL1(std::bind(&Standard::addCon1, &s, std::placeholders::_1), OPT_R_CON, r);
                    readL2(std::bind(&Standard::addCon2, &s, std::placeholders::_1), OPT_R_CON, r);
                    readT1(std::bind(&Standard::addSeq2Unit, &s, std::placeholders::_1), OPT_R_CON, r);
                    readT2(std::bind(&Standard::addUnit2Seq, &s, std::placeholders::_1), OPT_R_CON, r);
                    break;
                }
                    
                case Tool::VarCopy:
                {
                    readL1(std::bind(&Standard::addCNV, &s, std::placeholders::_1), OPT_R_CNV, r);
                    readReg1(OPT_R_BED, r);
                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
                    break;
                }
                    
                case Tool::VarCalibrate:
                {
                    readReg1(OPT_R_BED, r);
                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
                    break;
                }

                case Tool::VarSomatic:
                {
                    readReg1(OPT_R_BED, r);
                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
                    readVCFSom1(OPT_R_VCF, r);
                    readVCF2(OPT_R_VCF, r);
                    break;
                }
                    
                case Tool::VarGermline:
                {
                    readReg1(OPT_R_BED, r);
                    readReg2(OPT_R_BED, r, _p.opts.count(OPT_EDGE) ? stoi(_p.opts[OPT_EDGE]) : 0);
                    readVCFNoSom1(OPT_R_VCF, r);
                    readVCF2(OPT_R_VCF, r);
                    break;
                }
                    
                case Tool::VarKmer:
                {
                    readL1(std::bind(&Standard::addAF, &s, std::placeholders::_1), OPT_R_AF, r);
                    break;
                }
                    
                default: { break; }
            }
            
            Standard::instance().r_var.finalize(_p.tool, r);

            switch (_p.tool)
            {
                case Tool::VarFlip:     { analyze_1<VFlip>(OPT_U_SEQS);     break; }
                case Tool::VarKmer:     { analyze_1<VarKmer>(OPT_U_SEQS);   break; }
                case Tool::VarConjoint: { analyze_1<VConjoint>(OPT_U_SEQS); break; }

                case Tool::VarAlign:
                {
                    VAlign::Options o;
                    
                    if (_p.opts.count(OPT_EDGE))
                    {
                        o.edge = stoi(_p.opts[OPT_EDGE]);
                    }

                    analyze_2<VAlign>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }

                case Tool::VarTrim:
                {
                    VTrim::Options o;
                    
                    if (_p.opts.count(OPT_METHOD))
                    {
                        const auto &x = _p.opts.at(OPT_METHOD);
                        
                        if (x == "leftRight")  { o.meth = VTrim::Method::LeftRight; }
                        else if (x == "left")  { o.meth = VTrim::Method::Left;      }
                        else if (x == "right") { o.meth = VTrim::Method::Right;     }
                        else
                        {
                            throw InvalidValueException("-method", x);
                        }
                    }

                    analyze_1<VTrim>(OPT_U_SEQS, o);
                    break;
                }

                case Tool::VarSomatic:
                {
                    VSomatic::Options o;
                    
                    if (_p.opts.count(OPT_METHOD))
                    {
                        const auto &x = _p.opts.at(OPT_METHOD);
                        
                        if (x == "pass")     { o.meth = VSomatic::Method::Passed;         }
                        else if (x == "all") { o.meth = VSomatic::Method::NotFiltered;    }
                        else                 { throw InvalidValueException("-method", x); }
                    }
                    
                    analyze_2<VSomatic>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }
                    
                case Tool::VarGermline:
                {
                    VGerm::Options o;
                    
                    if (_p.opts.count(OPT_METHOD))
                    {
                        const auto &x = _p.opts.at(OPT_METHOD);
                        
                        if (x == "pass")     { o.meth = VGerm::Method::Passed;      }
                        else if (x == "all") { o.meth = VGerm::Method::NotFiltered; }
                        else                 { throw InvalidValueException("-method", x); }
                    }
                    
                    analyze_2<VGerm>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }
                
                case Tool::VarStructure:
                {
                    VStructure::Options o;
                    
                    if (_p.opts.count(OPT_METHOD))
                    {
                        const auto &x = _p.opts.at(OPT_METHOD);
                        
                        if (x == "pass")     { o.meth = VStructure::Method::Passed;       }
                        else if (x == "all") { o.meth = VStructure::Method::NotFiltered;  }
                        else                 { throw InvalidValueException("-method", x); }
                    }

                    analyze_2<VStructure>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }

                case Tool::VarCopy:
                {
                    VCopy::Options o;
                    
                    // Eg: "mean", "median", "reads", "0.75"
                    const auto meth = _p.opts[OPT_METHOD];
                    
                    auto isFloat = [&]()
                    {
                        std::istringstream iss(meth);
                        float f;
                        iss >> std::noskipws >> f;
                        return iss.eof() && !iss.fail();
                    };
                    
                    if (meth == "mean")
                    {
                        o.meth = VCalibrate::Method::Mean;
                    }
                    else if (meth == "median")
                    {
                        o.meth = VCalibrate::Method::Median;
                    }
                    else if (meth == "reads")
                    {
                        o.meth = VCalibrate::Method::Reads;
                    }
                    else if (isFloat())
                    {
                        o.p = stod(meth);
                        o.meth = VCalibrate::Method::Prop;
                        
                        if (o.p <= 0.0)
                        {
                            throw std::runtime_error("Normalization factor must be greater than zero");
                        }
                        else if (o.p >= 1.0)
                        {
                            throw std::runtime_error("Normalization factor must be less than one");
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unknown method: " + meth);
                    }
                    
                    if (_p.opts.count(OPT_EDGE))
                    {
                        o.edge = stoi(_p.opts[OPT_EDGE]);
                    }
                    
                    analyze_2<VCopy>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }

                case Tool::VarCalibrate:
                {
                    VCalibrate::Options o;
                    
                    if (_p.opts.count(OPT_TRIM))
                    {
                        if      (_p.opts[OPT_TRIM] == "both") { o.trim = true;  }
                        else if (_p.opts[OPT_TRIM] == "none") { o.trim = false; }
                        else { throw InvalidOptionException(_p.opts[OPT_TRIM]); }
                    }
                    
                    // Eg: "mean", "median", "reads", "0.75"
                    const auto meth = _p.opts[OPT_METHOD];
                    
                    auto isFloat = [&]()
                    {
                        std::istringstream iss(meth);
                        float f;
                        iss >> std::noskipws >> f;
                        return iss.eof() && !iss.fail();
                    };
                    
                    if (meth == "mean")
                    {
                        o.meth = VCalibrate::Method::Mean;
                    }
                    else if (meth == "median")
                    {
                        o.meth = VCalibrate::Method::Median;
                    }
                    else if (meth == "reads")
                    {
                        o.meth = VCalibrate::Method::Reads;
                    }
                    else if (isFloat())
                    {
                        o.p = stod(meth);
                        o.meth = VCalibrate::Method::Prop;
                        
                        if (o.p <= 0.0)
                        {
                            throw std::runtime_error("Normalization factor must be greater than zero");
                        }
                        else if (o.p >= 1.0)
                        {
                            throw std::runtime_error("Normalization factor must be less than one");
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unknown method: " + meth);
                    }

                    if (_p.opts.count(OPT_EDGE))
                    {
                        o.edge = stoi(_p.opts[OPT_EDGE]);
                    }

                    analyze_2<VCalibrate>(OPT_U_SAMPLE, OPT_U_SEQS, o);
                    break;
                }

                default : { break; }
            }

            break;
        }

        default : { break; }
    }
}

extern int parse_options(int argc, char ** argv)
{
    char cwd[1024];
    
    auto printError = [&](const std::string &x)
    {
        std::cerr << "***********************" << std::endl;
        std::cerr << "[ERRO]: " << x << std::endl;
        std::cerr << "***********************" << std::endl;
    };
    
    if (getcwd(cwd, sizeof(cwd)))
    {
        __working__ = cwd;
    }
    
    try
    {
        parse(argc, argv);
        return 0;
    }
    catch (const FailedCommandException &ex)
    {
        printError("Invalid command: " + std::string(ex.what()));
    }
    catch (const InvalidFormatException &ex)
    {
        printError("Invalid file format: " + std::string(ex.what()));
    }
    catch (const UnknownFormatError &ex)
    {
        printError("Unknown format for the input file(s)");
    }
    catch (const InvalidUsageException &ex)
    {
        printError("Invalid usage. Please check and try again.");
    }
    catch (const InvalidToolError &ex)
    {
        printError("Invalid command. Unknown tool: " + ex.val + ". Please check your usage and try again.");
    }
    catch (const InvalidOptionException &ex)
    {
        printError((boost::format("Invalid usage. Unknown option: %1%") % ex.opt).str());
    }
    catch (const InvalidValueException &ex)
    {
        printError((boost::format("Invalid command. %1% not expected for %2%.") % ex.val % ex.opt).str());
    }
    catch (const MissingOptionError &ex)
    {
        const auto format = "Invalid command. Mandatory option is missing. Please specify %1%.";
        printError((boost::format(format) % ex.opt).str());
    }
    catch (const InvalidFileError &ex)
    {
        printError((boost::format("%1%%2%") % "Invalid command. File is invalid: " % ex.file).str());
    }
    catch (const std::exception &ex)
    {
        printError(ex.what());
    }

    return 1;
}

int main(int argc, char ** argv)
{
    return parse_options(argc, argv);
}
