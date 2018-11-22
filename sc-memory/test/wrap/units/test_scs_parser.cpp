/*
* This source file is part of an OSTIS project. For the latest info, see http://ostis.net
* Distributed under the MIT License
* (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
*/

#include "sc-memory/cpp/utils/sc_test.hpp"
#include "sc-memory/cpp/scs/scs_parser.hpp"

#include <glib.h>

namespace
{

#define SPLIT_TRIPLE(t) \
  auto const & src = parser.GetParsedElement(t.m_source); \
  auto const & edge = parser.GetParsedElement(t.m_edge); \
  auto const & trg = parser.GetParsedElement(t.m_target);

struct TripleElement
{
  TripleElement(ScType const & type)
    : m_type(type)
    , m_visibility(scs::Visibility::System)
  {
  }

  TripleElement(ScType const & type, std::string const & idtf)
    : m_type(type)
    , m_idtf(idtf)
    , m_visibility(scs::Visibility::System)
  {
  }

  TripleElement(ScType const & type, std::string const & idtf, scs::Visibility const & vis)
    : m_type(type)
    , m_idtf(idtf)
    , m_visibility(vis)
  {
  }

  TripleElement(ScType const & type, scs::Visibility const & vis)
    : m_type(type)
    , m_visibility(vis)
  {
  }

  void Test(scs::ParsedElement const & el) const
  {
    SC_CHECK_EQUAL(m_type, el.GetType(), ());
    if (!m_idtf.empty())
      SC_CHECK_EQUAL(m_idtf, el.GetIdtf(), ());

    SC_CHECK_EQUAL(m_visibility, el.GetVisibility(), ());
  }

  ScType m_type;
  std::string m_idtf;
  scs::Visibility m_visibility;
};

std::ostream & operator<< (std::ostream & out, TripleElement const & t)
{
  out << "{ m_type: " << *t.m_type << ", m_idtf: \"" << t.m_idtf << "\", m_visibility: " << int(t.m_visibility) << " }";
  return out;
}


struct TripleResult
{
  void Test(scs::Parser const & parser, scs::ParsedTriple const & triple) const
  {
    auto const & src = parser.GetParsedElement(triple.m_source);
    auto const & edge = parser.GetParsedElement(triple.m_edge);
    auto const & trg = parser.GetParsedElement(triple.m_target);

    try
    {
      m_source.Test(src);
      m_edge.Test(edge);
      m_target.Test(trg);
    }
    catch (utils::ScException const & ex)
    {
      std::cout << "Should be: " << std::endl
                << " m_source: " << m_source << ", " << std::endl
                << " m_edge: " << m_edge << ", " << std::endl
                << " m_target: " << m_target << std::endl;

      auto const elToString = [](scs::ParsedElement const & el) -> std::string
      {
        std::stringstream ss;

        ss << "m_type: " << *el.GetType() << ", m_idtf: \"" << el.GetIdtf() << "\"";

        return ss.str();
      };

      std::cout << "Parsed: " << std::endl
                << " m_source: " << elToString(src) << std::endl
                << " m_edge: " << elToString(edge) << std::endl
                << " m_target: " << elToString(trg) << std::endl;

      throw ex;
    }
  }

  TripleElement m_source;
  TripleElement m_edge;
  TripleElement m_target;
};


using ResultTriples = std::vector<TripleResult>;

struct TripleTester
{
  explicit TripleTester(scs::Parser const & parser) : m_parser(parser) {}

  void operator() (ResultTriples const & resultTriples)
  {
    auto const & triples = m_parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), resultTriples.size(), ());
    for (size_t i = 0; i < triples.size(); ++i)
      resultTriples[i].Test(m_parser, triples[i]);
  }

private:
  scs::Parser const & m_parser;
};

}

UNIT_TEST(scs_ElementHandle)
{
  scs::ElementHandle handle_err;
  SC_CHECK_NOT(handle_err.IsValid(), ());
  SC_CHECK_NOT(handle_err.IsLocal(), ());

  scs::ElementHandle handle_ok(1);
  SC_CHECK(handle_ok.IsValid(), ());
  SC_CHECK_NOT(handle_ok.IsLocal(), ());

  scs::ElementHandle handle_local(0, true);
  SC_CHECK(handle_local.IsValid(), ());
  SC_CHECK(handle_local.IsLocal(), ());
}

