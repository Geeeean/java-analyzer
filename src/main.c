#include "config.h"
#include "info.h"
#include "method.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Correct usage: ./analyzer <methodid>\n");
        return 1;
    }

    config* cfg = load_config();
    if (cfg == NULL) {
        printf("Config file is wrongly formatted\n");
        return 1;
    }

    print_config(cfg);

    char* method_id = argv[1];
    if (is_info(method_id)) {
        print_info(cfg);
        return 0;
    }

    method* m = create_method(method_id);
    if (m == NULL) {
        printf("methodid is not valid\n");
        return 1;
    }

    print_method(m);

    delete_method(m);
    delete_config(cfg);

    return 0;
}
