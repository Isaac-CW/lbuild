#ifndef LBUILD_ARGS
#define LBUILD_ARGS

namespace LBUILD {
    enum LBUILD_CMP {
        LBUILD_GCC,
        LBUILD_CLANG,
    };

    enum LBUILD_RESULT {
        LBUILD_STATUS_OK,
    };

    enum LBUILD_TYPE {
        LBUILD_O,
        LBUILD_BIN,
    } ;
}

#endif