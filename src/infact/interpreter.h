// Copyright 2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   * Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above
//     copyright notice, this list of conditions and the following disclaimer
//     in the documentation and/or other materials provided with the
//     distribution.
//   * Neither the name of Google Inc. nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// -----------------------------------------------------------------------------
//
//
/// \file
/// Provides an interpreter for assigning primitives and Factory-constructible
/// objects to named variables, as well as vectors thereof.
/// \author dbikel@google.com (Dan Bikel)

#ifndef INFACT_INTERPRETER_H_
#define INFACT_INTERPRETER_H_

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "environment-impl.h"

namespace infact {

using std::ifstream;
using std::istream;
using std::unique_ptr;

/// An interface for classes that can build istreams for named files.
class IStreamBuilder {
 public:
  virtual ~IStreamBuilder() = default;

  virtual unique_ptr<istream> Build(
      const string &filename,
      std::ios_base::openmode mode = std::ios_base::in) const = 0;
};

/// The default implementation for the IStreamBuilder interface, returning
/// std::ifstream instances.
class DefaultIStreamBuilder : public IStreamBuilder {
 public:
  ~DefaultIStreamBuilder() override = default;

  unique_ptr<istream> Build(const string &filename,
                            std::ios_base::openmode mode = std::ios_base::in)
      const override;
};

class EnvironmentImpl;

/// Provides an interpreter for assigning primitives and
/// Factory-constructible objects to named variables, as well as
/// vectors thereof.  The interpreter maintains an internal
/// environment whereby previously defined variables may be used in
/// the definition of subsequent ones.  The syntax of this language
/// extends the syntax of the \link infact::Factory Factory \endlink
/// class, described in the documentation of the \link
/// infact::Factory::CreateOrDie Factory::CreateOrDie \endlink
/// method.
///
/// Statements in this language look like the following:
/// \code
/// // This is a comment.
/// bool b = true;    // assigns the value true to the boolean variable "b"
/// int f = 1;        // assigns the int value 1 to the variable "f"
/// double g = 2.4;   // assigns the double value 2.4 to the variable "g"
/// string n = "foo"  // assigns the string value "foo" to the variable "n"
/// bool[] b_vec = {true, false, true};  // assigns a vector of bool to "b_vec"
///
/// // Constructs an object of abstract type Model and assigns it to "m1"
/// Model m1 = PerceptronModel(name("foo"));
///
/// // Constructs an object of type Model and assigns it to "m2", crucially
/// // using the previously defined string variable "n" as the value for
/// // the PerceptronModel's name parameter.
/// Model m2 = PerceptronModel(name(n));
///
/// // Constructs a vector of Model objects and assigns it to "m_vec".
/// Model[] m_vec = {m2, PerceptronModel(name("bar"))};
/// \endcode
///
/// Additionally, the interpreter can do type inference, so all the type
/// specifiers in the previous examples are optional.  For example, one may
/// write the following statements:
/// \code
/// b = true;  // assigns the value true to the boolean variable "b"
///
/// // Constructs an object of abstract type Model and assigns it to "m1"
/// m1 = PerceptronModel(name("foo"));
///
/// // Constructs a vector of Model objects and assigns it to "m_vec".
/// m_vec = {m1, PerceptronModel(name("bar"))};
/// \endcode
///
/// Here's an example of using the interpreter after it has read the
/// three statements from the previous example from a file called
/// <tt>"example.infact"</tt>:
/// \code
/// #include "interpreter.h"
/// // ...
/// Interpreter i;
/// i.Eval("example.infact");
/// shared_ptr<Model> model;
/// vector<shared_ptr<Model> > model_vector;
/// bool b;
/// // The next statement only assigns a value to b if a variable "b"
/// // exists in the interpreter's environment.
/// i.Get("b", &b);
/// i.Get("m1", &model);
/// i.Get("m_vec", &model_vector);
/// \endcode
///
/// More formally, a statement in this language must conform to the
/// following grammar, defined on top of the BNF syntax in the
/// documentation of the \link infact::Factory::CreateOrDie
/// Factory::CreateOrDie \endlink method:
/// <table border=0>
/// <tr>
///   <td><tt>\<statement_list\></tt></td>
///   <td><tt>::=</tt></td>
///   <td><tt>[ \<import_or_statement\> ]*</tt></td>
/// </tr>
/// <tr>
///   <td><tt>\<import_or_statement\></tt></td>
///   <td><tt>::=</tt></td>
///   <td><tt>[ \<import\> | \<statement\> ]</tt></td>
/// </tr>
/// <tr>
///   <td><tt>\<import\></tt></td>
///   <td><tt>::=</tt></td>
///   <td><tt>'import' \<string_literal\> ';' </tt></td>
/// </tr>
/// <tr>
///   <td><tt>\<statement\></tt></td>
///   <td><tt>::=</tt></td>
///   <td>
///     <tt>[ \<type_specifier\> ] \<variable_name\> '=' \<value\> ';' </tt>
///   </td>
/// </tr>
/// <tr>
///   <td valign=top><tt>\<type_specifier\></tt></td>
///   <td valign=top><tt>::=</tt></td>
///   <td valign=top>
///     <table border="0">
///       <tr><td><tt>"bool" | "int" | "string" | "double" | "bool[]" | "int[]"
///                   "string[]" | "double[]" | T | T[]</tt></td></tr>
///       <tr><td>where <tt>T</tt> is any \link infact::Factory
///               Factory\endlink-constructible type.</td></tr>
///     </table>
///   </td>
/// </tr>
/// <tr>
///   <td><tt>\<variable_name\></tt></td>
///   <td><tt>::=</tt></td>
///   <td>any valid C++ identifier</td>
/// </tr>
/// <tr>
///   <td valign=top><tt>\<value\></tt></td>
///   <td valign=top><tt>::=</tt></td>
///   <td valign=top><tt>\<literal\> | '{' \<literal_list\> '}' |<br>
///                      \<spec_or_null\> | '{' \<spec_list\> '}'</tt>
///   </td>
/// </tr>
/// </table>
///
/// The above grammar doesn&rsquo;t contain rules covering C++ style
/// line comments, but they have the same behavior in this language as
/// they do in C++, i.e., everything after the <tt>//</tt> to the end
/// of the current line is treated as a comment and ignored.  There
/// are no C-style comments in this language.
///
/// There are very simple rules for finding the files specified by
/// imports: if a path is absolute, the Interpreter tries the path as-is.
/// If it is relative, the Interpreter tries the path relative to the
/// directory of the file with the import statement.  If that does not
/// succeed, the system simply tries the relative path as-is.
///
/// An imported file is evaluated <i>in situ</i>, meaning it is
/// evaluated in the current Environment; it is just as though you
/// pasted the contents of the imported file right at the import
/// statement.  As such, an imported file can refer to variables that
/// might be set prior to the import being evaluated.  As a corollary,
/// if an imported file uses the name of an unset variable, it is an
/// error.
class Interpreter {
 public:
  /// Constructs a new instance with the specified debug level.  The
  /// wrapped \link infact::Environment Environment \endlink will
  /// also have the specified debug level.
  Interpreter(int debug = 0) :
      Interpreter(unique_ptr<IStreamBuilder>(new DefaultIStreamBuilder()),
                  debug) { }

