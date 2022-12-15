
#include "include/c_api.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

std::vector<std::string> const &translate_vector_of_string(rust::Vec<rust::String> elements) {
    for (auto shared : elements) {
        std::cout << std::string(shared) << std::endl;
    }
    
    std::vector<std::string> *stdv = new std::vector<std::string>{};
    std::transform(elements.begin(), elements.end(), std::back_inserter(*stdv),
     [](rust::String elem) {
        return std::string(elem);
    });

    assert(elements.size() == stdv->size());
    return *stdv;
}
