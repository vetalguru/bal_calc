#include <ballistics/version.h>

#include <cstdio>

int main() {
    std::printf("ballistics %s\n", ballistics::version());
    return 0;
}