  /// Constructs a new instance with the specified IStreamBuilder and
  /// debug level.  The wrapped \link infact::Environment Environment
  /// \endlink will also have the specified debug level.
  Interpreter(unique_ptr<IStreamBuilder> istream_builder, int debug = 0) :
      env_(new EnvironmentImpl(debug)),
      istream_builder_(std::move(istream_builder)),
      debug_(debug) { }

  /// Destroys this interpreter.
  virtual ~Interpreter() = default;

  /// Sets the IStreamBuilder object, to be owned by this object.
  void SetIStreamBuilder(unique_ptr<IStreamBuilder> istream_builder) {
    istream_builder_ = std::move(istream_builder);
  }

  /// Evaluates the statements in the specified text file.
  void Eval(const string &filename);

  /// Evaluates the statements in the specified string.
  void EvalString(const string& input) {
    StreamTokenizer st(input);
    Eval(st);
  }

  /// Evaluates the statements in the specified stream.
  void Eval(istream &is) {
    StreamTokenizer st(is);
    Eval(st);
  }

  void PrintEnv(ostream &os) const {
    env_->Print(os);
  }

  void PrintFactories(ostream &os) const {
    env_->PrintFactories(os);
  }

  /// The base case for the variadic template method GetMany defined below.
  bool GetMany() { return true; }

