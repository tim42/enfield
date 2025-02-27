//
// file : name.hpp
// in : file:///home/tim/projects/enfield/enfield/component/name.hpp
//
// created by : Timothée Feuillet
// date: Sat Mar 04 2017 14:55:52 GMT-0500 (EST)
//
//
// Copyright (c) 2017 Timothée Feuillet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once


#include "data_holder.hpp"

namespace neam
{
  namespace enfield
  {
    namespace components
    {
      /// \brief Component that gives a name to the entity (you still can't query the entity from its name)
      /// \note You can make this component inherit from any concept provider (like serializable, editable, ...)
      ///       as long as it does not require specific arguments from the constructor.
      /// \note I may recomend you to use an alias of name, as you can then use that alias to perform queries
      template<typename DatabaseConf, template<typename X> class... ConceptProviders>
      using name = data_holder<DatabaseConf, std::string, ConceptProviders...>;
    } // namespace components
  } // namespace enfield
} // namespace neam


