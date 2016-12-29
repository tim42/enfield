//
// file : enfield_reflective.hpp
// in : file:///home/tim/projects/enfield/enfield/enfield_reflective.hpp
//
// created by : Timothée Feuillet
// date: Sat Apr 23 2016 15:42:21 GMT+0200 (CEST)
//
//
// Copyright (c) 2016 Timothée Feuillet
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

#ifndef __N_16009239742501814934_182762242_ENFIELD_REFLECTIVE_HPP__
#define __N_16009239742501814934_182762242_ENFIELD_REFLECTIVE_HPP__

//
// Enfield will only monitor things with reflective if you included the reflective
// header in your project. To include reflective in your project but to keep out
// Enfield from using reflective, define ENFIELD_NO_REFLECTIVE_SUPPORT
//
// Enfield won't monitor everything, only errors and some important functions
//
//
// Do not use those macros outside enfield code.
//

#if defined(N_REFLECTIVE_PRESENT) && !defined(ENFIELD_NO_REFLECTIVE_SUPPORT)

#pragma message "building enfield with reflective support enabled"


// Use this when there's a function you want to monitor
#define NER_MONITOR_THIS_FUNCTION(f)    neam::r::function_call self_call(N_PRETTY_FUNCTION_INFO(f))

// Should only be used when nothing else is possible (like constructors / destructors)
// NOTE: slower than NER_MONITOR_THIS_FUNCTION(my_class::my_function)
#define NER_MONITOR_THIS_NAME(n)        neam::r::function_call self_call(N_PRETTY_NAME_INFO(n))
#define NER_MONITOR_THIS                neam::r::function_call self_call(N_PRETTY_NAME_INFO(__PRETTY_FUNCTION__))

#define _NER_GET_ACTIVE_FUNC            if (neam::r::function_call::get_active_function_call()) neam::r::function_call::get_active_function_call()

// Use this when you fail in a method/function that is monitored
#define NER_FAIL(rsn)                   self_call->fail(rsn)

// Use this when you fail in a method/function that isn't monitored
#define NER_IND_FAIL(rsn)                   _NER_GET_ACTIVE_FUNC->fail(rsn)

// Create a measure point
#define NER_MEASURE_POINT(mpn)          neam::r::measure_point _u ## __COUNTER__(mpn)

#else
#define NER_MONITOR_THIS_FUNCTION(f)    /* :) */
#define NER_MONITOR_THIS                /* :/ */
#define NER_MONITOR_THIS_NAME(n)        /* :/ */
#define NER_FAIL(rsn)                   /* :) */
#define NER_IND_FAIL(rsn)               /* :| */
#define NER_MEASURE_POINT(mpn)          /* :) */
#endif

#endif // __N_16009239742501814934_182762242_ENFIELD_REFLECTIVE_HPP__