  /// Retrieves values for many variables, specified as a sequence of
  /// pairs of arguments.  It is an error to invoke this method with
  /// an odd number of arguments.
  ///
  /// Example:
  /// \code
  /// Interpreter i;
  ///
  /// // Evaluate a short string defining two variables.
  /// i.EvalString("i = 6; f = \"foo\";");
  /// int my_int;
  /// string my_string;
  /// i.GetMany("i", &my_int, "f", &my_string);
  /// \endcode
  ///
  /// \param[in]   varname the name of a variable to be retrieved
  /// \param[out]  var a pointer to a value for the variable named by \c varname
  /// \param       rest the remaining arguments
  ///
  /// \tparam T    the type of value to be assigned; the value of
  ///              \c varname, if it has a binding in the Environment,
  ///              must be assignable to \c T
  /// \tparam Args a variadic list of arguments, where each successive
  ///              pair of arguments must be a variable name of type
  ///              <code>const string &</code> and a pointer to an
  ///              arbitrary type T to
  ///              which the value of the variable may be assigned
  ///
  /// \return whether this method has succeeded in assigning values to
  ///         all named variables
  template<typename T, typename... Args>
  bool GetMany(const string &varname, T *var,
               Args &&...rest) {
    bool success = Get(varname, var);
    if (!success) {
      std::ostringstream err_ss;
      err_ss << "infact::Interpreter: no variable with name '"
             << varname << "'";
      Error(err_ss.str());
      return false;
    }
    return GetMany(std::forward<Args>(rest)...);  // compile-time recursion ftw
  }

  /// Retrieves the value of the specified variable.  It is an error
  /// if the type of the specified pointer to a value object is different
  /// from the specified variable in this interpreter&rsquo;s environment.
  ///
  /// \tparam the type of value object being set by this method
  ///
  /// \param varname the name of the variable for which to retrieve the value
  /// \param value   a pointer to the object whose value to be set by this
  ///                method
  template<typename T>
  bool Get(const string &varname, T *value) const {
    return env_->Get(varname, value);
  }

  /// Returns a pointer to the environment of this interpreter.
  /// Crucially, this method returns a pointer to the Environment
  /// implementation class, \link infact::EnvironmentImpl
  /// EnvironmentImpl\endlink, so that its templated \link
  /// infact::EnvironmentImpl::Get EnvironmentImpl::Get \endlink
  /// method may be invoked.
  EnvironmentImpl *env() { return env_.get(); }

 private:
  /// Returns whether \c filename is an absolute path.
  bool IsAbsolute(const string &filename) const;

  /// Returns wheterh \c filename is a file that exists and can be read.
  bool CanReadFile(const string &filename) const;

  /// Returns whether either f1 or f2 can be read, trying them in that
  /// order.  If one of them can be read (according to the result of
  /// <code>CanReadFile(const string &)</code>), then the resulting
  /// name is output to filename.
  ///
  /// \param[in]  f1 the first filename to check for readability
  /// \param[in]  f2 the second filename to check for readability
  /// \param[out] filename the name of the readable file, either \c f1 or \c f2
  ///
  /// \return whether either \c f1 or \c f2 are readable and the
  ///         output parameter \c filename has been set
  bool CanReadFile(const string &f1, const string &f2, string *filename) const;

  /// Returns whether \c filename introduces a cycle in the specified
  /// stack of \c filenames.  Used for determining file import cycles.
  bool HasCycle(const string &filename, const vector<string> &filenames) const;

  /// Evaluates the expressions contained the named file.
  void EvalFile(const string &filename);

  /// Evalutes the expressions contained in the specified token stream.
  void Eval(StreamTokenizer &st);

  /// Reads and evaluates an import statement.
  void Import(StreamTokenizer &st);

  /// Returns the name of the current file being interpreted, or the empty
  /// string if there is no such file.
  string curr_filename() const {
    return filenames_.size() > 0 ? filenames_.back() : "";
  }

  /// Returns a human-readable form of the stack of filenames, useful for
  /// error messages and/or debugging.
  ///
  /// \param st  the current StreamTokenizer
  /// \param pos the stream position of the current file (at the top
  ///            of the statck)
  string filestack(StreamTokenizer &st, size_t pos) const;

  void WrongTokenError(StreamTokenizer &st,
                       size_t pos,
                       const string &expected,
                       const string &found,
                       StreamTokenizer::TokenType found_type) const;

  void WrongTokenTypeError(StreamTokenizer &st,
                           size_t pos,
                           StreamTokenizer::TokenType expected,
                           StreamTokenizer::TokenType found,
                           const string &token) const;

  void WrongTokenTypeError(StreamTokenizer &st,
                           size_t pos,
                           const string &expected_type,
                           const string &found_type,
                           const string &token) const;

  /// The environment of this interpreter.
  unique_ptr<EnvironmentImpl> env_;

  /// The stack of files being interpreted, where the back of the
  /// vector is the top of the stack. The string on the top of the
  /// stack is the name of the current file being interpreted, or the
  /// empty string if there is no file associated with the stream
  /// being interpreted.
  vector<string> filenames_;

  // The object for building new istream objects from named files.
  unique_ptr<IStreamBuilder> istream_builder_;

  // The debug level of this interpreter.
  int debug_;
};

}  // namespace infact

#endif
