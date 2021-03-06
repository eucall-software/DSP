/*
@copyright Louis Dionne 2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#include <boost/hana/equal.hpp>
#include <boost/hana/tail.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(
    hana::tail(hana::make_tuple(1, '2', 3.3, nullptr))
        ==
    hana::make_tuple('2', 3.3, nullptr)
, "");

int main() { }
