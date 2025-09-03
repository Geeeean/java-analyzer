#include "method_parser.h"
#include "version.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Correct usage: ./analyzer <methodid>\n");
        return 1;
    }

    char* method_id = argv[1];

    if (is_info(method_id)) {
        print_info();
        return 0;
    }

    char* method_name = get_method_name(&method_id);
    if (method_id == NULL) {
        printf("Please insert a valid methoid: ./analyzer <methodid>\n");
        return 1;
    }

    char* params = get_method_params(&method_id);
    if (params == NULL) {
        printf("Please insert a valid methoid: ./analyzer <methodid>\n");
        return 1;
    }

    printf("%s\n%s\n%s\n", method_name, params, method_id);

    return 0;
}