UNIT_TEST(scs_parser_error)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_parser_error");

  SUBTEST_START(error_1)
  {
    char const * data = "a -> b;;\nc ->";

    scs::Parser parser;

    SC_CHECK_NOT(parser.Parse(data), ());
    SC_LOG_WARNING(parser.GetParseError());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_parser_triple)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_parser_triple");

  SUBTEST_START(triple_1)
  {
    scs::Parser parser;
    char const * data = "a -> b;;";
    SC_CHECK(parser.Parse(data), ());

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConst, ());

    SC_CHECK_EQUAL(src.GetIdtf(), "a", ());
    SC_CHECK_EQUAL(trg.GetIdtf(), "b", ());

    SC_CHECK_EQUAL(src.GetVisibility(), scs::Visibility::System, ());
    SC_CHECK_EQUAL(trg.GetVisibility(), scs::Visibility::System, ());
  }
  SUBTEST_END()

  SUBTEST_START(reversed_1)
  {
    scs::Parser parser;
    char const * data = "a <- b;;";
    SC_CHECK(parser.Parse(data), ());

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetIdtf(), "b", ());
    SC_CHECK_EQUAL(trg.GetIdtf(), "a", ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());
  }
  SUBTEST_END()

  SUBTEST_START(sentences_1)
  {
    scs::Parser parser;
    char const * data = "a <- b;; r => x;;";
    SC_CHECK(parser.Parse(data), ());

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 2, ());

    {
      auto const & t = triples[0];
      auto const & source = parser.GetParsedElement(t.m_source);
      auto const & target = parser.GetParsedElement(t.m_target);

      SC_CHECK_EQUAL(source.GetIdtf(), "b", ());
      SC_CHECK_EQUAL(target.GetIdtf(), "a", ());
    }

    {
      SPLIT_TRIPLE(triples[1]);

      SC_CHECK_EQUAL(src.GetIdtf(), "r", ());
      SC_CHECK_EQUAL(trg.GetIdtf(), "x", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeDCommonConst, ());
    }
  }
  SUBTEST_END()
}

UNIT_TEST(scs_comments)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_comments");
  scs::Parser parser;

  char const * data =
      "//Level1\n"
      "a -> b;;/* example */\n"
      "c <> d;;";

  SC_CHECK(parser.Parse(data), ());

  auto const & triples = parser.GetParsedTriples();
  SC_CHECK_EQUAL(triples.size(), 2, ());

  {
    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetIdtf(), "a", ());
    SC_CHECK_EQUAL(trg.GetIdtf(), "b", ());

    SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConst, ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());
  }

  {
    SPLIT_TRIPLE(triples[1]);

    SC_CHECK_EQUAL(src.GetIdtf(), "c", ());
    SC_CHECK_EQUAL(trg.GetIdtf(), "d", ());

    SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConst, ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeUCommon, ());
  }
}

UNIT_TEST(scs_level_1)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_level_1");

  SUBTEST_START(simple)
  {
    char const * data = "sc_node#a | sc_edge#e1 | sc_node#b;;";
    scs::Parser parser;

    SC_CHECK(parser.Parse(data), ());

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());
    {
      SPLIT_TRIPLE(triples[0]);

      SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConst, ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeUCommonConst, ());
    }
  }
  SUBTEST_END()
}

UNIT_TEST(scs_const_var)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_const_var");

  char const * data = "_a _-> b;;";
  scs::Parser parser;

  SC_CHECK(parser.Parse(data), ());

  auto const & triples = parser.GetParsedTriples();
  SC_CHECK_EQUAL(triples.size(), 1, ());

  {
    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetType(), ScType::NodeVar, ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessVarPosPerm, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConst, ());

    SC_CHECK_EQUAL(src.GetIdtf(), "_a", ());
    SC_CHECK_EQUAL(trg.GetIdtf(), "b", ());
  }
}

