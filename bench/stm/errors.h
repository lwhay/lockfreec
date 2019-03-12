//
// Created by lwh on 19-3-12.
//

#ifndef ERRORS_H
#define ERRORS_H

#include <iostream>
#include <string>
#include <unistd.h>

#ifndef error
#define setbench_error(s) { \
    std::cout<<"ERROR: "<<s<<" (at "<<__FILE__<<"::"<<__FUNCTION__<<":"<<__LINE__<<")"<<std::endl; \
    exit(-1); \
}
#endif

#endif    /* ERRORS_H */
