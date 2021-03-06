/* lorina: C++ parsing library
 * Copyright (C) 2018  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file blif.hpp
  \brief Implements blif parser
  \author Heinz Riener
*/

#pragma once

#include <lorina/common.hpp>
#include <lorina/diagnostics.hpp>
#include <lorina/detail/utils.hpp>
#include <regex>
#include <iostream>

namespace lorina
{

/*! \brief A reader visitor for the BLIF format.
 *
 * Callbacks for the BLIF (Berkeley Logic Interchange Format) format.
 */
class blif_reader
{
public:
  /*! \brief Type of the output cover as truth table. */
  using output_cover_t = std::vector<std::pair<std::string, std::string>>;
  /*! Latch input values */
  enum latch_init_value
  {
    ZERO = 0 /*!< Initialized with 0 */
  , ONE /*!< Initialized with 1 */
  , NONDETERMINISTIC /*!< Not initialized (non-deterministic value) */
  , UNKNOWN
  };

  enum latch_type
  {
    FALLING = 0
  , RISING
  , ACTIVE_HIGH
  , ACTIVE_LOW
  , ASYNC
  , NONE
  };

public:
  /*! \brief Callback method for parsed model.
   *
   * \param model_name Name of the model
   */
  virtual void on_model( const std::string& model_name ) const
  {
    (void)model_name;
  }

  /*! \brief Callback method for parsed input.
   *
   * \param name Input name
   */
  virtual void on_input( const std::string& name ) const
  {
    (void)name;
  }

  /*! \brief Callback method for parsed output.
   *
   * \param name Output name
   */
  virtual void on_output( const std::string& name ) const
  {
    (void)name;
  }

  virtual void on_latch( const std::string& input, const std::string& output, const latch_type& type, const std::string& control, const latch_init_value& reset ) const
  {
    (void)input;
    (void)output;
    (void)type;
    (void)control;
    (void)reset;
  }

  virtual void on_latch( const std::string& input, const std::string& output, const latch_init_value& reset ) const
  {
    (void)input;
    (void)output;
    (void)reset;
  }

  /*! \brief Callback method for parsed gate.
   *
   * \param inputs A list of input names
   * \param output Name of output of the gate
   * \param cover N-input, 1-output PLA description of the logic function corresponding to the logic gate
   */
  virtual void on_gate( const std::vector<std::string>& inputs, const std::string& output, const output_cover_t& cover ) const
  {
    (void)inputs;
    (void)output;
    (void)cover;
  }

  /*! \brief Callback method for parsed end.
   *
   */
  virtual void on_end() const {}

  /*! \brief Callback method for parsed comment.
   *
   * \param comment Comment
   */
  virtual void on_comment( const std::string& comment ) const
  {
    (void)comment;
  }
}; /* blif_reader */

/*! \brief A BLIF reader for prettyprinting BLIF.
 *
 * Callbacks for prettyprinting of BLIF.
 *
 */
class blif_pretty_printer : public blif_reader
{
public:
  /*! \brief Constructor of the BLIF pretty printer.
   *
   * \param os Output stream
   */
  blif_pretty_printer( std::ostream& os = std::cout )
      : _os( os )
  {
  }

  virtual void on_model( const std::string& model_name ) const
  {
    _os << ".model " << model_name << std::endl;
  }

  virtual void on_input( const std::string& name ) const
  {
    if ( first_input ) _os << std::endl << ".inputs ";
    _os << name << " ";
    first_input = false;
  }

  virtual void on_output( const std::string& name ) const
  {
    if ( first_output ) _os << std::endl << ".output ";
    _os << name << " ";
    first_output = false;
  }

  virtual void on_latch( const std::string& input, const std::string& output, const latch_type& type, const std::string& control, const latch_init_value& init ) const
  {
    _os << std::endl << fmt::format( ".latch {0} {1} {2} {3} {4}", input, output, type, control, init ) << std::endl;
  }

  virtual void on_latch( const std::string& input, const std::string& output, const latch_init_value& init ) const
  {
    _os << std::endl << fmt::format( ".latch {0} {1} {2}", input, output, init ) << std::endl;
  }