UNIT_TEST(scs_level_2)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_level_2");

  SUBTEST_START(simple_1)
  {
    char const * data = "a -> (b <- c);;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), ());
    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "c" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::NodeConst, "b" }
             },
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 2, ());
    SC_CHECK_EQUAL(triples[0].m_edge, triples[1].m_target, ());
  }
  SUBTEST_END()

  SUBTEST_START(simple_2)
  {
    char const * data = "(a -> b) => c;;";
    scs::Parser parser;

    SC_CHECK(parser.Parse(data), ());
    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::NodeConst, "b" }
             },
             {
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::EdgeDCommonConst, "", scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 2, ());
    SC_CHECK_EQUAL(triples[0].m_edge, triples[1].m_source, ());
  }
  SUBTEST_END()

  SUBTEST_START(complex)
  {
    char const * data =
        "a <> (b -> c);;"
        "(c <- x) <- (b -> y);;";

    scs::Parser parser;
    SC_CHECK(parser.Parse(data), ());

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             },
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeUCommon, "", scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "x" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::NodeConst, "y" }
             },
             {
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, "", scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 5, ());

    SC_CHECK_EQUAL(triples[0].m_edge, triples[1].m_target, ());
    SC_CHECK_EQUAL(triples[2].m_edge, triples[4].m_target, ());
    SC_CHECK_EQUAL(triples[3].m_edge, triples[4].m_source, ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_3)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_level_3");

  SUBTEST_START(simple_1)
  {
    char const * data = "a -> c: _b:: d;;";
    scs::Parser parser;

    SC_CHECK(parser.Parse(data), ());
    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "d" }
             },
             {
               { ScType::NodeConst, "c" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeVar, "_b" },
               { ScType::EdgeAccessVarPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 3, ());

    SC_CHECK_EQUAL(triples[1].m_target, triples[0].m_edge, ());
    SC_CHECK_EQUAL(triples[2].m_target, triples[0].m_edge, ());
  }
  SUBTEST_END()

  SUBTEST_START(complex_1)
  {
    char const * data = "(a <- f: d) -> (c -> b: d);;";

    scs::Parser parser;
    SC_CHECK(parser.Parse(data), ());

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "d" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "a" }
             },
             {
               { ScType::NodeConst, "f" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "c" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "d" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 5, ());

    SC_CHECK_EQUAL(triples[1].m_target, triples[0].m_edge, ());
    SC_CHECK_EQUAL(triples[3].m_target, triples[2].m_edge, ());
    SC_CHECK_EQUAL(triples[4].m_source, triples[0].m_edge, ());
    SC_CHECK_EQUAL(triples[4].m_target, triples[2].m_edge, ());
  }
  SUBTEST_END()

  SUBTEST_START(complex_2)
  {
    char const * data = "a -> c: (d -> g: h);;";

    scs::Parser parser;
    SC_CHECK(parser.Parse(data), ());

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "d" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "h"}
             },
             {
               { ScType::NodeConst, "g" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "c" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 4, ());

    SC_CHECK_EQUAL(triples[0].m_edge, triples[1].m_target, ());
    SC_CHECK_EQUAL(triples[2].m_target, triples[0].m_edge, ());
    SC_CHECK_EQUAL(triples[3].m_target, triples[2].m_edge, ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_4)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_level_4");

  SUBTEST_START(simple_1)
  {
    char const * data = "a -> b: c; d;;";

    scs::Parser parser;
    SC_CHECK(parser.Parse(data), ());

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "d" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();

    SC_CHECK_EQUAL(triples.size(), 4, ());

    SC_CHECK_EQUAL(triples[1].m_target, triples[0].m_edge, ());
    SC_CHECK_EQUAL(triples[3].m_target, triples[2].m_edge, ());
  }
  SUBTEST_END()

  SUBTEST_START(simple_2)
  {
    char const * data = "a -> b: c; <- d: e: f;;";

    scs::Parser parser;
    SC_CHECK(parser.Parse(data), ());

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "a" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
             },
             {
               { ScType::NodeConst, "f" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "a" }
             },
             {
               { ScType::NodeConst, "d" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
             },
             {
               { ScType::NodeConst, "e" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
             }
           });

    auto const & triples = parser.GetParsedTriples();

    SC_CHECK_EQUAL(triples.size(), 5, ());

    SC_CHECK_EQUAL(triples[0].m_edge, triples[1].m_target, ());
    SC_CHECK_EQUAL(triples[2].m_edge, triples[3].m_target, ());
    SC_CHECK_EQUAL(triples[2].m_edge, triples[4].m_target, ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_5)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_level_5");

  SUBTEST_START(simple)
  {
    std::string const data = "set ~> attr:: item (* -/> subitem;; <= subitem2;; *);;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConst, "item" },
               { ScType::EdgeAccessConstFuzPerm, scs::Visibility::Local },
               { ScType::NodeConst, "subitem" }
             },
             {
               { ScType::NodeConst, "subitem2" },
               { ScType::EdgeDCommonConst, scs::Visibility::Local },
               { ScType::NodeConst, "item" }
             },
             {
               { ScType::NodeConst, "set" },
               { ScType::EdgeAccessConstPosTemp, scs::Visibility::Local },
               { ScType::NodeConst, "item" }
             },
             {
               { ScType::NodeConst, "attr" },
               { ScType::EdgeAccessVarPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosTemp, scs::Visibility::Local }
             }
           });

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 4, ());

    SC_CHECK_EQUAL(triples[3].m_target, triples[2].m_edge, ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_6_set)
{
  SUBTEST_START(base)
  {
    std::string const data = "@set = { a; b: c; d: e: f };;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    TripleTester tester(parser);
    tester({
             {
               { ScType::NodeConstTuple, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "a" }
             },
             {
               { ScType::NodeConstTuple, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "c" }
             },
             {
               { ScType::NodeConst, "b" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConstTuple, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::NodeConst, "f" }
             },
             {
               { ScType::NodeConst, "d" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             },
             {
               { ScType::NodeConst, "e" },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local },
               { ScType::EdgeAccessConstPosPerm, scs::Visibility::Local }
             }
           });
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_6_smoke)
{
  std::vector<std::string> data = {
    "z -> [**];;",
    "x -> [test*];;",
    "@a = [\\[* r-> b;; *\\]];;",
    "@alias = u;; @alias -> [* x -> [* y -> z;; *];; *];;",
    "y <= nrel_main_idtf: [y*];;",
    "a -> [* z -> [begin*];; *];;",
    "a -> [* b -> c;; *];;"
  };

  for (auto const & d : data)
  {
    scs::Parser parser;
    SC_CHECK(parser.Parse(d), (parser.GetParseError()));
  }
}

UNIT_TEST(scs_level_6_content)
{
  SUBTEST_START(constant)
  {
    std::string const data = "x -> [content_const];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetIdtf(), "x", ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::LinkConst, ());

    SC_CHECK_EQUAL(trg.GetValue(), "content_const", ());
  }
  SUBTEST_END()

  SUBTEST_START(empty)
  {
    std::string const data = "x -> [];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(src.GetIdtf(), "x", ());
    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::LinkConst, ());

    SC_CHECK_EQUAL(trg.GetValue(), "", ());
  }
  SUBTEST_END()

  SUBTEST_START(var)
  {
    std::string const data = "x -> _[var_content];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    auto const & trg = parser.GetParsedElement(triples[0].m_target);

    SC_CHECK_EQUAL(trg.GetType(), ScType::LinkVar, ());
    SC_CHECK_EQUAL(trg.GetValue(), "var_content", ());
  }
  SUBTEST_END()


  auto const testContent = [](std::string const & src, std::string const & check)
  {
    scs::Parser parser;

    SC_CHECK(parser.Parse(src), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    auto const & trg = parser.GetParsedElement(triples[0].m_target);

    SC_CHECK_EQUAL(trg.GetValue(), check, ());
  };

  SUBTEST_START(escape)
  {
    testContent("x -> _[\\[test\\]];;", "[test]");
  }
  SUBTEST_END()

  SUBTEST_START(escape_sequence)
  {
    testContent("x -> _[\\\\\\[test\\\\\\]];;", "\\[test\\]");
  }
  SUBTEST_END()

  SUBTEST_START(escape_error)
  {
    std::string const data = "x -> _[\\test]];;";

    scs::Parser parser;

    SC_CHECK(!parser.Parse(data), (parser.GetParseError()));
  }
  SUBTEST_END()

  SUBTEST_START(multiline)
  {
    std::string const data = "x -> _[line1\nline2];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    auto const & trg = parser.GetParsedElement(triples[0].m_target);

    SC_CHECK_EQUAL(trg.GetValue(), "line1\nline2", ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_level_6_contour)
{
  SUBTEST_START(empty)
  {
    std::string const data = "x -> [**];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    SPLIT_TRIPLE(triples[0]);

    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConstStruct, ());
  }
  SUBTEST_END()

  SUBTEST_START(base)
  {
    std::string const data = "x -|> [* y _=> z;; *];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 5, ());

    {
      SPLIT_TRIPLE(triples[0]);

      SC_CHECK_EQUAL(src.GetIdtf(), "y", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeDCommonVar, ());
      SC_CHECK_EQUAL(trg.GetIdtf(), "z", ());
    }

    for (size_t i = 1; i < 4; ++i)
    {
      auto const & edge = parser.GetParsedElement(triples[i].m_edge);
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());

      auto const & src = parser.GetParsedElement(triples[i].m_source);
      SC_CHECK_EQUAL(src.GetType(), ScType::NodeConstStruct, ());
    }

    {
      SPLIT_TRIPLE(triples.back());

      SC_CHECK_EQUAL(src.GetIdtf(), "x", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstNegPerm, ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConstStruct, ());
    }
  }
  SUBTEST_END()

  SUBTEST_START(recursive)
  {
    std::string const data = "x ~|> [* y _=> [* k ~> z;; *];; *];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 15, ());

    {
      SPLIT_TRIPLE(triples[0]);
      SC_CHECK_EQUAL(src.GetIdtf(), "k", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosTemp, ());
      SC_CHECK_EQUAL(trg.GetIdtf(), "z", ());
    }

    auto const checkStructEdges = [&triples, &parser](size_t idxStart, size_t idxEnd)
    {
      for (size_t i = idxStart; i < idxEnd; ++i)
      {
        auto const & edge = parser.GetParsedElement(triples[i].m_edge);
        SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstPosPerm, ());

        auto const & src = parser.GetParsedElement(triples[i].m_source);
        SC_CHECK_EQUAL(src.GetType(), ScType::NodeConstStruct, ());
      }
    };

    checkStructEdges(1, 4);

    {
      SPLIT_TRIPLE(triples[4]);

      SC_CHECK_EQUAL(src.GetIdtf(), "y", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeDCommonVar, ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConstStruct, ());
    }

    checkStructEdges(5, 14);

    {
      SPLIT_TRIPLE(triples[14]);

      SC_CHECK_EQUAL(src.GetIdtf(), "x", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstNegTemp, ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConstStruct, ());
    }
  }
  SUBTEST_END()

  SUBTEST_START(aliases)
  {
    std::string const data = "@alias = _[];; x -> [* @alias2 = y;; @alias _~> @alias2;;*];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 5, ());

    {
      SPLIT_TRIPLE(triples[0]);

      SC_CHECK_EQUAL(src.GetType(), ScType::LinkVar, ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessVarPosTemp, ());
      SC_CHECK_EQUAL(trg.GetIdtf(), "y", ());
    }
  }
  SUBTEST_END()

  SUBTEST_START(content)
  {
    std::string const data = "x -> [* y _=> [test*];; *];;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 5, ());

    {
      SPLIT_TRIPLE(triples[0]);

      SC_CHECK_EQUAL(src.GetIdtf(), "y", ());
      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeDCommonVar, ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::LinkConst, ());
      SC_CHECK_EQUAL(trg.GetValue(), "test*", ());
    }
  }
  SUBTEST_END()
}

