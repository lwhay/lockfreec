#include "internal.h"
#include <stdarg.h>

void internal_display_test_name(char *test_name) {
    assert(NULL != test_name);
    printf("%s...", test_name);
    fflush(stdout);
    return;
}

void internal_display_test_result(unsigned int number_name_dvs_pairs, ...) {
    va_list va;
    int passed_flag = RAISED;
    unsigned int loop;
    char *name;
    enum data_structure_validity dvs;

    va_start(va, number_name_dvs_pairs);
    for (loop = 0; loop < number_name_dvs_pairs; loop++) {
        name = va_arg(va, char*);
        dvs = va_arg(va, enum data_structure_validity);
        if (dvs != VALIDITY_VALID) {
            passed_flag = LOWERED;
            break;
        }
    }
    va_end(va);
    if (passed_flag == RAISED)
        puts("passed");
    if (passed_flag == LOWERED) {
        printf("failed (");
        va_start(va, number_name_dvs_pairs);
        for (loop = 0; loop < number_name_dvs_pairs; loop++) {
            name = va_arg(va, char*);
            dvs = va_arg(va, enum data_structure_validity);
            printf("%s ", name);
            internal_display_data_structure_validity(dvs);

            if (loop + 1 < number_name_dvs_pairs)
                printf(", ");
        }
        va_end(va);
        printf(")\n");
    }
    return;
}

void internal_display_data_structure_validity(enum data_structure_validity dvs) {
    char *strinfo = NULL;
    switch (dvs) {
    case VALIDITY_VALID:
        strinfo = "valid";
        break;
    case VALIDITY_INVALID_LOOP:
        strinfo = "invalid - loop detected";
        break;
    case VALIDITY_INVALID_MISSING_ELEMENTS:
        strinfo = "invalid - missing elements";
        break;
    case VALIDITY_INVALID_ADDITIONAL_ELEMENTS:
        strinfo = "invalid - additional elements";
        break;
    case VALIDITY_INVALID_TEST_DATA:
        strinfo = "invalid - invalid test data";
        break;
    }

    printf("%s", strinfo);
    return;
}