  virtual void on_gate( const std::vector<std::string>& inputs, const std::string& output, const output_cover_t& cover ) const
  {
    _os << std::endl << fmt::format( ".names {0} {1}", detail::join( inputs, "," ), output ) << std::endl;
    for ( const auto& c : cover )
    {
      _os << c.first << ' ' << c.second << std::endl;
    }
  }

  virtual void on_end() const
  {
    _os << std::endl << ".end" << std::endl;
  }

  virtual void on_comment( const std::string& comment ) const
  {
    _os << std::endl << "# " << comment << std::endl;
  }

  mutable bool first_input = true; /*!< Predicate that is true until the first input was parsed */
  mutable bool first_output = true; /*!< Predicate that is true until the first output was parsed */
  std::ostream& _os; /*!< Output stream */
}; /* blif_pretty_printer */

namespace blif_regex
{
static std::regex model( R"(.model\s+(.*))" );
static std::regex names( R"(.names\s+(.*))" );
static std::regex line_of_truthtable( R"(([01\-]*)\s*([01\-]))" );
static std::regex end( R"(.end)" );
} // namespace blif_regex

/*! \brief Reader function for the BLIF format.
 *
 * Reads BLIF format from a stream and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param in Input stream
 * \param reader A BLIF reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing have been successful, or parse error if parsing have failed
 */
inline return_code read_blif( std::istream& in, const blif_reader& reader, diagnostic_engine* diag = nullptr )
{
  return_code result = return_code::success;

  const auto dispatch_function = [&]( std::vector<std::string> inputs, std::string output, std::vector<std::pair<std::string, std::string>> tt )
    {
      reader.on_gate( inputs, output, tt );
    };

  detail::call_in_topological_order<std::vector<std::string>, std::string, std::vector<std::pair<std::string, std::string>>> on_action( dispatch_function );

  std::smatch m;
  detail::foreach_line_in_file_escape( in, [&]( std::string line ) {
    redo:
      /* empty line or comment */
      if ( line.empty() || line[0] == '#' )
        return true;

      /* names */
      if ( std::regex_search( line, m, blif_regex::names ) )
      {
        auto args = detail::split( detail::trim_copy( m[1] ), " " );
        const auto output = *( args.rbegin() );
        args.pop_back();

        std::vector<std::pair<std::string, std::string>> tt;
        std::string copy_line;
        detail::foreach_line_in_file_escape( in, [&]( const std::string& line ) {
          copy_line = line;

          /* terminate when the next '.' declaration starts */
          if ( line[0] == '.' )
            return false;

          /* empty line or comment */
          if ( line.empty() || line[0] == '#' )
            return true;

          if ( std::regex_search( line, m, blif_regex::line_of_truthtable ) )
          {
            tt.push_back( std::make_pair( detail::trim_copy( m[1] ), detail::trim_copy( m[2] ) ) );
            return true;
          }

          return false;
        } );

        on_action.call_deferred( args, output, args, output, tt );

        if ( in.eof() )
        {
          return true;
        }
        else
        {
          /* restart parsing with the line of the subparser */
          line = copy_line;
          goto redo;
        }
      }

      /* .model <string> */
      if ( std::regex_search( line, m, blif_regex::model ) )
      {
        reader.on_model( detail::trim_copy( m[1] ) );
        return true;
      }

      /* .inputs <list of whitespace separated strings> */
      if ( detail::starts_with( line, ".inputs" ) )
      {
        std::string const input_declaration = line.substr( 7 );
        for ( const auto& input : detail::split( detail::trim_copy( input_declaration ), " " ) )
        {
          auto const s = detail::trim_copy( input );
          on_action.declare_known( s );
          
          reader.on_input( s );
        }
        return true;
      }

      /* .inputs <list of whitespace separated strings> */
      if ( detail::starts_with( line, ".latch" ) )
      {
        std::string const latch_declaration = line.substr( 6 );
        std::string elements;
        std::vector<std::string> latch_elements = detail::split( detail::trim_copy( latch_declaration ), " " );
        if ( latch_elements.size() == 3 )
        {
          std::string input = latch_elements[0];
          std::string output = latch_elements[1];
          std::string latch_init = latch_elements[2];

          blif_reader::latch_init_value reset;
          if ( latch_init == "0" )
          {
            reset = blif_reader::latch_init_value::ZERO;
          }
          else if ( latch_init == "1" )
          {
            reset = blif_reader::latch_init_value::ONE;
          }
          else if ( latch_init == "2" )
          {
            reset = blif_reader::latch_init_value::NONDETERMINISTIC;
          }
          else{
            reset = blif_reader::latch_init_value::UNKNOWN;
          }
          // std::cout << "latch_init = " << latch_init << "\n";
          on_action.declare_known( output );
          reader.on_latch(input, output, reset);

          return true;
        }
        else
        {
          if ( latch_elements.size() == 5 )
          {
            std::string input = latch_elements[0];
            std::string output = latch_elements[1];
            std::string type = latch_elements[2];
            std::string control = latch_elements[3];
            std::string latch_init = latch_elements[4];

            blif_reader::latch_type l_type;
            if ( type == "fe" )
            {
              l_type = blif_reader::latch_type::FALLING;
            }
            else if ( type == "re" )
            {
              l_type = blif_reader::latch_type::RISING;
            }
            else if ( type == "ah" )
            {
              l_type = blif_reader::latch_type::ACTIVE_HIGH;
            }
            else if ( type == "al" )
            {
              l_type = blif_reader::latch_type::ACTIVE_LOW;
            }
            else 
            {
              l_type = blif_reader::latch_type::ASYNC;
            }

            blif_reader::latch_init_value reset;
            if ( latch_init == "0" )
            {
              reset = blif_reader::latch_init_value::ZERO;
            }
            else if ( latch_init == "1" )
            {
              reset = blif_reader::latch_init_value::ONE;
            }
            else if ( latch_init == "2" )
            {
              reset = blif_reader::latch_init_value::NONDETERMINISTIC;
            }
            else{
              reset = blif_reader::latch_init_value::UNKNOWN;
            }

            on_action.declare_known( output );
            reader.on_latch(input, output, l_type, control, reset);
          }
          else if ( diag )
          {
            diag->report( diagnostic_level::error,
                      fmt::format( "latch format not supported `{0}`", line ) );

            result = return_code::parse_error;
          }
          
          return true;
        }
      }

      /* .outputs <list of whitespace separated strings> */
      if ( detail::starts_with( line, ".outputs" ) )
      {
        std::string const output_declaration = line.substr( 8 );
        for ( const auto& output : detail::split( detail::trim_copy( output_declaration ), " " ) )
        {
          auto const s = detail::trim_copy( output );
          reader.on_output( s );
        }
        return true;
      }

      /* .end */
      if ( std::regex_search( line, m, blif_regex::end ) )
      {
        reader.on_end();
        return true;
      }

      if ( diag )
      {
        diag->report( diagnostic_level::error,
                      fmt::format( "cannot parse line `{0}`", line ) );
      }

      result = return_code::parse_error;
      return true;
    } );

  /* check dangling objects */
  auto const& deps = on_action.unresolved_dependencies();
  if ( deps.size() > 0 )
    result = return_code::parse_error;

  for ( const auto& r : deps )
  {
    if ( diag )
    {
      diag->report( diagnostic_level::warning,
                    fmt::format( "unresolved dependencies: `{0}` requires `{1}`",  r.first, r.second ) );
    }
  }

  return result;
}

/*! \brief Reader function for BLIF format.
 *
 * Reads binary BLIF format from a file and invokes a callback
 * method for each parsed primitive and each detected parse error.
 *
 * \param filename Name of the file
 * \param reader A BLIF reader with callback methods invoked for parsed primitives
 * \param diag An optional diagnostic engine with callback methods for parse errors
 * \return Success if parsing have been successful, or parse error if parsing have failed
 */
inline return_code read_blif( const std::string& filename, const blif_reader& reader, diagnostic_engine* diag = nullptr )
{
  std::ifstream in( detail::word_exp_filename( filename ), std::ifstream::in );
  return read_blif( in, reader, diag );
}

} // namespace lorina