UNIT_TEST(scs_types)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_types");

  SUBTEST_START(nodes)
  {
    char const * data = "a -> b;;"
                        "sc_node_tuple -> a;;"
                        "sc_node_struct -> b;;"
                        "sc_node_role_relation -> c;;"
                        "c -> _d;;"
                        "sc_node_norole_relation -> _d;;"
                        "sc_node_class -> e;;"
                        "e -> f;;"
                        "sc_node_abstract -> f;;"
                        "f -> g;;"
                        "sc_node_material -> g;;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 4, ());
    {
      auto const CheckSourceNode = [&triples, &parser](size_t index, ScType type)
      {
        SC_CHECK(index < triples.size(), ());
        return (parser.GetParsedElement(triples[index].m_source).GetType() == type);
      };

      auto const CheckTargetNode = [&triples, &parser](size_t index, ScType type)
      {
        SC_CHECK(index < triples.size(), ());
        return (parser.GetParsedElement(triples[index].m_target).GetType() == type);
      };

      SC_CHECK(CheckSourceNode(0, ScType::NodeConstTuple), ());
      SC_CHECK(CheckTargetNode(0, ScType::NodeConstStruct), ());
      SC_CHECK(CheckSourceNode(1, ScType::NodeConstRole), ());
      SC_CHECK(CheckTargetNode(1, ScType::NodeVarNoRole), ());
      SC_CHECK(CheckSourceNode(2, ScType::NodeConstClass), ());
      SC_CHECK(CheckTargetNode(2, ScType::NodeConstAbstract), ());
      SC_CHECK(CheckSourceNode(3, ScType::NodeConstAbstract), ());
      SC_CHECK(CheckTargetNode(3, ScType::NodeConstMaterial), ());
    }
  }
  SUBTEST_END()

  SUBTEST_START(links)
  {
    std::string const data =
        "a -> \"file://data.txt\";;"
        "b -> [x];;"
        "c -> _[];;"
        "d -> [];;";
    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();

    SC_CHECK_EQUAL(triples.size(), 4, ());

    SC_CHECK_EQUAL(parser.GetParsedElement(triples[0].m_target).GetType(), ScType::Link, ());
    SC_CHECK_EQUAL(parser.GetParsedElement(triples[1].m_target).GetType(), ScType::LinkConst, ());
    SC_CHECK_EQUAL(parser.GetParsedElement(triples[2].m_target).GetType(), ScType::LinkVar, ());
    SC_CHECK_EQUAL(parser.GetParsedElement(triples[3].m_target).GetType(), ScType::LinkConst, ());
  }
  SUBTEST_END()

  SUBTEST_START(backward_compatibility)
  {
    std::string const data = "a <- c;; a <- sc_node_not_relation;; b <- c;; b <- sc_node_not_binary_tuple;;";
    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 2, ());

    SC_CHECK_EQUAL(parser.GetParsedElement(triples[0].m_target).GetType(), ScType::NodeConstClass, ());
    SC_CHECK_EQUAL(parser.GetParsedElement(triples[1].m_target).GetType(), ScType::NodeConstTuple, ());
  }
  SUBTEST_END()

  SUBTEST_START(edges)
  {
    std::string const data = "x"
                             "> _y; <> y4; ..> y5;"
                             "<=> y7; _<=> y8; => y9; _=> y11;"
                             "-> y2; _-> y13; -|> y15; _-|> y17; -/> y19; _-/> y21;"
                             " ~> y23; _~> y25; ~|> y27; _~|> y29; ~/> y31; _~/> y33;;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 19, ());
    {
      auto const CheckEdgeType = [&triples, &parser](size_t index, ScType type) -> bool
      {
        SC_CHECK(index < triples.size(), ("Invalid index, check test logic please"));
        return (parser.GetParsedElement(triples[index].m_edge).GetType() == type);
      };

      SC_CHECK(CheckEdgeType(0, ScType::EdgeDCommon), ());
      SC_CHECK(CheckEdgeType(1, ScType::EdgeUCommon), ());
      SC_CHECK(CheckEdgeType(2, ScType::EdgeAccess), ());

      SC_CHECK(CheckEdgeType(3, ScType::EdgeUCommonConst), ());
      SC_CHECK(CheckEdgeType(4, ScType::EdgeUCommonVar), ());
      SC_CHECK(CheckEdgeType(5, ScType::EdgeDCommonConst), ());
      SC_CHECK(CheckEdgeType(6, ScType::EdgeDCommonVar), ());

      SC_CHECK(CheckEdgeType(7, ScType::EdgeAccessConstPosPerm), ());
      SC_CHECK(CheckEdgeType(8, ScType::EdgeAccessVarPosPerm), ());
      SC_CHECK(CheckEdgeType(9, ScType::EdgeAccessConstNegPerm), ());
      SC_CHECK(CheckEdgeType(10, ScType::EdgeAccessVarNegPerm), ());
      SC_CHECK(CheckEdgeType(11, ScType::EdgeAccessConstFuzPerm), ());
      SC_CHECK(CheckEdgeType(12, ScType::EdgeAccessVarFuzPerm), ());

      SC_CHECK(CheckEdgeType(13, ScType::EdgeAccessConstPosTemp), ());
      SC_CHECK(CheckEdgeType(14, ScType::EdgeAccessVarPosTemp), ());
      SC_CHECK(CheckEdgeType(15, ScType::EdgeAccessConstNegTemp), ());
      SC_CHECK(CheckEdgeType(16, ScType::EdgeAccessVarNegTemp), ());
      SC_CHECK(CheckEdgeType(17, ScType::EdgeAccessConstFuzTemp), ());
      SC_CHECK(CheckEdgeType(18, ScType::EdgeAccessVarFuzTemp), ());
    }
  }
  SUBTEST_END()

  SUBTEST_START(type_error)
  {
    std::string const data = "a <- sc_node_abstract;; a <- sc_node_role_relation;;";

    scs::Parser parser;
    SC_CHECK_NOT(parser.Parse(data), ());
  }
  SUBTEST_END()
}

UNIT_TEST(scs_aliases)
{
  ScMemoryContext ctx(sc_access_lvl_make_min, "scs_aliases");

  SUBTEST_START(simple_assign)
  {
    std::string const data = "@alias = [];; x ~> @alias;;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    auto const & t = triples.front();
    SC_CHECK(parser.GetParsedElement(t.m_source).GetType().IsNode(), ());
    SC_CHECK_EQUAL(parser.GetParsedElement(t.m_edge).GetType(), ScType::EdgeAccessConstPosTemp, ());
    SC_CHECK(parser.GetParsedElement(t.m_target).GetType().IsLink(), ());
  }
  SUBTEST_END()

  SUBTEST_START(no_assign)
  {
    std::string const data = "x -> @y;;";

    scs::Parser parser;

    SC_CHECK(!parser.Parse(data), (parser.GetParseError()));
  }
  SUBTEST_END()

  SUBTEST_START(recursive_assigns)
  {
    std::string const data = "@alias1 = x;; @alias1 <- sc_node_tuple;; @alias2 = @alias1;; _y -|> x;;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 1, ());

    auto const t = triples.front();
    auto const & src = parser.GetParsedElement(t.m_source);
    auto const & edge = parser.GetParsedElement(t.m_edge);
    auto const & trg = parser.GetParsedElement(t.m_target);

    SC_CHECK_EQUAL(src.GetIdtf(), "_y", ());
    SC_CHECK(src.GetType().IsNode(), ());
    SC_CHECK(src.GetType().IsVar(), ());

    SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessConstNegPerm, ());

    SC_CHECK_EQUAL(trg.GetIdtf(), "x", ());
    SC_CHECK_EQUAL(trg.GetType(), ScType::NodeConstTuple, ());
  }
  SUBTEST_END()

  SUBTEST_START(alias_reassign)
  {
    std::string const data = "@alias = _x;; _x <- sc_node_struct;; y _~/> @alias;; @alias = _[];; z _~> @alias;;";

    scs::Parser parser;

    SC_CHECK(parser.Parse(data), (parser.GetParseError()));

    auto const & triples = parser.GetParsedTriples();
    SC_CHECK_EQUAL(triples.size(), 2, ());

    {
      auto const & t = triples[0];

      auto const & src = parser.GetParsedElement(t.m_source);
      auto const & edge = parser.GetParsedElement(t.m_edge);
      auto const & trg = parser.GetParsedElement(t.m_target);

      SC_CHECK_EQUAL(src.GetIdtf(), "y", ());
      SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());

      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessVarFuzTemp, ());

      SC_CHECK_EQUAL(trg.GetIdtf(), "_x", ());
      SC_CHECK_EQUAL(trg.GetType(), ScType::NodeVarStruct, ());
    }

    {
      auto const & t = triples[1];

      auto const & src = parser.GetParsedElement(t.m_source);
      auto const & edge = parser.GetParsedElement(t.m_edge);
      auto const & trg = parser.GetParsedElement(t.m_target);

      SC_CHECK_EQUAL(src.GetIdtf(), "z", ());
      SC_CHECK_EQUAL(src.GetType(), ScType::NodeConst, ());

      SC_CHECK_EQUAL(edge.GetType(), ScType::EdgeAccessVarPosTemp, ());

      SC_CHECK_EQUAL(trg.GetType(), ScType::LinkVar, ());
    }
  }
  SUBTEST_END()
}
